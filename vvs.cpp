// obfusvb_interactive.cpp
// Multi-Language Rename Obfuscator — v3.3
// Dual symbol source: embedded resources extracted to <exeDir>\elements\ (default),
// or generate-from-system via dumpbin+PowerShell (dev machine only).
//
// v3.4: project mode injects ONE public decryptor class (mxcncbs) into a single
//       existing project file chosen by the user via a themed pick dialog --
//       no standalone decryptor file; other files reference it in-place.

#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0601
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <richedit.h>
#include <shlobj.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <random>
#include <regex>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <memory>
#include <functional>
#include <tuple>
#include <thread>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <wincrypt.h>
#include <gdiplus.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")

using namespace std;

const int MIN_NAME_LEN = 16;
const int MAX_NAME_LEN = 32;

static const wchar_t* kElemWin32Exports = L"win32_exports.txt";
static const wchar_t* kElemWin32Libs = L"win32_libs.txt";
static const wchar_t* kElemStlHeaders = L"stl_headers.txt";
static const wchar_t* kElemNetAssemblies = L"net_assemblies.txt";   // legacy, spec A1
static const wchar_t* kElemNetMembers = L"net_members.txt";         // authoritative, spec A1
static const wchar_t* kElemVbRuntime = L"vb_runtime.txt";           // spec A2
static const wchar_t* kElemManifest = L"manifest.json";

#define RES_WIN32_EXPORTS  1
#define RES_WIN32_LIBS     2
#define RES_STL_HEADERS    3
#define RES_NET_ASSEMBLIES 4
#define RES_MANIFEST       5
#define RES_NET_MEMBERS    6
#define RES_VB_RUNTIME     7

// ============================================================================
// THEME
// ============================================================================
const COLORREF CLR_BG = RGB(6, 30, 15);
const COLORREF CLR_BG_LO = RGB(2, 12, 6);
const COLORREF CLR_CARD = RGB(0, 0, 0);
const COLORREF CLR_CARD_TOP = RGB(8, 52, 26);
const COLORREF CLR_CARD_BOTTOM = RGB(3, 26, 13);
const COLORREF CLR_CARD_HI = RGB(4, 14, 6);
const COLORREF CLR_CTRL_BG = CLR_BG;
const COLORREF CLR_BORDER = RGB(0, 70, 25);
const COLORREF CLR_BORDER_HI = RGB(0, 255, 65);
const COLORREF CLR_TEXT = RGB(0, 255, 65);
const COLORREF CLR_TEXT_DIM = RGB(0, 130, 40);
const COLORREF CLR_TEXT_BRIGHT = RGB(170, 255, 170);
const COLORREF CLR_ACCENT = RGB(57, 255, 20);
const COLORREF CLR_ACCENT_DIM = RGB(0, 150, 50);
const COLORREF CLR_SUCCESS = RGB(57, 255, 20);
const COLORREF CLR_ERROR = RGB(255, 60, 60);
const COLORREF CLR_WARN = RGB(220, 255, 0);
const COLORREF CLR_INFO = RGB(0, 255, 180);
const COLORREF CLR_LOG_BG = CLR_CTRL_BG;
const COLORREF CLR_EDIT_BG = RGB(0, 58, 24);
const COLORREF CLR_EDIT_BG_OFF = RGB(0, 40, 17);
const COLORREF CLR_BTN_PRIMARY = RGB(0, 50, 18);
const COLORREF CLR_BTN_PRIMARY_H = RGB(0, 90, 32);
const COLORREF CLR_BTN_ACTION = RGB(0, 90, 32);
const COLORREF CLR_BTN_ACTION_H = RGB(57, 255, 20);
const COLORREF CLR_SEG_OFF = RGB(0, 20, 8);
const COLORREF CLR_SEG_ON = RGB(0, 170, 55);

// ============================================================================
// LANGUAGE
// ============================================================================
enum class Lang { VB, CSharp, Cpp };
static const wchar_t* langName(Lang l) {
    switch (l) {
    case Lang::VB: return L"VB.NET";
    case Lang::CSharp: return L"C#";
    case Lang::Cpp: return L"C++";
    }
    return L"";
}

// ============================================================================
// RESERVED SETS
// ============================================================================
static const set<wstring> VB_KEYWORDS = {
    L"AddHandler", L"AddressOf", L"Alias", L"And", L"AndAlso", L"As", L"Boolean",
    L"ByRef", L"Byte", L"ByVal", L"Call", L"Case", L"Catch", L"CBool", L"CByte",
    L"CChar", L"CDate", L"CDbl", L"CDec", L"Char", L"CInt", L"Class", L"CLng",
    L"CObj", L"Const", L"Continue", L"CSByte", L"CShort", L"CSng", L"CStr",
    L"CType", L"CUInt", L"CULng", L"CUShort", L"Date", L"Decimal", L"Declare",
    L"Default", L"Delegate", L"Dim", L"DirectCast", L"Do", L"Double", L"Each",
    L"Else", L"ElseIf", L"End", L"EndIf", L"Enum", L"Erase", L"Error", L"Event",
    L"Exit", L"False", L"Finally", L"For", L"Friend", L"Function", L"Get",
    L"GetType", L"GetXMLNamespace", L"Global", L"GoSub", L"GoTo", L"Handles",
    L"If", L"Implements", L"Imports", L"In", L"Inherits", L"Integer", L"Interface",
    L"Is", L"IsNot", L"Let", L"Lib", L"Like", L"Long", L"Loop", L"Me", L"Mod",
    L"Module", L"MustInherit", L"MustOverride", L"MyBase", L"MyClass", L"Namespace",
    L"Narrowing", L"New", L"Next", L"Not", L"Nothing", L"NotInheritable",
    L"NotOverridable", L"Object", L"Of", L"On", L"Operator", L"Option", L"Optional",
    L"Or", L"OrElse", L"Overloads", L"Overridable", L"Overrides", L"ParamArray",
    L"Partial", L"Private", L"Property", L"Protected", L"Public", L"RaiseEvent",
    L"ReadOnly", L"ReDim", L"RemoveHandler", L"Resume", L"Return", L"SByte", L"Select",
    L"Set", L"Shadows", L"Shared", L"Short", L"Single", L"Static", L"Step", L"Stop",
    L"String", L"Structure", L"Sub", L"SyncLock", L"Then", L"Throw", L"To", L"True",
    L"Try", L"TryCast", L"TypeOf", L"UInteger", L"ULong", L"UShort", L"Using",
    L"Variant", L"Wend", L"When", L"While", L"Widening", L"With", L"WithEvents",
    L"WriteOnly", L"Xor"
};

static const set<wstring> DOTNET_TYPES = {
    // .NET networking types (System.Net.Sockets / System.Net).
    L"TcpClient", L"TcpListener", L"UdpClient", L"Socket", L"NetworkStream",
    L"SocketAsyncEventArgs", L"AddressFamily", L"SocketType", L"ProtocolType",
    L"EndPoint", L"DnsEndPoint",
    // WPF controls, primitives, and enum types (System.Windows / Controls).
    L"ScrollViewer", L"DataGrid", L"DataGridRow", L"DataGridCell", L"DataGridColumn",
    L"DataGridTextColumn", L"DataGridTemplateColumn", L"DataGridCheckBoxColumn",
    L"Visibility", L"Orientation", L"ScrollBarVisibility", L"PanningMode",
    L"Grid", L"StackPanel", L"WrapPanel", L"DockPanel", L"Canvas", L"Border",
    L"TextBlock", L"PasswordBox", L"FlowDocument", L"Run", L"Inline", L"Paragraph",
    L"Hyperlink", L"ItemsControl", L"ItemsPresenter", L"ListBoxItem", L"ComboBoxItem",
    L"TreeViewItem", L"TabItem", L"Window", L"ContentControl", L"HeaderedContentControl",
    L"ButtonBase", L"ToggleButton", L"RepeatButton", L"Thumb", L"Slider", L"ProgressBar",
    L"VirtualizingStackPanel", L"UniformGrid", L"GridView", L"GridViewColumn",
    L"ControlTemplate", L"DataTemplate", L"Setter", L"Trigger", L"DependencyObject",
    L"FrameworkElement", L"UIElement", L"Visual", L"Visual3D", L"PresentationSource",
    L"HttpClient", L"HttpResponseMessage", L"ManagementObjectSearcher",
    L"ManagementObject", L"NetworkInterface", L"Screen", L"Registry",
    L"RegistryKey", L"Timer", L"Form", L"CreateParams", L"Point", L"MessageBox", L"MessageBoxButtons",
    L"MessageBoxIcon", L"DialogResult", L"Environment", L"Convert", L"Encoding",
    L"File", L"Directory", L"Path", L"StreamWriter", L"StreamReader", L"MemoryStream",
    L"CryptoStream", L"AesCryptoServiceProvider", L"HMACSHA256", L"ICryptoTransform",
    L"Task", L"JObject", L"JsonConvert", L"Process", L"Application",
    L"Win32_Processor", L"Win32_BIOS", L"Win32_BaseBoard", L"Win32_ComputerSystemProduct",
    L"Win32_SystemEnclosure", L"Win32_ComputerSystem", L"Win32_PhysicalMemory",
    L"SpecialFolder", L"Graphics", L"PaintEventArgs", L"Color", L"Font", L"Brush",
    L"Pen", L"SolidBrush", L"Rectangle", L"Size", L"Padding", L"DockStyle",
    L"AnchorStyles", L"ContentAlignment", L"FormStartPosition", L"BorderStyle",
    L"FlatStyle", L"ImageLayout", L"DateTime", L"TimeSpan", L"Guid", L"Random",
    L"BitConverter", L"StringBuilder", L"Regex", L"Match", L"MatchCollection",
    L"Group", L"Capture", L"Uri", L"WebClient", L"WebRequest", L"WebResponse",
    L"HttpWebRequest", L"HttpWebResponse", L"Cookie", L"CookieContainer",
    L"WebHeaderCollection", L"NameValueCollection", L"Thread", L"ThreadPool",
    L"CancellationToken", L"CancellationTokenSource", L"Parallel", L"Enumerable",
    L"List", L"Dictionary", L"HashSet", L"Queue", L"Stack", L"SortedList",
    L"SortedDictionary", L"ConcurrentDictionary", L"ConcurrentQueue",
    L"ConcurrentBag", L"ObservableCollection", L"BindingList", L"DataTable",
    L"DataRow", L"DataColumn", L"DataSet", L"DataView", L"BindingSource",
    L"BindingNavigator", L"ToolStrip", L"MenuStrip", L"StatusStrip",
    L"ContextMenuStrip", L"ToolStripMenuItem", L"ToolStripSeparator",
    L"OpenFileDialog", L"SaveFileDialog", L"FolderBrowserDialog", L"ColorDialog",
    L"FontDialog", L"PrintDialog", L"PrintDocument", L"PrintPreviewDialog",
    L"PageSetupDialog", L"BackgroundWorker", L"ProgressBar", L"TrackBar",
    L"NumericUpDown", L"DomainUpDown", L"DateTimePicker", L"MonthCalendar",
    L"PictureBox", L"Image", L"Bitmap", L"Icon", L"Cursor", L"Clipboard",
    L"DataObject", L"DragEventArgs", L"KeyEventArgs", L"MouseEventArgs",
    L"KeyPressEventArgs", L"PreviewKeyDownEventArgs", L"LayoutEventArgs",
    L"FormClosingEventArgs", L"FormClosedEventArgs", L"CancelEventArgs",
    L"EventHandler", L"Action", L"Func", L"Predicate", L"Comparison",
    L"Converter", L"Nullable", L"Tuple", L"ValueTuple", L"Lazy", L"WeakReference",
    L"GC", L"AppDomain", L"Assembly", L"Type", L"MethodInfo", L"PropertyInfo",
    L"FieldInfo", L"ConstructorInfo", L"ParameterInfo", L"MemberInfo",
    L"Attribute", L"FlagsAttribute", L"ObsoleteAttribute", L"SerializableAttribute",
    L"Debugger", L"Debug", L"Trace", L"TraceSource", L"TraceListener",
    L"PerformanceCounter", L"ProcessStartInfo", L"ProcessModule",
    L"FileVersionInfo", L"DriveInfo", L"FileInfo", L"DirectoryInfo",
    L"FileSystemInfo", L"PathTooLongException", L"FileNotFoundException",
    L"DirectoryNotFoundException", L"IOException", L"UnauthorizedAccessException",
    L"SecurityException", L"CryptographicException",
    L"CryptographicUnexpectedOperationException", L"HashAlgorithm", L"MD5",
    L"SHA1", L"SHA256", L"SHA384", L"SHA512", L"RSA", L"DSA", L"ECDSA",
    L"RandomNumberGenerator", L"Rijndael", L"DES", L"TripleDES", L"RC2",
    L"ProtectedData", L"ProtectedMemory", L"SecureString", L"WindowsIdentity",
    L"WindowsPrincipal", L"GenericIdentity", L"GenericPrincipal", L"IIdentity",
    L"IPrincipal", L"ClaimsPrincipal", L"ClaimsIdentity", L"Claim",
    L"Authentication", L"Authorization", L"PrincipalPermission",
    L"SecurityPermission", L"FileIOPermission", L"RegistryPermission",
    L"EnvironmentPermission", L"UIPermission", L"ReflectionPermission",
    L"SocketPermission", L"WebPermission", L"DnsPermission",
    L"SqlClientPermission", L"OleDbPermission", L"OdbcPermission",
    L"OraclePermission", L"Control", L"UserControl", L"ContainerControl",
    L"ScrollableControl", L"Panel", L"FlowLayoutPanel", L"TableLayoutPanel",
    L"SplitContainer", L"SplitterPanel", L"TabControl", L"TabPage", L"GroupBox",
    L"Label", L"Button", L"TextBox", L"ComboBox", L"ListBox", L"CheckedListBox",
    L"ListView", L"TreeView", L"DataGridView", L"DataGridViewCell",
    L"DataGridViewRow", L"DataGridViewColumn", L"CheckBox", L"RadioButton",
    L"ToolTip", L"ErrorProvider", L"HelpProvider", L"ImageList", L"NotifyIcon",
    L"ContextMenu", L"MainMenu", L"MenuItem", L"StatusBar", L"ToolBar",
    L"HScrollBar", L"VScrollBar", L"RichTextBox", L"MaskedTextBox",
    L"LinkLabel", L"Forms", L"MouseButtons", L"Keys", L"EventArgs",
    L"Windows", L"System", L"Drawing", L"Text", L"IO",
    L"Net", L"Security", L"Reflection", L"Runtime",
    L"InteropServices", L"CompilerServices", L"Serialization",
    L"Xml", L"Data", L"Diagnostics", L"Globalization",
    L"Resources", L"Configuration", L"Collections", L"Generic",
    L"Threading", L"Tasks", L"Linq",
    L"IEnumerable", L"IEnumerator", L"IDisposable", L"IComparable",
    L"IEquatable", L"IList", L"ICollection", L"IDictionary",
    L"Component", L"Container", L"Marshal", L"GCHandle",
    L"IntPtr", L"UIntPtr", L"Exception", L"ArgumentException",
    L"InvalidOperationException", L"NullReferenceException",
    L"ArgumentNullException", L"ArgumentOutOfRangeException",
    L"FormatException", L"OverflowException", L"IndexOutOfRangeException",
    L"KeyNotFoundException", L"NotSupportedException", L"NotImplementedException",
    L"ObjectDisposedException", L"StackOverflowException", L"OutOfMemoryException",
    L"DivideByZeroException", L"ArithmeticException", L"SystemException",
    L"AccessViolationException", L"AppDomainUnloadedException",
    L"BadImageFormatException", L"CannotUnloadAppDomainException",
    L"ContextMarshalException", L"DataMisalignedException",
    L"ExecutionEngineException", L"InsufficientExecutionStackException",
    L"InsufficientMemoryException", L"InvalidCastException",
    L"InvalidProgramException", L"MemberAccessException",
    L"MissingFieldException", L"MissingMemberException",
    L"MissingMethodException", L"MulticastNotSupportedException",
    L"NotFiniteNumberException", L"OperationCanceledException",
    L"PlatformNotSupportedException", L"RankException",
    L"ReflectionTypeLoadException", L"TargetException",
    L"TargetInvocationException", L"TargetParameterCountException",
    L"TimeoutException", L"TypeAccessException", L"TypeInitializationException",
    L"TypeLoadException", L"TypeUnloadedException",
    L"UriFormatException", L"WaitHandleCannotBeOpenedException",
    L"AbandonedMutexException", L"SemaphoreFullException",
    L"SynchronizationLockException", L"ThreadAbortException",
    L"ThreadInterruptedException", L"ThreadStartException",
    L"ThreadStateException", L"BarrierPostPhaseException",
    L"LockRecursionException", L"HostProtectionException",
    L"PolicyException", L"VerificationException",
    L"XmlException", L"XmlSchemaException", L"XmlSyntaxException",
    L"XPathException", L"XsltException", L"XsltCompileException",
    L"ConstraintException", L"DataException", L"DBConcurrencyException",
    L"DeletedRowInaccessibleException", L"DuplicateNameException",
    L"EvaluateException", L"InvalidConstraintException",
    L"InvalidExpressionException", L"MissingPrimaryKeyException",
    L"NoNullAllowedException", L"ReadOnlyException",
    L"RowNotInTableException", L"StrongTypingException",
    L"SyntaxErrorException", L"TypedDataSetGeneratorException",
    L"VersionNotFoundException", L"SqlException", L"SqlAlreadyFilledException",
    L"SqlNotFilledException", L"SqlTypeException", L"OleDbException",
    L"OdbcException", L"OracleException", L"OracleNullValueException",
    L"OracleTruncateException", L"OracleTypeException", L"OracleWarningException",
    L"SocketException", L"NetworkInformationException",
    L"PingException", L"ProtocolViolationException", L"WebException",
    L"HttpListenerException", L"SmtpException", L"SmtpFailedRecipientException",
    L"SmtpFailedRecipientsException",
    L"FtpWebRequest", L"FtpWebResponse", L"HttpListener", L"HttpListenerContext",
    L"HttpListenerRequest", L"HttpListenerResponse", L"HttpRequest", L"HttpResponse",
    L"MailMessage", L"MailAddress", L"Attachment", L"SmtpClient",
    L"NetworkCredential", L"CredentialCache", L"ServicePoint", L"ServicePointManager",
    L"IPHostEntry", L"IPAddress", L"IPEndPoint", L"Dns", L"Ping", L"PingReply",
    L"PingOptions", L"PhysicalAddress", L"UnicastIPAddressInformation",
    L"MulticastIPAddressInformation", L"IPAddressInformation", L"IPInterfaceProperties",
    L"IPv4InterfaceProperties", L"IPv6InterfaceProperties", L"TcpConnectionInformation",
    L"TcpStatistics", L"UdpStatistics", L"IPGlobalProperties", L"IPGlobalStatistics",
    L"IcmpV4Statistics", L"IcmpV6Statistics",
    L"UdpConnectionInformation", L"ComponentModel", L"Design",
    L"TypeDescriptor", L"PropertyDescriptor", L"EventDescriptor", L"MemberDescriptor",
    L"TypeConverterAttribute", L"TypeDescriptionProviderAttribute",
    L"PropertyTabAttribute", L"DesignerAttribute", L"DesignerCategoryAttribute",
    L"DefaultEventAttribute", L"DefaultPropertyAttribute", L"DescriptionAttribute",
    L"CategoryAttribute", L"BrowsableAttribute", L"BindableAttribute",
    L"ReadOnlyAttribute", L"LocalizableAttribute", L"AmbientValueAttribute",
    L"DefaultValueAttribute", L"DesignerSerializationVisibilityAttribute",
    L"EditorAttribute", L"EditorBrowsableAttribute", L"NotifyParentPropertyAttribute",
    L"ParenthesizePropertyNameAttribute", L"PasswordPropertyTextAttribute",
    L"RefreshPropertiesAttribute", L"RecommendedAsConfigurableAttribute",
    L"ImmutableObjectAttribute", L"MergablePropertyAttribute",
    L"DefaultBindingPropertyAttribute", L"ComplexBindingPropertiesAttribute",
    L"LookupBindingPropertiesAttribute", L"AttributeProviderAttribute",
    L"SettingsBindableAttribute", L"TypeConverter", L"Editor", L"Designer",
    L"ToolboxItem", L"ToolboxItemFilterAttribute", L"Install",
    L"RunInstaller", L"ServiceInstaller", L"EventLogInstaller",
    L"PerformanceCounterInstaller", L"LicenseProvider", L"ComponentEditor",
    L"UITypeEditor", L"ListBindableAttribute", L"ListChangedEventArgs",
    L"AddingNewEventArgs", L"PropertyChangedEventArgs", L"HandledEventArgs",
    L"InvalidateEventArgs", L"DrawItemEventArgs", L"MeasureItemEventArgs",
    L"SearchForVirtualItemEventArgs", L"ColumnClickEventArgs", L"ItemCheckEventArgs",
    L"ItemCheckedEventArgs", L"ItemDragEventArgs", L"NodeLabelEditEventArgs",
    L"TreeViewCancelEventArgs", L"TreeViewEventArgs",
    L"WebBrowserNavigatingEventArgs", L"WebBrowserNavigatedEventArgs",
    L"WebBrowserDocumentCompletedEventArgs", L"WebBrowserDocumentTitleChangedEventArgs",
    L"WebBrowserProgressChangedEventArgs", L"HtmlElementEventArgs",
    L"HtmlElementErrorEventArgs", L"HtmlElementCollection",
    L"HtmlWindowCollection", L"HtmlHistory", L"HtmlElement",
    L"HtmlWindow", L"HtmlDocument", L"HtmlAttributeCollection",
    L"HtmlAttribute", L"HtmlElementEventHandler", L"HtmlElementErrorEventHandler",
    L"My", L"Settings", L"Computer", L"Application", L"User", L"Resources",
    L"WebServices", L"Forms",
    L"StructLayout", L"LayoutKind", L"FieldOffset", L"MarshalAs", L"UnmanagedType",
    L"EditorBrowsable", L"EditorBrowsableState", L"Browsable", L"Bindable",
    L"DefaultValue", L"AmbientValue", L"DesignerSerializationVisibility",
    L"TypeDescriptionProvider", L"LicenseProvider", L"Designer", L"Editor",
    L"ToolboxItem", L"ToolboxBitmap", L"RunInstaller", L"ServiceInstaller",
    L"EventLogInstaller", L"DataContract", L"DataMember", L"CollectionDataContract",
    L"EnumMember", L"OnSerializing", L"OnSerialized", L"OnDeserializing",
    L"OnDeserialized", L"OnDeserialization", L"XmlRoot", L"XmlElement",
    L"XmlAttribute", L"XmlArray", L"XmlIgnore", L"JsonConverter", L"JsonIgnore",
    L"JsonSerializer", L"JsonSerializerOptions", L"JsonProperty", L"JsonPropertyName",
    L"ProtoContract", L"ProtoMember", L"ProtoInclude", L"CallerMemberName",
    L"CallerFilePath", L"CallerLineNumber", L"CallerArgumentExpression",
    L"GeneratedCode", L"CompilerGenerated", L"Conditional",
    L"DependencyProperty", L"RoutedEvent", L"RoutedEventArgs", L"Unsafe",
    L"MemoryMarshal", L"Span", L"ReadOnlySpan", L"Memory", L"ReadOnlyMemory",
    L"ValueTask", L"IAsyncEnumerable", L"IAsyncEnumerator", L"IAsyncDisposable",
    L"Channel", L"ChannelReader", L"ChannelWriter"
};

static const set<wstring> CSHARP_KEYWORDS = {
    L"abstract", L"as", L"base", L"bool", L"break", L"byte", L"case", L"catch",
    L"char", L"checked", L"class", L"const", L"continue", L"decimal", L"default",
    L"delegate", L"do", L"double", L"else", L"enum", L"event", L"explicit",
    L"extern", L"false", L"finally", L"fixed", L"float", L"for", L"foreach",
    L"goto", L"if", L"implicit", L"in", L"int", L"interface", L"internal",
    L"is", L"lock", L"long", L"namespace", L"new", L"null", L"object",
    L"operator", L"out", L"override", L"params", L"private", L"protected",
    L"public", L"readonly", L"ref", L"return", L"sbyte", L"sealed", L"short",
    L"sizeof", L"stackalloc", L"static", L"string", L"struct", L"switch",
    L"this", L"throw", L"true", L"try", L"typeof", L"uint", L"ulong",
    L"unchecked", L"unsafe", L"ushort", L"using", L"virtual", L"void",
    L"volatile", L"while", L"var", L"dynamic", L"async", L"await", L"yield",
    L"partial", L"get", L"set", L"value", L"nameof", L"where", L"select",
    L"from", L"group", L"into", L"orderby", L"join", L"let", L"on", L"equals",
    L"ascending", L"descending", L"global", L"add", L"remove", L"alias",
    L"when", L"with", L"record", L"init", L"required", L"nint", L"nuint",
    L"Main"
};

static const set<wstring> CPP_KEYWORDS = {
    L"alignas", L"alignof", L"and", L"and_eq", L"asm", L"auto", L"bitand",
    L"bitor", L"bool", L"break", L"case", L"catch", L"char", L"char8_t",
    L"char16_t", L"char32_t", L"class", L"compl", L"concept", L"const",
    L"consteval", L"constexpr", L"constinit", L"const_cast", L"continue",
    L"co_await", L"co_return", L"co_yield", L"decltype", L"default",
    L"delete", L"do", L"double", L"dynamic_cast", L"else", L"enum",
    L"explicit", L"export", L"extern", L"false", L"float", L"for",
    L"friend", L"goto", L"if", L"inline", L"int", L"long", L"mutable",
    L"namespace", L"new", L"noexcept", L"not", L"not_eq", L"nullptr",
    L"operator", L"or", L"or_eq", L"private", L"protected", L"public",
    L"register", L"reinterpret_cast", L"requires", L"return", L"short",
    L"signed", L"sizeof", L"static", L"static_assert", L"static_cast",
    L"struct", L"switch", L"template", L"this", L"thread_local", L"throw",
    L"true", L"try", L"typedef", L"typeid", L"typename", L"union",
    L"unsigned", L"using", L"virtual", L"void", L"volatile", L"wchar_t",
    L"while", L"xor", L"xor_eq", L"override", L"final", L"main", L"WinMain",
    L"wWinMain", L"DllMain"
};

static const set<wstring> CPP_PRAGMA_INTRODUCER_TOKENS = {
    L"__pragma", L"_Pragma", L"__declspec", L"__uuidof", L"__attribute_",
    L"__attribute", L"alignas", L"__based", L"__unaligned", L"__ptr32",
    L"__ptr64", L"__sptr", L"__uptr"
};

static const set<wstring> CPP_MSVC_PRAGMA_WORDS = {
    L"alloc_text", L"auto_inline", L"bss_seg", L"check_stack", L"code_seg",
    L"comment", L"component", L"conform", L"const_seg", L"data_seg",
    L"deprecated", L"detect_mismatch", L"execution_character_set",
    L"fenv_access", L"float_control", L"fp_contract", L"function",
    L"function_level_linking", L"init_seg", L"inline_depth",
    L"inline_recursion", L"intrinsic", L"loop", L"make_public", L"managed",
    L"message", L"oem_define", L"optimize", L"pack", L"pointers_to_members",
    L"pop_macro", L"prefast", L"push_macro", L"region", L"endregion",
    L"runtime_checks", L"same_name", L"section", L"setlocale",
    L"strict_gs_check", L"system_header", L"unmanaged", L"vtordisp",
    L"warning"
};

static const set<wstring> CPP_MSVC_DECLSPEC_WORDS = {
    L"align", L"allocate", L"allocator", L"appdomain", L"code_seg",
    L"deprecated", L"dllexport", L"dllimport", L"empty_bases",
    L"jitintrinsic", L"naked", L"noalias", L"noinline", L"noreturn",
    L"nothrow", L"novtable", L"no_sanitize_address", L"process",
    L"property", L"restrict", L"safebuffers", L"selectany", L"spectre",
    L"thread", L"uuid"
};

static const set<wstring> CPP_GCC_ATTRIBUTE_WORDS_COMMON = {
    L"aligned", L"always_inline", L"cleanup", L"cold", L"constructor",
    L"deprecated", L"destructor", L"error", L"fallthrough", L"format",
    L"gnu_inline", L"hot", L"malloc", L"may_alias", L"noinline", L"nonnull",
    L"noreturn", L"nothrow", L"packed", L"pure", L"section", L"sentinel",
    L"unused", L"used", L"visibility", L"warn_unused_result", L"weak"
};

static const set<wstring> CPP_ATTRIBUTE_LITERAL_WORDS = {
    L"property", L"get", L"put", L"warning", L"disable", L"push", L"pop",
    L"message", L"section", L"cleanup", L"format", L"printf", L"alias",
    L"align", L"uuid", L"deprecated", L"noinline", L"selectany", L"thread",
    L"dllexport", L"dllimport", L"naked", L"restrict", L"safebuffers",
    L"spectre", L"novtable", L"jitintrinsic", L"noalias", L"allocate",
    L"allocator", L"appdomain", L"code_seg", L"empty_bases", L"process",
    L"noreturn", L"nothrow", L"no_sanitize_address"
};

static const set<wstring> CPP_UNDERSCORE_PREFIXES = {
    L"_In_", L"_Out_", L"_Inout_", L"_In_opt_", L"_Out_opt_", L"_Inout_opt_",
    L"_In_z_", L"_Out_z_", L"_Inout_z_", L"_In_reads_", L"_Out_writes_",
    L"_mm_", L"_Interlocked", L"_ReturnAddress", L"_AddressOfReturnAddress",
    L"_ReadWriteBarrier", L"_ReadBarrier", L"_WriteBarrier",
    L"_com_ptr_t", L"_com_error", L"_bstr_t", L"_variant_t", L"_w64",
    L"_stdcall", L"_cdecl", L"_fastcall", L"_thiscall", L"_vectorcall",
    L"_Ptr_", L"_Pre_", L"_Post_", L"_Field_", L"_Reserved_",
    L"_Check_return_", L"_Success_", L"_When_", L"_At_", L"_Always_",
    L"_Use_decl_annotations_", L"_CRTIMP", L"_CRT_INSECURE"
};

static const set<wstring> CPP_STD_TYPES = {
    L"std", L"string", L"wstring", L"vector", L"map", L"unordered_map",
    L"set", L"unordered_set", L"pair", L"tuple", L"list", L"deque", L"queue",
    L"stack", L"array", L"shared_ptr", L"unique_ptr", L"weak_ptr", L"function",
    L"optional", L"variant", L"any", L"cout", L"cin", L"cerr", L"wcout",
    L"wcin", L"wcerr", L"endl", L"ostream", L"istream", L"stringstream",
    L"wstringstream", L"ostringstream", L"istringstream", L"size_t",
    L"ptrdiff_t", L"int8_t", L"int16_t", L"int32_t", L"int64_t", L"uint8_t",
    L"uint16_t", L"uint32_t", L"uint64_t", L"FILE", L"NULL", L"DWORD",
    L"WORD", L"BYTE", L"LPCWSTR", L"LPWSTR", L"LPCSTR", L"LPSTR", L"HANDLE",
    L"HWND", L"HINSTANCE", L"HDC", L"HBRUSH", L"HPEN", L"HFONT", L"HICON",
    L"HCURSOR", L"HMENU", L"HMODULE", L"WPARAM", L"LPARAM", L"LRESULT",
    L"BOOL", L"TRUE", L"FALSE", L"UINT", L"ULONG", L"LONG", L"COLORREF",
    L"RECT", L"POINT", L"SIZE", L"MSG", L"WNDCLASSEX", L"PAINTSTRUCT",
    L"CALLBACK", L"WINAPI", L"APIENTRY", L"VOID", L"CHAR", L"WCHAR",
    L"INT", L"UINT8", L"UINT16", L"UINT32", L"UINT64", L"INT8", L"INT16",
    L"INT32", L"INT64", L"nullptr_t",
    L"ECMAScript", L"basic", L"extended", L"awk", L"grep", L"egrep",
    L"icase", L"nosubs", L"optimize", L"collate",
    L"match_default", L"match_not_bol", L"match_not_eol",
    L"match_not_bow", L"match_not_eow", L"match_any",
    L"match_not_null", L"match_continuous", L"match_prev_avail",
    L"format_default", L"format_sed", L"format_no_copy",
    L"format_first_only",
    L"error_collate", L"error_ctype", L"error_escape",
    L"error_backref", L"error_brack", L"error_paren", L"error_brace",
    L"error_badbrace", L"error_range", L"error_space",
    L"error_badrepeat", L"error_complexity", L"error_stack",
    L"Ok", L"GenericError", L"InvalidParameter", L"OutOfMemory",
    L"ObjectBusy", L"InsufficientBuffer", L"NotImplemented",
    L"Win32Error", L"WrongState", L"Aborted", L"FileNotFound",
    L"ValueOverflow", L"AccessDenied", L"UnknownImageFormat",
    L"FontFamilyNotFound", L"FontStyleNotFound", L"NotTrueTypeFont",
    L"UnsupportedGdiplusVersion", L"GdiplusNotInitialized",
    L"PropertyNotFound", L"PropertyNotSupported", L"ProfileNotFound",
    L"SmoothingModeInvalid", L"SmoothingModeDefault",
    L"SmoothingModeHighSpeed", L"SmoothingModeHighQuality",
    L"SmoothingModeNone", L"SmoothingModeAntiAlias",
    L"SmoothingModeAntiAlias8x4", L"SmoothingModeAntiAlias8x8",
    L"TextRenderingHintSystemDefault",
    L"TextRenderingHintSingleBitPerPixelGridFit",
    L"TextRenderingHintSingleBitPerPixel",
    L"TextRenderingHintAntiAliasGridFit", L"TextRenderingHintAntiAlias",
    L"TextRenderingHintClearTypeGridFit",
    L"InterpolationModeInvalid", L"InterpolationModeDefault",
    L"InterpolationModeLowQuality", L"InterpolationModeHighQuality",
    L"InterpolationModeBilinear", L"InterpolationModeBicubic",
    L"InterpolationModeNearestNeighbor",
    L"InterpolationModeHighQualityBilinear",
    L"InterpolationModeHighQualityBicubic",
    L"PixelOffsetModeInvalid", L"PixelOffsetModeDefault",
    L"PixelOffsetModeHighSpeed", L"PixelOffsetModeHighQuality",
    L"PixelOffsetModeNone", L"PixelOffsetModeHalf",
    L"CompositingModeSourceOver", L"CompositingModeSourceCopy",
    L"CompositingQualityInvalid", L"CompositingQualityDefault",
    L"CompositingQualityHighSpeed", L"CompositingQualityHighQuality",
    L"CompositingQualityGammaCorrected",
    L"CompositingQualityAssumeLinear",
    L"LineCapFlat", L"LineCapSquare", L"LineCapRound",
    L"LineCapTriangle", L"LineCapNoAnchor", L"LineCapSquareAnchor",
    L"LineCapRoundAnchor", L"LineCapDiamondAnchor",
    L"LineCapArrowAnchor", L"LineCapCustom",
    L"LineJoinMiter", L"LineJoinBevel", L"LineJoinRound",
    L"LineJoinMiterClipped",
    L"DashStyleSolid", L"DashStyleDash", L"DashStyleDot",
    L"DashStyleDashDot", L"DashStyleDashDotDot", L"DashStyleCustom",
    L"DashCapFlat", L"DashCapRound", L"DashCapTriangle",
    L"FontStyleRegular", L"FontStyleBold", L"FontStyleItalic",
    L"FontStyleBoldItalic", L"FontStyleUnderline",
    L"FontStyleStrikeout",
    L"UnitWorld", L"UnitDisplay", L"UnitPixel", L"UnitPoint",
    L"UnitInch", L"UnitDocument", L"UnitMillimeter",
    L"StringAlignmentNear", L"StringAlignmentCenter",
    L"StringAlignmentFar",
    L"StringDigitSubstituteUser", L"StringDigitSubstituteNone",
    L"StringDigitSubstituteNational", L"StringDigitSubstituteTraditional",
    L"HotkeyPrefixNone", L"HotkeyPrefixShow", L"HotkeyPrefixHide",
    L"StringTrimmingNone", L"StringTrimmingCharacter",
    L"StringTrimmingWord", L"StringTrimmingEllipsisCharacter",
    L"StringTrimmingEllipsisWord", L"StringTrimmingEllipsisPath",
    L"StringFormatFlagsDirectionRightToLeft",
    L"StringFormatFlagsDirectionVertical",
    L"StringFormatFlagsNoFitBlackBox",
    L"StringFormatFlagsDisplayFormatControl",
    L"StringFormatFlagsNoFontFallback",
    L"StringFormatFlagsMeasureTrailingSpaces",
    L"StringFormatFlagsNoWrap", L"StringFormatFlagsLineLimit",
    L"StringFormatFlagsNoClip", L"StringFormatFlagsBypassGDI",
    L"FillModeAlternate", L"FillModeWinding",
    L"MatrixOrderPrepend", L"MatrixOrderAppend",
    L"WrapModeTile", L"WrapModeTileFlipX", L"WrapModeTileFlipY",
    L"WrapModeTileFlipXY", L"WrapModeClamp",
    L"ImageLockModeRead", L"ImageLockModeWrite",
    L"ImageLockModeUserInputBuf",
    L"LinearGradientModeHorizontal", L"LinearGradientModeVertical",
    L"LinearGradientModeForwardDiagonal",
    L"LinearGradientModeBackwardDiagonal",
    L"PixelFormat32bppPARGB", L"PixelFormat32bppARGB",
    L"PixelFormat32bppRGB", L"PixelFormat24bppRGB",
    L"PixelFormat8bppIndexed"
};

static const set<wstring> CPP_RESERVED_MEMBERS = {
    L"detach", L"join", L"joinable", L"get_id", L"native_handle",
    L"lock", L"unlock", L"try_lock", L"notify_one", L"notify_all", L"wait",
    L"wait_for", L"wait_until", L"owns_lock", L"get_future", L"set_value",
    L"set_exception", L"set_value_at_thread_exit", L"set_exception_at_thread_exit",
    L"valid", L"share",
    L"now", L"time_since_epoch", L"count", L"duration_cast",
    L"seconds", L"milliseconds", L"minutes", L"hours",
    L"nanoseconds", L"microseconds",
    L"steady_clock", L"system_clock", L"high_resolution_clock",
    L"binary", L"trunc", L"app", L"ate", L"in", L"out",
    L"open", L"close", L"is_open", L"good", L"bad", L"fail", L"eof",
    L"clear", L"seekg", L"seekp", L"tellg", L"tellp",
    L"read", L"write", L"get", L"put", L"getline", L"ignore",
    L"peek", L"unget", L"flush", L"sync",
    L"c_str", L"append", L"assign", L"substr", L"compare", L"resize",
    L"reserve", L"shrink_to_fit", L"copy", L"starts_with", L"ends_with",
    L"find_first_of", L"find_last_of", L"find_first_not_of",
    L"find_last_not_of", L"rfind", L"replace", L"to_string", L"to_wstring",
    L"push_back", L"pop_back", L"push_front", L"pop_front",
    L"emplace", L"emplace_back", L"emplace_front",
    L"try_emplace", L"insert_or_assign", L"erase", L"insert",
    L"capacity", L"begin", L"end", L"cbegin", L"cend",
    L"rbegin", L"rend", L"crbegin", L"crend",
    L"size", L"empty", L"front", L"back", L"at", L"data", L"find",
    L"contains", L"swap", L"merge", L"extract", L"keys", L"values",
    L"lower_bound", L"upper_bound", L"equal_range", L"key_comp", L"value_comp",
    L"reset", L"release", L"use_count", L"unique",
    L"owner_before", L"expired", L"make_shared", L"make_unique",
    L"has_value", L"value", L"value_or", L"index",
    L"holds_alternative", L"get_if", L"any_cast",
    L"target", L"target_type",
    L"path", L"filename", L"stem", L"extension", L"parent_path",
    L"root_name", L"root_directory", L"root_path", L"relative_path",
    L"is_absolute", L"is_relative", L"lexically_normal", L"lexically_relative",
    L"lexically_proximate", L"u8string", L"u16string", L"u32string",
    L"generic_string", L"generic_wstring", L"generic_u8string",
    L"replace_filename", L"replace_extension", L"remove_filename",
    L"has_filename", L"has_stem", L"has_extension", L"has_parent_path",
    L"has_root_path", L"has_root_name", L"has_root_directory", L"has_relative_path",
    L"is_regular_file", L"is_directory", L"is_symlink", L"is_block_file",
    L"is_character_file", L"is_fifo", L"is_socket", L"is_other", L"exists",
    L"file_size", L"hard_link_count", L"last_write_time", L"permissions",
    L"status", L"symlink_status", L"canonical", L"weakly_canonical",
    L"absolute", L"relative", L"proximate", L"copy_file", L"copy_symlink",
    L"create_directory", L"create_directories", L"create_hard_link",
    L"create_symlink", L"create_directory_symlink", L"current_path",
    L"remove", L"remove_all", L"rename", L"resize_file", L"space",
    L"temp_directory_path", L"directory_iterator", L"recursive_directory_iterator",
    L"increment", L"decrement",
    L"Internal", L"InternalHigh", L"Offset", L"OffsetHigh", L"hEvent",
    L"NextEntryOffset", L"Action", L"FileNameLength", L"FileName",
    L"dwFileAttributes", L"ftCreationTime", L"ftLastAccessTime",
    L"ftLastWriteTime", L"nFileSizeHigh", L"nFileSizeLow",
    L"dwReserved0", L"dwReserved1", L"cFileName", L"cAlternateFileName",
    L"style", L"lpfnWndProc", L"cbClsExtra", L"cbWndExtra",
    L"hInstance", L"hIcon", L"hCursor", L"hbrBackground", L"lpszMenuName",
    L"lpszClassName", L"hIconSm", L"cbSize",
    L"hwnd", L"message", L"wParam", L"lParam", L"time", L"pt",
    L"hdc", L"fErase", L"rcPaint", L"fRestore", L"fIncUpdate", L"rgbReserved",
    L"left", L"top", L"right", L"bottom", L"x", L"y", L"cx", L"cy",
    L"cb", L"lpReserved", L"lpDesktop", L"lpTitle", L"dwX", L"dwY",
    L"dwXSize", L"dwYSize", L"dwXCountChars", L"dwYCountChars",
    L"dwFillAttribute", L"dwFlags", L"wShowWindow", L"cbReserved2",
    L"lpReserved2", L"hStdInput", L"hStdOutput", L"hStdError",
    L"hProcess", L"hThread", L"dwProcessId", L"dwThreadId",
    L"fMask", L"lpVerb", L"lpFile", L"lpParameters", L"lpDirectory",
    L"nShow", L"hInstApp", L"lpIDList", L"lpClass", L"hkeyClass",
    L"dwHotKey", L"hMonitor",
    L"nLength", L"lpSecurityDescriptor", L"bInheritHandle",
    L"Revision", L"Sbz1", L"Control", L"Owner", L"Group", L"Sacl", L"Dacl",
    L"PrivilegeCount", L"Privileges", L"Luid", L"Attributes",
    L"dwServiceType", L"dwCurrentState", L"dwControlsAccepted",
    L"dwWin32ExitCode", L"dwServiceSpecificExitCode", L"dwCheckPoint",
    L"dwWaitHint", L"dwServiceFlags",
    L"Start", L"Stop", L"GetDefaultDomain", L"CreateLogicalThreadState",
    L"DeleteLogicalThreadState", L"SwitchInLogicalThreadState",
    L"SwitchOutLogicalThreadState", L"LocksHeldByLogicalThread",
    L"MapFile", L"GetConfiguration", L"GetRuntime", L"GetInterface",
    L"IsLoaded", L"IsStarted", L"GetVersionString", L"BindAsLegacyV2Runtime",
    L"EnumerateInstalledRuntimes", L"EnumerateLoadedRuntimes",
    L"RequestRuntimeLoadedNotification",
    L"ConnectServer", L"ExecQuery", L"ExecQueryAsync",
    L"ExecNotificationQuery", L"ExecNotificationQueryAsync",
    L"ExecMethod", L"ExecMethodAsync", L"GetObject", L"DeleteInstance",
    L"PutInstance", L"AddRef", L"Release", L"QueryInterface",
    L"Next", L"Skip", L"Reset", L"Clone", L"BeginEnumeration",
    L"EndEnumeration", L"GetNames", L"GetPropertyQualifierSet",
    L"GetMethod", L"GetMethodQualifierSet", L"GetMethodOrigin",
    L"GetQualifierSet", L"PutMethod", L"Delete", L"DeleteMethod",
    L"GetObjectText", L"SpawnDerivedClass", L"SpawnInstance", L"CompareTo",
    L"GetPropertyOrigin", L"InheritsFrom", L"GetClass", L"GetInstances",
    L"GetInstancesAsync", L"GetReferences", L"GetReferencesAsync",
    L"GetFolder", L"GetTask", L"get_Xml", L"put_Xml",
    L"get_Name", L"put_Name", L"get_Path", L"put_Path",
    L"get_State", L"get_Enabled", L"put_Enabled",
    L"get_ApplicationName", L"put_ApplicationName",
    L"get_Action", L"put_Action", L"get_Direction", L"put_Direction",
    L"get_Profiles", L"put_Profiles", L"get_Rules",
    L"Add", L"Remove", L"Item", L"get__NewEnum", L"get_Count",
    L"load", L"store", L"exchange", L"compare_exchange_weak",
    L"compare_exchange_strong", L"fetch_add", L"fetch_sub",
    L"fetch_and", L"fetch_or", L"fetch_xor", L"is_lock_free",
    L"name", L"combine", L"global", L"classic",
    L"setw", L"setfill", L"setprecision", L"setbase", L"setiosflags",
    L"resetiosflags", L"fixed", L"scientific", L"hex", L"dec", L"oct",
    L"internal", L"showbase", L"noshowbase", L"showpoint", L"noshowpoint",
    L"showpos", L"noshowpos", L"uppercase", L"nouppercase",
    L"boolalpha", L"noboolalpha", L"unitbuf", L"nounitbuf",
    L"skipws", L"noskipws",
    L"S_OK", L"S_FALSE", L"E_FAIL", L"E_NOINTERFACE", L"E_POINTER",
    L"E_INVALIDARG", L"E_OUTOFMEMORY", L"E_UNEXPECTED", L"E_NOTIMPL",
    L"WBEM_S_NO_ERROR", L"WBEM_S_FALSE", L"WBEM_S_TIMEDOUT",
    L"WBEM_S_NO_MORE_DATA", L"WBEM_E_FAILED", L"WBEM_E_NOT_FOUND",
    L"first", L"second", L"value_type",
    L"size_type", L"difference_type", L"reference",
    L"pointer", L"iterator", L"const_iterator",
    L"make_pair", L"make_tuple", L"tie", L"forward", L"move",
    L"forward_as_tuple", L"tuple_cat", L"apply",
    L"invoke", L"invoke_result", L"decay", L"enable_if", L"conditional",
    L"integral_constant", L"bool_constant", L"is_same",
    L"is_convertible", L"remove_reference", L"remove_cv",
    L"add_pointer", L"add_reference", L"add_const",
    L"filesystem", L"format", L"print", L"println", L"ranges", L"span",
    L"bit_cast", L"source_location", L"expected", L"generator", L"mdspan",
    L"atomic", L"mutex", L"recursive_mutex", L"timed_mutex", L"shared_mutex",
    L"condition_variable", L"condition_variable_any", L"future", L"shared_future",
    L"promise", L"packaged_task", L"async", L"jthread", L"stop_token", L"stop_source",
    L"latch", L"barrier", L"counting_semaphore", L"binary_semaphore", L"memory_order",
    L"string_view", L"wstring_view", L"u8string_view", L"u16string_view", L"u32string_view",
    L"u8string", L"u16string", L"u32string", L"nullopt", L"in_place",
    L"in_place_type", L"in_place_index", L"numeric_limits", L"ratio", L"chrono",
    L"thread", L"this_thread", L"smatch", L"cmatch", L"wsmatch",
    L"regex_match", L"regex_search", L"regex_replace", L"multimap", L"multiset",
    L"unordered_map", L"unordered_multimap", L"unordered_set", L"unordered_multiset",
    L"forward_list", L"deque", L"unique_ptr", L"bind", L"mem_fn", L"invoke",
    L"make_tuple", L"allocator", L"allocator_traits", L"char_traits",
    L"basic_string", L"basic_ostream", L"basic_istream", L"ios_base",
    L"streamsize", L"streamoff", L"streampos", L"fpos", L"true_type", L"false_type",
    L"is_same_v", L"enable_if_t", L"decay_t", L"remove_reference_t",
    L"type_identity", L"type_identity_t", L"byte", L"to_integer",
    L"mark_count", L"flags", L"imbue", L"getloc",
    L"prefix", L"suffix", L"str", L"length", L"position",
    L"get_allocator",
    L"X", L"Y", L"Width", L"Height", L"Inflate", L"Intersect",
    L"Union", L"Offset", L"Contains", L"IsEmptyArea", L"Equals",
    L"GetLeft", L"GetTop", L"GetRight", L"GetBottom", L"IsEmpty",
    L"GetA", L"GetR", L"GetG", L"GetB", L"GetValue", L"SetValue",
    L"SetFromCOLORREF", L"ToCOLORREF", L"MakeARGB",
    L"DrawLine", L"DrawString", L"DrawImage", L"DrawRectangle",
    L"DrawEllipse", L"DrawArc", L"DrawPath", L"DrawPolygon",
    L"DrawCurve", L"DrawBezier", L"FillRectangle", L"FillEllipse",
    L"FillPath", L"FillPolygon", L"FillRegion", L"Clear",
    L"SetSmoothingMode", L"SetTextRenderingHint",
    L"SetInterpolationMode", L"SetPixelOffsetMode",
    L"SetCompositingMode", L"SetCompositingQuality",
    L"SetClip", L"ResetClip", L"TranslateTransform",
    L"RotateTransform", L"ScaleTransform", L"Save", L"Restore",
    L"MeasureString", L"GetHDC", L"ReleaseHDC",
    L"SetColor", L"GetColor", L"SetWidth", L"GetWidth",
    L"SetDashStyle", L"SetLineJoin", L"SetLineCap", L"SetStartCap",
    L"SetEndCap", L"SetDashPattern", L"SetDashOffset",
    L"SetMiterLimit", L"SetAlignment", L"SetTransform",
    L"ResetTransform", L"GetTransform", L"MultiplyTransform",
    L"GetType", L"SetLinearColors", L"GetLinearColors",
    L"SetInterpolationColors", L"SetBlend", L"SetBlendBellShape",
    L"SetBlendTriangularShape", L"SetGammaCorrection",
    L"GetRectangle", L"SetWrapMode", L"GetWrapMode",
    L"GetFamily", L"GetStyle", L"GetSize", L"GetUnit",
    L"GetLogFontW",
    L"GetFamilyName", L"GetLineSpacing", L"GetEmHeight",
    L"GetCellAscent", L"GetCellDescent", L"IsStyleAvailable",
    L"AddArc", L"AddLine", L"AddRectangle", L"AddEllipse",
    L"AddPolygon", L"AddString", L"AddPath", L"AddBezier",
    L"AddBeziers", L"AddCurve", L"AddClosedCurve", L"AddPie",
    L"CloseFigure", L"StartFigure", L"FillMode", L"SetFillMode",
    L"GetFillMode", L"GetLastPoint", L"GetPointCount",
    L"GetPathPoints", L"GetPathTypes", L"GetBounds", L"Flatten",
    L"Outline", L"Widen", L"Warp", L"Transform", L"IsVisible",
    L"IsOutlineVisible",
    L"LockBits", L"UnlockBits", L"GetPixel", L"SetPixel",
    L"GetPixelFormat", L"FromFile", L"FromStream", L"FromHICON",
    L"FromHBITMAP", L"FromResource", L"GetThumbnailImage",
    L"Stride", L"PixelFormat", L"Scan0", L"Reserved",
    L"SetTrimming", L"GetTrimming", L"SetLineAlignment",
    L"GetLineAlignment", L"SetFormatFlags", L"GetFormatFlags",
    L"SetTabStops", L"SetDigitSubstitution", L"SetHotkeyPrefix"
};

static const set<wstring> CPP_RESERVED_FREE_FUNCTIONS = {
    L"GetMessage", L"GetMessageW", L"GetMessageA", L"PeekMessage",
    L"PeekMessageW", L"PeekMessageA", L"TranslateMessage", L"DispatchMessage",
    L"DispatchMessageW", L"DispatchMessageA", L"PostQuitMessage",
    L"PostMessage", L"PostMessageW", L"PostMessageA", L"SendMessage",
    L"SendMessageW", L"SendMessageA", L"DefWindowProc", L"DefWindowProcW",
    L"DefWindowProcA", L"RegisterClass", L"RegisterClassW", L"RegisterClassA",
    L"RegisterClassEx", L"RegisterClassExW", L"RegisterClassExA",
    L"CreateWindow", L"CreateWindowEx", L"CreateWindowExW", L"CreateWindowExA",
    L"ShowWindow", L"UpdateWindow", L"DestroyWindow", L"LoadIcon",
    L"LoadIconW", L"LoadIconA", L"LoadCursor", L"LoadCursorW", L"LoadCursorA",
    L"LoadString", L"LoadStringW", L"LoadStringA", L"MessageBox",
    L"MessageBoxW", L"MessageBoxA", L"GetWindowLongPtr", L"SetWindowLongPtr",
    L"GetWindowLong", L"SetWindowLong", L"SetTimer", L"KillTimer",
    L"InvalidateRect", L"GetClientRect", L"GetWindowRect",
    L"CreateFile", L"CreateFileW", L"CreateFileA", L"ReadFile", L"WriteFile",
    L"CloseHandle", L"DuplicateHandle", L"CreateThread", L"CreateProcess",
    L"CreateProcessW", L"CreateProcessA", L"OpenProcess", L"TerminateProcess",
    L"ExitProcess", L"ExitThread", L"WaitForSingleObject",
    L"WaitForMultipleObjects", L"GetExitCodeProcess", L"GetExitCodeThread",
    L"GetCurrentProcess", L"GetCurrentThread", L"GetCurrentProcessId",
    L"GetCurrentThreadId", L"Sleep", L"SleepEx", L"GetLastError",
    L"SetLastError", L"FormatMessage", L"FormatMessageW", L"FormatMessageA",
    L"LoadLibrary", L"LoadLibraryW", L"LoadLibraryA", L"LoadLibraryEx",
    L"GetProcAddress", L"FreeLibrary", L"GetModuleHandle",
    L"GetModuleHandleW", L"GetModuleHandleA", L"GetModuleFileName",
    L"GetModuleFileNameW", L"GetModuleFileNameA",
    L"VirtualAlloc", L"VirtualFree", L"VirtualProtect", L"HeapAlloc",
    L"HeapFree", L"HeapCreate", L"HeapDestroy", L"GlobalAlloc", L"GlobalFree",
    L"LocalAlloc", L"LocalFree",
    L"RegOpenKeyEx", L"RegOpenKeyExW", L"RegOpenKeyExA", L"RegCloseKey",
    L"RegQueryValueEx", L"RegQueryValueExW", L"RegQueryValueExA",
    L"RegSetValueEx", L"RegSetValueExW", L"RegSetValueExA", L"RegCreateKeyEx",
    L"RegCreateKeyExW", L"RegCreateKeyExA", L"RegDeleteKey", L"RegDeleteValue",
    L"RegEnumKeyEx", L"RegEnumValue",
    L"CoInitialize", L"CoInitializeEx", L"CoUninitialize", L"CoCreateInstance",
    L"CoCreateInstanceEx", L"CLSIDFromString", L"IIDFromString",
    L"VariantInit", L"VariantClear", L"SysAllocString", L"SysFreeString",
    L"malloc", L"calloc", L"realloc", L"free", L"memcpy", L"memmove",
    L"memset", L"memcmp", L"strlen", L"strcpy", L"strcpy_s", L"strcat",
    L"strcat_s", L"strcmp", L"strncmp", L"sprintf", L"sprintf_s", L"snprintf",
    L"vsnprintf", L"printf", L"fprintf", L"wprintf", L"wcslen", L"wcscpy",
    L"wcscpy_s", L"wcscat", L"wcscat_s", L"wcscmp", L"wcsncmp",
    L"TlsAlloc", L"TlsFree", L"TlsGetValue", L"TlsSetValue",
    L"_InterlockedIncrement", L"_InterlockedDecrement", L"_InterlockedExchange",
    L"_InterlockedExchangeAdd", L"_InterlockedCompareExchange", L"_InterlockedCompareExchange64",
    L"_InterlockedAnd", L"_InterlockedOr", L"_InterlockedXor",
    L"_InterlockedExchangePointer", L"_InterlockedCompareExchangePointer",
    L"_InterlockedIncrement16", L"_InterlockedDecrement16", L"_InterlockedCompareExchange16",
    L"_InterlockedAnd8", L"_InterlockedOr8", L"_InterlockedXor8", L"_InterlockedExchange8",
    L"_ReturnAddress", L"_AddressOfReturnAddress",
    L"_byteswap_ushort", L"_byteswap_ulong", L"_byteswap_uint64",
    L"_rotl", L"_rotr", L"_rotl64", L"_rotr64",
    L"_alloca", L"_malloca", L"_freea"
};

static const set<wstring> CSHARP_STMT_WORDS = {
    L"return", L"throw", L"new", L"else", L"do", L"goto", L"case", L"yield",
    L"await", L"lock", L"using", L"namespace", L"checked", L"unchecked",
    L"fixed", L"default", L"switch", L"foreach", L"for", L"while", L"if",
    L"catch", L"finally", L"try", L"delegate", L"event", L"operator"
};

static const set<wstring> CPP_STMT_WORDS = {
    L"return", L"throw", L"new", L"delete", L"else", L"do", L"goto", L"case",
    L"typedef", L"using", L"namespace", L"template", L"friend", L"sizeof",
    L"co_return", L"co_yield", L"co_await", L"static_assert", L"operator",
    L"switch", L"for", L"while", L"if", L"catch", L"try", L"default"
};

static const set<wstring> CSHARP_MODIFIER_WORDS = {
    L"public", L"private", L"protected", L"internal", L"static", L"sealed",
    L"abstract", L"partial", L"virtual", L"override", L"async", L"unsafe",
    L"extern", L"new", L"readonly", L"const", L"volatile", L"event",
    L"delegate", L"explicit", L"implicit"
};

static const set<wstring> CPP_MODIFIER_WORDS = {
    L"explicit", L"static", L"virtual", L"inline", L"friend", L"constexpr",
    L"mutable", L"extern", L"public", L"private", L"protected"
};

static bool isStatementWord(const wstring& name, Lang lang) {
    if (lang == Lang::CSharp) return CSHARP_STMT_WORDS.count(name) != 0;
    if (lang == Lang::Cpp) return CPP_STMT_WORDS.count(name) != 0;
    return false;
}

static bool isModifierOnlyWord(const wstring& name, Lang lang) {
    if (lang == Lang::CSharp) return CSHARP_MODIFIER_WORDS.count(name) != 0;
    if (lang == Lang::Cpp) return CPP_MODIFIER_WORDS.count(name) != 0;
    return false;
}

// Contextual keywords are legal identifiers in some positions, but treating
// them as renameable identifiers is unsafe for a source-to-source obfuscator:
// their meaning depends on grammar context (query expressions, async/await,
// records, patterns, nullable annotations, etc.).  Preserve them globally.
// This deliberately prefers a small amount of under-renaming to invalid code.
static const set<wstring> CSHARP_CONTEXTUAL_KEYWORDS = {
    L"add", L"alias", L"and", L"ascending", L"async", L"await", L"by",
    L"descending", L"dynamic", L"equals", L"file", L"from", L"get",
    L"global", L"group", L"init", L"into", L"join", L"let", L"managed",
    L"nameof", L"nint", L"not", L"notnull", L"nuint", L"on", L"or",
    L"orderby", L"partial", L"record", L"remove", L"required", L"scoped",
    L"select", L"set", L"unmanaged", L"value", L"var", L"when", L"where",
    L"with", L"yield"
};

static const set<wstring> VB_CONTEXTUAL_KEYWORDS = {
    L"addhandler", L"aggregate", L"ansi", L"assembly", L"async",
    L"auto", L"await", L"by", L"custom", L"equals", L"from", L"group",
    L"handles", L"iterator", L"into", L"isfalse", L"istrue", L"join",
    L"key", L"mid", L"nameof", L"narrowing", L"off", L"on", L"orderby",
    L"preserve", L"removehandler", L"select", L"skip", L"strict",
    L"take", L"text", L"unicode", L"widening", L"where", L"aggregate"
};

static bool containsCaseInsensitive(const set<wstring>& values, const wstring& value) {
    for (const auto& candidate : values) {
        if (candidate.size() != value.size()) continue;
        bool same = true;
        for (size_t i = 0; i < value.size(); ++i) {
            if (towlower(candidate[i]) != towlower(value[i])) { same = false; break; }
        }
        if (same) return true;
    }
    return false;
}

static bool isReserved(const wstring& name, Lang lang) {
    switch (lang) {
    case Lang::VB:
        // VB identifiers and keywords are case-insensitive.
        return containsCaseInsensitive(VB_KEYWORDS, name) ||
            containsCaseInsensitive(DOTNET_TYPES, name) ||
            containsCaseInsensitive(VB_CONTEXTUAL_KEYWORDS, name);
    case Lang::CSharp:
        return CSHARP_KEYWORDS.count(name) != 0 ||
            DOTNET_TYPES.count(name) != 0 ||
            CSHARP_CONTEXTUAL_KEYWORDS.count(name) != 0;
    case Lang::Cpp:
        return CPP_KEYWORDS.count(name) != 0 || CPP_STD_TYPES.count(name) != 0 ||
            CPP_MSVC_PRAGMA_WORDS.count(name) != 0 ||
            CPP_MSVC_DECLSPEC_WORDS.count(name) != 0 ||
            CPP_GCC_ATTRIBUTE_WORDS_COMMON.count(name) != 0;
    }
    return true;
}

// ============================================================================
// STRING UTILS
// ============================================================================
static wstring toLowerW(const wstring& s) {
    wstring r = s;
    transform(r.begin(), r.end(), r.begin(), ::towlower);
    return r;
}

static bool iequals(const wstring& a, const wstring& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (towlower(a[i]) != towlower(b[i])) return false;
    return true;
}

static string wstringToString(const wstring& ws) {
    string out;
    out.reserve(ws.size() * 2);
    for (size_t i = 0; i < ws.size(); i++) {
        unsigned long cp = (unsigned long)(wchar_t)ws[i];
        if (sizeof(wchar_t) == 2 && cp >= 0xD800 && cp <= 0xDBFF && i + 1 < ws.size()) {
            unsigned long lo = (unsigned long)(wchar_t)ws[i + 1];
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                i++;
            }
        }
        if (cp <= 0x7F) out += (char)cp;
        else if (cp <= 0x7FF) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        }
        else if (cp <= 0xFFFF) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
        else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

static wstring stringToWstring(const string& s) {
    wstring out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = (unsigned char)s[i];
        unsigned long cp = 0;
        int extra = 0;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { i++; continue; }
        if (i + extra >= s.size()) break;
        bool ok = true;
        for (int k = 1; k <= extra; k++) {
            unsigned char cc = (unsigned char)s[i + k];
            if ((cc & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { i++; continue; }
        i += extra + 1;
        if (sizeof(wchar_t) == 2 && cp > 0xFFFF) {
            cp -= 0x10000;
            out += (wchar_t)(0xD800 + (cp >> 10));
            out += (wchar_t)(0xDC00 + (cp & 0x3FF));
        }
        else out += (wchar_t)cp;
    }
    return out;
}

static wstring trimW(const wstring& s) {
    wstring r = s;
    size_t a = r.find_first_not_of(L" \t\r\n");
    if (a == wstring::npos) return L"";
    size_t b = r.find_last_not_of(L" \t\r\n");
    return r.substr(a, b - a + 1);
}

static wstring firstWordOf(const wstring& s) {
    size_t i = 0;
    while (i < s.size() && iswspace(s[i])) i++;
    size_t start = i;
    while (i < s.size() && (iswalnum(s[i]) || s[i] == L'_')) i++;
    return s.substr(start, i - start);
}

void LogMessage(const wchar_t* msg, COLORREF color);
void LogMessage(const wstring& msg, COLORREF color);

// ============================================================================
// SYMBOL STATE
// ============================================================================
static set<wstring> g_generatedWin32Symbols;
static set<wstring> g_generatedThirdPartySymbols;
static set<wstring> g_generatedHeaderOnlySymbols;
static set<wstring> g_generatedNetSymbols;      // legacy net_assemblies.txt (kept for backward compat)
static set<wstring> g_generatedNetMembers;      // authoritative: net_members.txt (spec Part A1)
static set<wstring> g_generatedVbRuntime;       // vb_runtime.txt (spec Part A2)
static set<wstring> g_generatedProjectSymbols;  // external_symbols.txt per project (spec Part B)

static std::atomic<int> g_hitMembers{ 0 };
static std::atomic<int> g_hitFreeFn{ 0 };
static std::atomic<int> g_hitWin32{ 0 };
static std::atomic<int> g_hitThirdParty{ 0 };
static std::atomic<int> g_hitHeaderOnly{ 0 };
static std::atomic<int> g_hitNet{ 0 };
static std::atomic<int> g_hitMiss{ 0 };
static std::atomic<int> g_totalLookups{ 0 };

static void resetLayerHits() {
    g_hitMembers = 0; g_hitFreeFn = 0; g_hitWin32 = 0; g_hitThirdParty = 0;
    g_hitHeaderOnly = 0; g_hitNet = 0; g_hitMiss = 0; g_totalLookups = 0;
}

static bool isWin32MacroConstant(const wstring& name) {
    static const wchar_t* kPrefixes[] = {
        L"WM_", L"WS_", L"CS_", L"SW_", L"BS_", L"ES_", L"SS_", L"IDC_", L"IDI_",
        L"GWL_", L"GWLP_", L"CW_", L"PS_", L"DT_", L"MF_", L"BM_", L"SM_", L"VK_",
        L"TPM_", L"LWA_", L"WS_EX_", L"HWND_", L"COLOR_", L"SWP_", L"LB_", L"CB_",
        L"EM_", L"TVM_", L"TVS_", L"LVS_", L"LVM_", L"ODS_", L"DFC_", L"DFCS_"
    };
    if (name.size() < 3) return false;
    for (auto* p : kPrefixes) {
        size_t plen = wcslen(p);
        if (name.compare(0, plen, p) == 0) {
            bool allCapsTail = true;
            for (size_t i = plen; i < name.size(); i++) {
                wchar_t c = name[i];
                if (!(iswupper(c) || iswdigit(c) || c == L'_')) { allCapsTail = false; break; }
            }
            if (allCapsTail) return true;
        }
    }
    return false;
}

// Common WinForms/WPF inherited members and attached properties that must
// never be renamed regardless of which language-specific protected-name
// lists happen to be loaded: these are "member names", not type names, so
// the DOTNET_TYPES/CPP_STD_TYPES lists never cover them, and the loaded
// g_generatedNetSymbols data file may simply not include every one of them.
static const set<wstring> COMMON_DOTNET_FRAMEWORK_MEMBERS = {
    // FIX (A4): immediate one-line fix while the reflection-based generator (Part A)
    // is rebuilt to emit the full public surface of System.Drawing.Image and friends.
    L"RotateFlip", L"DragMove", L"OnMouseLeftButtonDown", L"OnMouseLeftButtonUp",
    L"OnMouseRightButtonDown", L"OnMouseRightButtonUp", L"OnMouseMove", L"OnMouseWheel",
    L"OnMouseEnter", L"OnMouseLeave", L"OnRender", L"OnApplyTemplate",
    L"OnPropertyChanged", L"OnInitialized", L"OnPreviewKeyDown", L"OnPreviewKeyUp",
    L"DebuggerBrowsable", L"DebuggerBrowsableState", L"Never",
    L"DebuggerDisplay", L"DebuggerStepThrough", L"DebuggerHidden", L"DebuggerNonUserCode",
    // WinForms Control/Form overrides and members commonly touched by hand
    L"OnLoad", L"OnPaint", L"OnClosing", L"OnClosed", L"OnResize", L"OnClick",
    L"OnKeyDown", L"OnKeyUp", L"OnKeyPress", L"OnMouseDown", L"OnMouseUp",
    L"OnMouseMove", L"OnMouseClick", L"OnEnter", L"OnLeave", L"OnGotFocus",
    L"OnLostFocus", L"OnShown", L"OnLoadComplete", L"OnHandleCreated",
    L"SuspendLayout", L"ResumeLayout", L"PerformLayout", L"InitializeComponent",
    L"ClientSize", L"Controls", L"Dispose",
    L"DefaultSize", L"ShowWithoutActivation", L"CreateParams",
    // Plain WinForms Control/Form methods and properties commonly called
    // directly (not just overridden) -- these are "this.Close()"-style
    // calls, not overrides, so hasPrecedingOverrideModifier never applies.
    L"Close", L"Show", L"ShowDialog", L"Hide", L"Focus", L"Select", L"Activate",
    L"Update", L"Refresh", L"Invalidate", L"BringToFront", L"SendToBack",
    L"PerformClick", L"CreateGraphics", L"ResetText", L"PointToScreen",
    L"PointToClient", L"RectangleToScreen", L"RectangleToClient", L"WndProc",
    L"DefWndProc", L"Invoke", L"BeginInvoke", L"EndInvoke", L"InvokeRequired",
    L"IsInputKey", L"ProcessCmdKey", L"FormClosing", L"FormClosed", L"Load",
    L"StartPosition", L"FormBorderStyle", L"WindowState", L"TopMost",
    L"ShowInTaskbar", L"Opacity", L"KeyPreview", L"AcceptButton", L"CancelButton",
    L"DialogResult", L"MinimizeBox", L"MaximizeBox", L"ControlBox", L"Icon",
    L"AutoScaleMode", L"AutoScaleDimensions", L"DoubleBuffered",
    // Common BCL enum MEMBER VALUES (StringComparison.OrdinalIgnoreCase,
    // DebuggerBrowsableState.Never, ImageLayout/DataGridViewImageCellLayout
    // .Stretch/.Zoom/etc.) -- these are accessed via EnumType.Value, so they
    // need the same "after a dot" protection as any other member name.
    L"Ordinal", L"OrdinalIgnoreCase", L"InvariantCulture", L"InvariantCultureIgnoreCase",
    L"CurrentCulture", L"CurrentCultureIgnoreCase",
    L"Never", L"Collapsed", L"RootHidden",
    L"Tile", L"Stretch", L"Zoom", L"Normal",
    // DataGridViewCellEventArgs / DataGridViewCellMouseEventArgs members
    L"RowIndex", L"ColumnIndex", L"Clicks", L"Delta",
    // ProcessStartInfo members
    L"FileName", L"Arguments", L"UseShellExecute", L"CreateNoWindow",
    L"RedirectStandardOutput", L"RedirectStandardError", L"RedirectStandardInput",
    L"WindowStyle", L"Verb", L"WorkingDirectory", L"EnvironmentVariables",
    L"StandardOutputEncoding",
    // TcpClient members
    L"NoDelay", L"ReceiveBufferSize", L"SendBufferSize", L"ReceiveTimeout",
    L"SendTimeout", L"Connected", L"Client", L"GetStream", L"Available",
    L"LingerState", L"ExclusiveAddressUse",
    // WebUtility methods
    L"UrlEncode", L"UrlDecode", L"HtmlEncode", L"HtmlDecode",
    // WPF attached properties commonly set on ScrollViewer/other controls
    L"HorizontalScrollBarVisibility", L"VerticalScrollBarVisibility",
    L"CanContentScroll", L"IsDeferredScrollingEnabled",
    L"HorizontalAlignment", L"VerticalAlignment", L"HorizontalContentAlignment",
    L"VerticalContentAlignment", L"Visibility", L"IsEnabled", L"IsHitTestVisible",
    L"Background", L"Foreground", L"BorderBrush", L"BorderThickness", L"Padding",
    L"Margin", L"Width", L"Height", L"MinWidth", L"MinHeight", L"MaxWidth",
    L"MaxHeight", L"Content", L"DataContext", L"Resources", L"Style", L"Template",
    L"TargetType", L"BasedOn",
    // Additional WPF/ScrollViewer/Touch properties commonly used in XAML.
    // Keep these stable even when framework metadata generation is incomplete.
    L"PanningMode", L"PanningDeceleration", L"PanningRatio",
    L"IsManipulationEnabled", L"ManipulationBoundaryFeedback",
    L"CanHorizontallyScroll", L"CanVerticallyScroll",
    L"HorizontalOffset", L"VerticalOffset", L"ScrollableWidth", L"ScrollableHeight",
    L"ViewportWidth", L"ViewportHeight", L"ExtentWidth", L"ExtentHeight",
    L"IsTabStop", L"Focusable", L"ToolTip", L"Cursor", L"ClipToBounds",
    L"SnapsToDevicePixels", L"UseLayoutRounding", L"RenderTransform",
    L"RenderTransformOrigin", L"LayoutTransform", L"OpacityMask",
    L"BitmapEffect", L"Effect", L"CacheMode", L"InputBindings",
    // System.Drawing.Rectangle (and Point/Size, which share several of these)
    L"X", L"Y", L"Location", L"Size", L"Left", L"Top", L"Right", L"Bottom",
    L"IsEmpty", L"Empty", L"Contains", L"IntersectsWith", L"Inflate", L"Offset",
    L"FromLTRB", L"Round", L"Ceiling", L"Truncate",
};

// WPF's DependencyProperty convention: a property "Foo" always comes with a
// public static readonly field "FooProperty", and an attached property
// additionally comes with static "GetFoo"/"SetFoo" accessors. Those three
// derived names are never individually listed anywhere (DOTNET_TYPES/
// CPP_STD_TYPES only have type names, and COMMON_DOTNET_FRAMEWORK_MEMBERS
// above would need 3x as many entries to cover every property by hand), so
// this checks the convention directly: if the base name (after stripping
// "Property" or a "Get"/"Set" prefix) is itself a known framework member,
// the derived name is protected the same way. This is intentionally a bit
// broader than strictly necessary -- it will also leave a user's own
// custom DependencyProperty fields un-renamed -- but under-renaming a
// field is harmless where over-renaming one breaks the build outright.
static bool isWpfDependencyPropertyConvention(const wstring& name) {
    if (name.size() > 8 && name.compare(name.size() - 8, 8, L"Property") == 0) {
        wstring base = name.substr(0, name.size() - 8);
        if (COMMON_DOTNET_FRAMEWORK_MEMBERS.count(base)) return true;
    }
    if (name.size() > 3 && (name.compare(0, 3, L"Get") == 0 || name.compare(0, 3, L"Set") == 0)) {
        wstring base = name.substr(3);
        if (COMMON_DOTNET_FRAMEWORK_MEMBERS.count(base)) return true;
    }
    return false;
}

// FIX (A2): the VB "My" namespace (My.Application, My.Computer, My.Forms, ...) is
// compiler-synthesized per project -- it is not a member of any loadable assembly,
// so reflection (here or in Part B's project/NuGet scan) can never discover it.
// Hardcoded, as the spec calls for. Used in processLineVB's dot-branch below, not
// here, because isFrameworkMember() is a free function with no access to this
// engine instance's "lang" -- gating by language at this call would need a global,
// and the actual risk ("My.Settings" colliding with the user's own local field or
// method literally named "Settings"/"Computer"/etc.) only exists for VB anyway.
static const set<wstring> VB_MY_NAMESPACE = {
    L"Application", L"Computer", L"Forms", L"Resources", L"Settings", L"User", L"WebServices"
};

// FIX: used only by property/event-accessor extraction (the two shapes that can
// legitimately be an "override" of a real framework member) to check THIS
// declaration's own modifiers for "override", rather than applying
// isFrameworkMember() to every property/event unconditionally -- a plain,
// non-override property/event must not be protected from renaming just
// because its name happens to collide with an unrelated framework member.
// Scans backward from matchStart over whitespace-separated modifier words
// (public/private/static/virtual/override/...); stops at the first word that
// is not a recognized modifier, which is the previous statement's "}"/";" or
// some other token the regex itself did not consume.
static bool declarationHasOverrideModifier(const string& src, size_t matchStart) {
    // Property/accessor regexes begin at the visibility keyword (or at `event`),
    // so scanning backwards from matchStart misses `protected override ...`.
    // Inspect only the physical declaration line to avoid borrowing an override
    // modifier from a preceding member. This is deliberately conservative: any
    // `override` token on that declaration line protects the member name.
    if (matchStart > src.size()) return false;
    size_t lineStart = src.rfind('\n', matchStart);
    lineStart = (lineStart == string::npos) ? 0 : lineStart + 1;
    size_t lineEnd = src.find('\n', matchStart);
    if (lineEnd == string::npos) lineEnd = src.size();
    string declarationLine = src.substr(lineStart, lineEnd - lineStart);
    static const regex overrideWord(R"(\boverride\b)");
    return regex_search(declarationLine, overrideWord);
}

// FIX (gap 5): finds the bare (pre-rename) name of the member -- method,
// property, or indexer -- whose body textually contains the position
// callPos. Needed so that a call to a [CallerMemberName]-parameterized
// method, made with that argument omitted, can have ITS CALLER's enclosing
// member protected from renaming: the compiler captures that member's name
// at compile time from the final (already-renamed) source, so the capture
// itself can never drift from the declaration -- but other code comparing
// the captured string against a literal elsewhere could, if that member's
// name changed. Returns L"" (a miss, never a wrong answer) if no enclosing
// brace can be found, e.g. for an expression-bodied member with no braces.
static wstring findEnclosingMemberName(const string& src, size_t callPos) {
    int depth = 0;
    size_t bracePos = string::npos;
    for (size_t i = callPos; i-- > 0; ) {
        char c = src[i];
        if (c == '}') depth++;
        else if (c == '{') { if (depth == 0) { bracePos = i; break; } depth--; }
    }
    size_t latestArrow = src.rfind("=>", callPos);
    if (latestArrow != string::npos && (bracePos == string::npos || latestArrow > bracePos)) {
        size_t boundary = src.find_last_of(";}\r\n", latestArrow);
        boundary = (boundary == string::npos || boundary < bracePos) ? (bracePos == string::npos ? 0 : bracePos + 1) : boundary + 1;
        string header = src.substr(boundary, latestArrow - boundary);
        size_t paren = header.find_last_of('(');
        size_t e = (paren == string::npos) ? header.size() : paren;
        while (e > 0 && isspace((unsigned char)header[e - 1])) e--;
        size_t b = e;
        while (b > 0 && (isalnum((unsigned char)header[b - 1]) || header[b - 1] == '_')) b--;
        if (b < e) return stringToWstring(header.substr(b, e - b));
    }
    if (bracePos == string::npos) {
        // Expression-bodied methods/properties have no body brace. Resolve the
        // nearest declaration header preceding the call and its '=>' marker.
        size_t arrow = src.rfind("=>", callPos);
        if (arrow != string::npos) {
            size_t boundary = src.find_last_of(";}\r\n", arrow);
            boundary = (boundary == string::npos) ? 0 : boundary + 1;
            string header = src.substr(boundary, arrow - boundary);
            // Prefer the identifier immediately before the declaration parameter
            // list; for expression-bodied properties, use the last header word.
            size_t paren = header.find_last_of('(');
            size_t e = (paren == string::npos) ? header.size() : paren;
            while (e > 0 && isspace((unsigned char)header[e - 1])) e--;
            size_t b = e;
            while (b > 0 && (isalnum((unsigned char)header[b - 1]) || header[b - 1] == '_')) b--;
            if (b < e) return stringToWstring(header.substr(b, e - b));
        }
        return L"";
    }
    // A property accessor's OWN brace ("get { ... }" / "set { ... }" / "add
    // { ... }" / "remove { ... }" / init accessors) names nothing itself --
    // CallerMemberName there reports the PROPERTY's/event's name, not
    // "get_Foo" -- so step out past a bare accessor keyword to the member's
    // own head.
    {
        size_t p = bracePos;
        while (p > 0 && isspace((unsigned char)src[p - 1])) p--;
        size_t wordEnd = p;
        while (p > 0 && isalpha((unsigned char)src[p - 1])) p--;
        string word = src.substr(p, wordEnd - p);
        if (word == "get" || word == "set" || word == "init" || word == "add" || word == "remove") {
            int depth2 = 0;
            size_t outerBrace = string::npos;
            for (size_t i = p; i-- > 0; ) {
                char c = src[i];
                if (c == '}') depth2++;
                else if (c == '{') { if (depth2 == 0) { outerBrace = i; break; } depth2--; }
            }
            if (outerBrace != string::npos) bracePos = outerBrace;
        }
    }
    size_t boundary = 0;
    for (size_t i = bracePos; i-- > 0; ) {
        char c = src[i];
        if (c == ';' || c == '}') { boundary = i + 1; break; }
    }
    if (boundary > bracePos) return L"";
    string header = src.substr(boundary, bracePos - boundary);
    // Last unnested "(" in the header is a method/indexer's parameter list;
    // the identifier right before it is the name. With no such "(" at all
    // (an auto-property's accessor block), fall back to the last identifier
    // in the header outright -- the property's own name.
    int pdepth = 0;
    long parenPos = -1;
    for (long i = (long)header.size() - 1; i >= 0; i--) {
        char c = header[(size_t)i];
        if (c == ')') pdepth++;
        else if (c == '(') { if (pdepth == 0) { parenPos = i; break; } pdepth--; }
    }
    string beforeParen = (parenPos >= 0) ? header.substr(0, (size_t)parenPos) : header;
    size_t e = beforeParen.size();
    while (e > 0 && isspace((unsigned char)beforeParen[e - 1])) e--;
    size_t s = e;
    while (s > 0 && (isalnum((unsigned char)beforeParen[s - 1]) || beforeParen[s - 1] == '_')) s--;
    if (s == e) return L"";
    return stringToWstring(beforeParen.substr(s, e - s));
}

static bool isFrameworkMember(const wstring& name) {
    g_totalLookups++;
    if (COMMON_DOTNET_FRAMEWORK_MEMBERS.count(name)) { g_hitNet++; return true; }
    // Spec A3: new generated sets slot in here -- after COMMON_DOTNET_FRAMEWORK_MEMBERS,
    // before the CPP_* checks below.
    if (g_generatedNetMembers.count(name)) { g_hitNet++; return true; }
    // Spec Part B2: project-specific external symbols (reflected from this
    // project's own PackageReference/Reference dependencies) slot in here,
    // immediately after the authoritative generated .NET member set.
    if (g_generatedProjectSymbols.count(name)) { g_hitNet++; return true; }
    if (g_generatedVbRuntime.count(name)) { g_hitNet++; return true; }
    if (isWpfDependencyPropertyConvention(name)) { g_hitNet++; return true; }
    if (isWin32MacroConstant(name)) { g_hitWin32++; return true; }
    if (CPP_RESERVED_MEMBERS.count(name)) { g_hitMembers++; return true; }
    if (CPP_RESERVED_FREE_FUNCTIONS.count(name)) { g_hitFreeFn++; return true; }
    if (g_generatedWin32Symbols.count(name)) { g_hitWin32++; return true; }
    if (!name.empty() && name.back() != L'A' && name.back() != L'W') {
        if (g_generatedWin32Symbols.count(name + L"A") ||
            g_generatedWin32Symbols.count(name + L"W") ||
            g_generatedWin32Symbols.count(name + L"AW")) {
            g_hitWin32++; return true;
        }
    }
    if (g_generatedThirdPartySymbols.count(name)) { g_hitThirdParty++; return true; }
    if (g_generatedHeaderOnlySymbols.count(name)) { g_hitHeaderOnly++; return true; }
    if (g_generatedNetSymbols.count(name)) { g_hitNet++; return true; }
    g_hitMiss++;
    return false;
}

// ============================================================================
// NAME GENERATOR
// ============================================================================
static const BYTE CRYPTO_KEY[32] = {
    0x2E, 0xBE, 0x5C, 0xCD, 0x94, 0x6C, 0xB2, 0xB5,
    0x78, 0x3A, 0x20, 0x26, 0x49, 0x87, 0x5F, 0x26,
    0x23, 0xB8, 0x59, 0x1E, 0xCF, 0x9D, 0x27, 0x0A,
    0xE3, 0x92, 0x1C, 0x89, 0x45, 0x25, 0x26, 0x09
};

static const BYTE CRYPTO_IV[16] = {
    0xAD, 0xFB, 0x58, 0x5A, 0xEB, 0x1C, 0x42, 0x23,
    0xE2, 0x34, 0x8E, 0x3D, 0x6C, 0x61, 0xA8, 0x9D
};

static vector<uint8_t> AddPKCS7Padding(const vector<uint8_t>& data) {
    size_t padLen = 16 - (data.size() % 16);
    if (padLen == 0) padLen = 16;
    vector<uint8_t> padded = data;
    padded.resize(data.size() + padLen, static_cast<uint8_t>(padLen));
    return padded;
}

static string Base64EncodeBytes(const vector<uint8_t>& data) {
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string result;
    size_t i = 0;
    unsigned char a3[3], a4[4];
    for (size_t idx = 0; idx < data.size(); idx++) {
        a3[i++] = data[idx];
        if (i == 3) {
            a4[0] = (a3[0] & 0xfc) >> 2;
            a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
            a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
            a4[3] = a3[2] & 0x3f;
            for (i = 0; i < 4; i++) result += b64[a4[i]];
            i = 0;
        }
    }
    if (i) {
        for (size_t j = i; j < 3; j++) a3[j] = 0;
        a4[0] = (a3[0] & 0xfc) >> 2;
        a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
        a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
        for (size_t j = 0; j < i + 1; j++) result += b64[a4[j]];
        while (i++ < 3) result += '=';
    }
    return result;
}

class CryptoProvider {
    HCRYPTPROV hProv = NULL;
    HCRYPTKEY hKey = NULL;

    void InitCrypto() {
        if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            throw runtime_error("Failed to acquire crypto context");
        struct { BLOBHEADER header; DWORD keySize; BYTE keyBytes[32]; } keyBlob;
        keyBlob.header.bType = PLAINTEXTKEYBLOB;
        keyBlob.header.bVersion = CUR_BLOB_VERSION;
        keyBlob.header.reserved = 0;
        keyBlob.header.aiKeyAlg = CALG_AES_256;
        keyBlob.keySize = 32;
        memcpy(keyBlob.keyBytes, CRYPTO_KEY, 32);
        if (!CryptImportKey(hProv, (BYTE*)&keyBlob, sizeof(keyBlob), 0, 0, &hKey)) {
            CryptReleaseContext(hProv, 0);
            throw runtime_error("Failed to import key");
        }
        DWORD mode = CRYPT_MODE_ECB;
        if (!CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&mode, 0)) {
            CryptDestroyKey(hKey); CryptReleaseContext(hProv, 0);
            throw runtime_error("Failed to set ECB mode");
        }
    }

public:
    CryptoProvider() { InitCrypto(); }
    ~CryptoProvider() {
        if (hKey) CryptDestroyKey(hKey);
        if (hProv) CryptReleaseContext(hProv, 0);
    }

    void EncryptBlock(const BYTE* input, BYTE* output) {
        DWORD dataLen = 16;
        BYTE temp[16];
        memcpy(temp, input, 16);
        if (!CryptEncrypt(hKey, 0, FALSE, 0, temp, &dataLen, 16))
            throw runtime_error("Block encryption failed");
        memcpy(output, temp, 16);
    }

    vector<uint8_t> CFB8_Encrypt(const vector<uint8_t>& plaintext, const BYTE* iv) {
        vector<uint8_t> ciphertext; ciphertext.reserve(plaintext.size());
        BYTE feedback[16]; memcpy(feedback, iv, 16);
        for (size_t i = 0; i < plaintext.size(); i++) {
            BYTE encFeedback[16];
            EncryptBlock(feedback, encFeedback);
            BYTE cipherByte = plaintext[i] ^ encFeedback[0];
            ciphertext.push_back(cipherByte);
            memmove(feedback, feedback + 1, 15);
            feedback[15] = cipherByte;
        }
        return ciphertext;
    }
};

enum class ObfuscationCharset { O0o, vwv, I1lL };

class GlobalNameGenerator {
    map<wstring, wstring, less<>> nameMap;
    set<wstring> usedNamesLower;
    int counter = 0;
    uint32_t freshSeedCounter = 0xA5A5A5A5u;
    ObfuscationCharset charset = ObfuscationCharset::O0o;

    static uint32_t fnv1a(const wstring& s) {
        uint32_t h = 2166136261u;
        for (wchar_t c : s) { h ^= (uint32_t)c; h *= 16777619u; }
        return h;
    }

    static wchar_t firstCharForAlt(ObfuscationCharset cs, mt19937& rng) {
        if (cs == ObfuscationCharset::vwv) return (rng() % 2 == 0) ? L'v' : L'w';
        int c = rng() % 3;
        if (c == 0) return L'I';
        if (c == 1) return L'l';
        return L'L';
    }

    static wchar_t randCharForAlt(ObfuscationCharset cs, mt19937& rng) {
        if (cs == ObfuscationCharset::vwv) return (rng() % 2 == 0) ? L'v' : L'w';
        int c = rng() % 4;
        if (c == 0) return L'I';
        if (c == 1) return L'l';
        if (c == 2) return L'L';
        return L'1';
    }

    wstring makeNameForSeed(uint32_t seed) const {
        mt19937 localRng(seed);
        uniform_int_distribution<int> lenDist(MIN_NAME_LEN, MAX_NAME_LEN);
        int len = lenDist(localRng);
        if (charset == ObfuscationCharset::O0o) {
            uniform_int_distribution<int> charDist(0, 3);
            wstring name = L"o";
            for (int i = 1; i < len; i++) {
                int c = charDist(localRng);
                switch (c) {
                case 0: name += L'O'; break;
                case 1: name += L'o'; break;
                case 2: name += L'0'; break;
                case 3: name += L'O'; break;
                }
            }
            return name;
        }
        wstring name;
        name += firstCharForAlt(charset, localRng);
        for (int i = 1; i < len; i++) name += randCharForAlt(charset, localRng);
        return name;
    }

public:
    GlobalNameGenerator() = default;
    void setCharset(ObfuscationCharset cs) { charset = cs; }

    wstring generateName(const wstring& original) {
        uint32_t seed = fnv1a(original);
        wstring name = makeNameForSeed(seed);
        wstring lowerName = toLowerW(name);
        if (usedNamesLower.count(lowerName)) {
            wstring uniqueName = name + to_wstring(counter++);
            wstring uniqueLower = toLowerW(uniqueName);
            while (usedNamesLower.count(uniqueLower)) {
                uniqueName = name + to_wstring(counter++);
                uniqueLower = toLowerW(uniqueName);
            }
            name = uniqueName;
            lowerName = uniqueLower;
        }
        usedNamesLower.insert(lowerName);
        return name;
    }

    wstring generateFreshName() {
        freshSeedCounter = freshSeedCounter * 1103515245u + 12345u;
        wstring name = makeNameForSeed(freshSeedCounter);
        wstring lowerName = toLowerW(name);
        while (usedNamesLower.count(lowerName)) {
            wstring uniqueName = name + to_wstring(counter++);
            lowerName = toLowerW(uniqueName);
            name = uniqueName;
        }
        usedNamesLower.insert(lowerName);
        return name;
    }

    wstring getObfuscatedName(const wstring& original) {
        auto it = nameMap.find(original);
        if (it != nameMap.end()) return it->second;
        wstring obf = generateName(original);
        nameMap[original] = obf;
        return obf;
    }
};

static wstring UnescapeCFamilyString(const wstring& raw) {
    wstring out; out.reserve(raw.size());
    size_t i = 0;
    while (i < raw.size()) {
        if (raw[i] == L'\\' && i + 1 < raw.size()) {
            wchar_t n = raw[i + 1];
            switch (n) {
            case L'n': out += L'\n'; i += 2; break;
            case L't': out += L'\t'; i += 2; break;
            case L'r': out += L'\r'; i += 2; break;
            case L'0': out += L'\0'; i += 2; break;
            case L'a': out += L'\a'; i += 2; break;
            case L'b': out += L'\b'; i += 2; break;
            case L'f': out += L'\f'; i += 2; break;
            case L'v': out += L'\v'; i += 2; break;
            case L'\\': out += L'\\'; i += 2; break;
            case L'"': out += L'"'; i += 2; break;
            case L'\'': out += L'\''; i += 2; break;
            case L'u': {
                unsigned long cp = 0; int consumed = 0;
                for (int k = 0; k < 4 && i + 2 + k < raw.size() && iswxdigit(raw[i + 2 + k]); k++) {
                    wchar_t h = raw[i + 2 + k];
                    int v = iswdigit(h) ? (h - L'0') : (towlower(h) - L'a' + 10);
                    cp = (cp << 4) | (unsigned long)v;
                    consumed++;
                }
                if (consumed == 4) { out += (wchar_t)cp; i += 2 + 4; }
                else { out += L'\\'; out += L'u'; i += 2; }
                break;
            }
            case L'U': {
                unsigned long cp = 0; int consumed = 0;
                for (int k = 0; k < 8 && i + 2 + k < raw.size() && iswxdigit(raw[i + 2 + k]); k++) {
                    wchar_t h = raw[i + 2 + k];
                    int v = iswdigit(h) ? (h - L'0') : (towlower(h) - L'a' + 10);
                    cp = (cp << 4) | (unsigned long)v;
                    consumed++;
                }
                if (consumed == 8) {
                    if (cp <= 0xFFFF) {
                        out += (wchar_t)cp;
                    }
                    else {
                        unsigned long v = cp - 0x10000;
                        out += (wchar_t)(0xD800 + (v >> 10));
                        out += (wchar_t)(0xDC00 + (v & 0x3FF));
                    }
                    i += 2 + 8;
                }
                else { out += L'\\'; out += L'U'; i += 2; }
                break;
            }
            case L'x': {
                unsigned long cp = 0; int consumed = 0;
                for (int k = 0; k < 8 && i + 2 + k < raw.size() && iswxdigit(raw[i + 2 + k]); k++) {
                    wchar_t h = raw[i + 2 + k];
                    int v = iswdigit(h) ? (h - L'0') : (towlower(h) - L'a' + 10);
                    cp = (cp << 4) | (unsigned long)v;
                    consumed++;
                }
                if (consumed > 0) { out += (wchar_t)cp; i += 2 + consumed; }
                else { out += L'\\'; out += L'x'; i += 2; }
                break;
            }
            default:
                out += L'\\';
                out += n;
                i += 2;
                break;
            }
        }
        else { out += raw[i]; i++; }
    }
    return out;
}

static int BraceDelta(const wstring& line, bool& inBlockComment) {
    int delta = 0;
    size_t i = 0, n = line.size();
    while (i < n) {
        if (inBlockComment) {
            if (line[i] == L'*' && i + 1 < n && line[i + 1] == L'/') { inBlockComment = false; i += 2; }
            else i++;
            continue;
        }
        wchar_t c = line[i];
        if (c == L'/' && i + 1 < n && line[i + 1] == L'/') break;
        if (c == L'/' && i + 1 < n && line[i + 1] == L'*') { inBlockComment = true; i += 2; continue; }
        if (c == L'"') {
            i++;
            while (i < n && line[i] != L'"') { if (line[i] == L'\\' && i + 1 < n) i += 2; else i++; }
            if (i < n) i++;
            continue;
        }
        if (c == L'\'') {
            i++;
            while (i < n && line[i] != L'\'') { if (line[i] == L'\\' && i + 1 < n) i += 2; else i++; }
            if (i < n) i++;
            continue;
        }
        if (c == L'{') { delta++; i++; continue; }
        if (c == L'}') { delta--; i++; continue; }
        i++;
    }
    return delta;
}

static size_t FindMatchingBrace(const wstring& text, size_t openBracePos) {
    int depth = 1;
    size_t i = openBracePos + 1, n = text.size();
    while (i < n) {
        wchar_t c = text[i];
        if (c == L'/' && i + 1 < n && text[i + 1] == L'/') {
            i += 2;
            while (i < n && text[i] != L'\n') i++;
            continue;
        }
        if (c == L'/' && i + 1 < n && text[i + 1] == L'*') {
            i += 2;
            while (i + 1 < n && !(text[i] == L'*' && text[i + 1] == L'/')) i++;
            i = (i + 1 < n) ? i + 2 : n;
            continue;
        }
        if (c == L'"') {
            i++;
            while (i < n && text[i] != L'"') { if (text[i] == L'\\' && i + 1 < n) i += 2; else i++; }
            if (i < n) i++;
            continue;
        }
        if (c == L'\'') {
            i++;
            while (i < n && text[i] != L'\'') { if (text[i] == L'\\' && i + 1 < n) i += 2; else i++; }
            if (i < n) i++;
            continue;
        }
        if (c == L'{') { depth++; i++; continue; }
        if (c == L'}') { depth--; if (depth == 0) return i; i++; continue; }
        i++;
    }
    return wstring::npos;
}

class StringEncryptionHelper {
    GlobalNameGenerator* ng;
    CryptoProvider crypto;

public:
    explicit StringEncryptionHelper(GlobalNameGenerator* nameGen) : ng(nameGen) {}
    wstring generateVarName() { return ng->generateFreshName(); }

    string encryptString(const string& plain) {
        if (plain.empty()) return "";
        vector<uint8_t> plainBytes(plain.begin(), plain.end());
        vector<uint8_t> padded = AddPKCS7Padding(plainBytes);
        vector<uint8_t> encrypted = crypto.CFB8_Encrypt(padded, CRYPTO_IV);
        string b64 = Base64EncodeBytes(encrypted);
        if (b64.size() > 12000) return plain;
        return b64;
    }

    wstring encryptStringW(const wstring& plain) {
        string b64 = encryptString(wstringToString(plain));
        for (char c : b64) if ((unsigned char)c > 0x7F) return plain;
        return stringToWstring(b64);
    }

    struct StubResult { vector<wstring> lines; wstring finalVar; };

    StubResult generateDecryptStub(const wstring& encryptedB64, Lang lang = Lang::VB) {
        StubResult res;
        wstring v1 = generateVarName();
        wstring v2 = generateVarName();
        switch (lang) {
        case Lang::CSharp:
            res.lines.push_back(L"string " + v1 + L" = \"" + encryptedB64 + L"\";");
            res.lines.push_back(L"string " + v2 + L" = mxcncbs.ertrymn(" + v1 + L");");
            break;
        case Lang::Cpp:
            res.lines.push_back(L"std::string " + v1 + L" = \"" + encryptedB64 + L"\";");
            res.lines.push_back(L"std::string " + v2 + L" = mxcncbs::ertrymn(" + v1 + L");");
            break;
        default:
            res.lines.push_back(L"Dim " + v1 + L" As String = \"" + encryptedB64 + L"\"");
            res.lines.push_back(L"Dim " + v2 + L" As String = mxcncbs.ertrymn(" + v1 + L")");
            break;
        }
        res.finalVar = v2;
        return res;
    }
};

class DeadCodeGenerator {
    mt19937 rng;
    GlobalNameGenerator* ng;

    wstring generateRandomString(int len) {
        uniform_int_distribution<int> charDist(0, 61);
        const wchar_t chars[] = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        wstring result;
        for (int i = 0; i < len; i++) result += chars[charDist(rng)];
        return result;
    }

public:
    explicit DeadCodeGenerator(GlobalNameGenerator* nameGen) : ng(nameGen) { rng.seed((unsigned)time(nullptr) ^ clock() ^ 0xDEAD); }

    wstring generateJunkFunction() {
        wstring funcName = ng->generateFreshName();
        wstring var1 = ng->generateFreshName(), var2 = ng->generateFreshName(), var3 = ng->generateFreshName(), var4 = ng->generateFreshName();
        uniform_int_distribution<int> typeDist(0, 5);
        int type = typeDist(rng);
        wstringstream ss;
        ss << L"    Private Function " << funcName << L"() As Integer\n";
        switch (type) {
        case 0:
            ss << L"        Dim " << var1 << L" As Integer = " << (rng() % 900 + 100) << L"\n"
                << L"        Dim " << var2 << L" As Integer = " << (rng() % 900 + 100) << L"\n"
                << L"        Dim " << var3 << L" As Integer = " << var1 << L" + " << var2 << L"\n"
                << L"        Dim " << var4 << L" As Integer = " << var3 << L" * " << (rng() % 10 + 2) << L"\n"
                << L"        If " << var4 << L" < 0 Then\n            Return -1\n"
                << L"        ElseIf " << var4 << L" > 999999 Then\n            Return 1\n        End If\n"
                << L"        Return " << var4 << L" Mod 7\n";
            break;
        case 1:
            ss << L"        Dim " << var1 << L" As String = \"" << generateRandomString(8) << L"\"\n"
                << L"        Dim " << var2 << L" As String = \"" << generateRandomString(8) << L"\"\n"
                << L"        Dim " << var3 << L" As Integer = " << var1 << L".Length + " << var2 << L".Length\n"
                << L"        Dim " << var4 << L" As String = " << var1 << L" & " << var2 << L"\n"
                << L"        If " << var3 << L" > 50 Then\n            Return " << var3 << L" * 2\n        End If\n"
                << L"        Return " << var4 << L".GetHashCode() Mod 100\n";
            break;
        case 2:
            ss << L"        Dim " << var1 << L"(" << (rng() % 10 + 5) << L") As Integer\n"
                << L"        Dim " << var2 << L" As Integer = 0\n"
                << L"        For " << var3 << L" As Integer = 0 To " << var1 << L".Length - 1\n"
                << L"            " << var1 << L"(" << var3 << L") = (" << var3 << L" * " << (rng() % 7 + 3) << L") Mod 256\n"
                << L"            " << var2 << L" += " << var1 << L"(" << var3 << L")\n        Next\n"
                << L"        If " << var2 << L" > 1000 Then\n            Return " << var2 << L" \\ 10\n        End If\n"
                << L"        Return " << var2 << L" Mod 13\n";
            break;
        case 3:
            ss << L"        Dim " << var1 << L" As Integer = 0\n"
                << L"        For " << var2 << L" As Integer = 1 To " << (rng() % 5 + 3) << L"\n"
                << L"            For " << var3 << L" As Integer = 1 To " << (rng() % 5 + 3) << L"\n"
                << L"                " << var1 << L" += " << var2 << L" * " << var3 << L"\n"
                << L"                If " << var1 << L" > 500 Then\n                    Exit For\n                End If\n"
                << L"            Next\n            If " << var1 << L" > 500 Then\n                Exit For\n            End If\n"
                << L"        Next\n        Return " << var1 << L" Mod 17\n";
            break;
        case 4:
            ss << L"        Dim " << var1 << L" As Integer = " << (rng() % 20 + 1) << L"\n"
                << L"        Dim " << var2 << L" As Integer = 0\n"
                << L"        Select Case " << var1 << L"\n"
                << L"            Case 1 To 5\n                " << var2 << L" = " << var1 << L" * 3\n"
                << L"            Case 6 To 10\n                " << var2 << L" = " << var1 << L" * 5\n"
                << L"            Case 11 To 15\n                " << var2 << L" = " << var1 << L" * 7\n"
                << L"            Case Else\n                " << var2 << L" = " << var1 << L" * 11\n"
                << L"        End Select\n        Return " << var2 << L" Mod 23\n";
            break;
        case 5:
            ss << L"        Dim " << var1 << L" As Integer = " << (rng() % 100 + 1) << L"\n"
                << L"        Dim " << var2 << L" As Integer = 0\n"
                << L"        Try\n            " << var2 << L" = " << var1 << L" * " << (rng() % 10 + 2) << L"\n"
                << L"            If " << var2 << L" < 0 Then\n                Throw New InvalidOperationException()\n            End If\n"
                << L"        Catch ex As Exception\n            " << var2 << L" = -1\n        End Try\n"
                << L"        Return " << var2 << L" Mod 31\n";
            break;
        }
        ss << L"    End Function\n";
        return ss.str();
    }

    wstring generateJunkSub() {
        wstring subName = ng->generateFreshName();
        wstring var1 = ng->generateFreshName(), var2 = ng->generateFreshName(), var3 = ng->generateFreshName(), var4 = ng->generateFreshName();
        uniform_int_distribution<int> typeDist(0, 4);
        int type = typeDist(rng);
        wstringstream ss;
        ss << L"    Private Sub " << subName << L"()\n";
        switch (type) {
        case 0:
            ss << L"        Dim " << var1 << L" As DateTime = DateTime.Now\n"
                << L"        Dim " << var2 << L" As TimeSpan = TimeSpan.FromSeconds(" << (rng() % 3600 + 1) << L")\n"
                << L"        Dim " << var3 << L" As DateTime = " << var1 << L" + " << var2 << L"\n"
                << L"        If " << var3 << L" > " << var1 << L" Then\n"
                << L"            Dim " << var4 << L" As Integer = " << var3 << L".Second + " << var1 << L".Minute\n        End If\n";
            break;
        case 1:
            ss << L"        Dim " << var1 << L" As New System.Text.StringBuilder()\n"
                << L"        " << var1 << L".Append(\"" << generateRandomString(12) << L"\")\n"
                << L"        " << var1 << L".Append(\"" << generateRandomString(8) << L"\")\n"
                << L"        Dim " << var2 << L" As String = " << var1 << L".ToString()\n"
                << L"        If " << var2 << L".Length > 10 Then\n            " << var1 << L".Clear()\n        End If\n";
            break;
        case 2:
            ss << L"        Dim " << var1 << L" As New Dictionary(Of String, Integer)\n"
                << L"        " << var1 << L".Add(\"" << generateRandomString(5) << L"\", " << (rng() % 100) << L")\n"
                << L"        " << var1 << L".Add(\"" << generateRandomString(5) << L"\", " << (rng() % 100) << L")\n"
                << L"        Dim " << var2 << L" As Integer = 0\n"
                << L"        For Each " << var3 << L" As KeyValuePair(Of String, Integer) In " << var1 << L"\n"
                << L"            " << var2 << L" += " << var3 << L".Value\n        Next\n";
            break;
        case 3:
            ss << L"        Dim " << var1 << L" As New List(Of Integer)\n"
                << L"        For " << var2 << L" As Integer = 1 To " << (rng() % 8 + 3) << L"\n"
                << L"            " << var1 << L".Add(" << var2 << L" * " << (rng() % 5 + 2) << L")\n        Next\n"
                << L"        " << var1 << L".Sort()\n        " << var1 << L".Reverse()\n"
                << L"        Dim " << var3 << L" As Integer = " << var1 << L".Count\n";
            break;
        case 4:
            ss << L"        Dim " << var1 << L" As Double = " << (rng() % 100 + 1) << L"." << (rng() % 99 + 1) << L"\n"
                << L"        Dim " << var2 << L" As Double = Math.Sqrt(" << var1 << L")\n"
                << L"        Dim " << var3 << L" As Double = Math.Pow(" << var2 << L", 2)\n"
                << L"        If Math.Abs(" << var3 << L" - " << var1 << L") < 0.001 Then\n"
                << L"            Dim " << var4 << L" As Double = " << var1 << L" * " << var2 << L"\n"
                << L"        Else\n            Dim " << var4 << L" As Double = " << var1 << L" + " << var2 << L"\n        End If\n";
            break;
        }
        ss << L"    End Sub\n";
        return ss.str();
    }

    wstring generateJunkFunctionCS() {
        wstring funcName = ng->generateFreshName();
        wstring var1 = ng->generateFreshName(), var2 = ng->generateFreshName(), var3 = ng->generateFreshName(), var4 = ng->generateFreshName();
        uniform_int_distribution<int> typeDist(0, 2);
        int type = typeDist(rng);
        wstringstream ss;
        ss << L"    private static int " << funcName << L"() {\n";
        switch (type) {
        case 0:
            ss << L"        int " << var1 << L" = " << (rng() % 900 + 100) << L";\n"
                << L"        int " << var2 << L" = " << (rng() % 900 + 100) << L";\n"
                << L"        int " << var3 << L" = " << var1 << L" + " << var2 << L";\n"
                << L"        int " << var4 << L" = " << var3 << L" * " << (rng() % 10 + 2) << L";\n"
                << L"        if (" << var4 << L" < 0) { return -1; }\n"
                << L"        else if (" << var4 << L" > 999999) { return 1; }\n"
                << L"        return " << var4 << L" % 7;\n";
            break;
        case 1:
            ss << L"        string " << var1 << L" = \"" << generateRandomString(8) << L"\";\n"
                << L"        string " << var2 << L" = \"" << generateRandomString(8) << L"\";\n"
                << L"        int " << var3 << L" = " << var1 << L".Length + " << var2 << L".Length;\n"
                << L"        string " << var4 << L" = " << var1 << L" + " << var2 << L";\n"
                << L"        if (" << var3 << L" > 50) { return " << var3 << L" * 2; }\n"
                << L"        return " << var4 << L".GetHashCode() % 100;\n";
            break;
        case 2:
            ss << L"        int[] " << var1 << L" = new int[" << (rng() % 10 + 5) << L"];\n"
                << L"        int " << var2 << L" = 0;\n"
                << L"        for (int " << var3 << L" = 0; " << var3 << L" < " << var1 << L".Length; " << var3 << L"++) {\n"
                << L"            " << var1 << L"[" << var3 << L"] = (" << var3 << L" * " << (rng() % 7 + 3) << L") % 256;\n"
                << L"            " << var2 << L" += " << var1 << L"[" << var3 << L"];\n        }\n"
                << L"        if (" << var2 << L" > 1000) { return " << var2 << L" / 10; }\n"
                << L"        return " << var2 << L" % 13;\n";
            break;
        }
        ss << L"    }\n";
        return ss.str();
    }

    wstring generateJunkSubCS() {
        wstring subName = ng->generateFreshName();
        wstring var1 = ng->generateFreshName(), var2 = ng->generateFreshName();
        uniform_int_distribution<int> typeDist(0, 1);
        int type = typeDist(rng);
        wstringstream ss;
        ss << L"    private static void " << subName << L"() {\n";
        switch (type) {
        case 0:
            ss << L"        System.DateTime " << var1 << L" = System.DateTime.Now;\n"
                << L"        System.TimeSpan " << var2 << L" = System.TimeSpan.FromSeconds(" << (rng() % 3600 + 1) << L");\n"
                << L"        System.DateTime v3 = " << var1 << L" + " << var2 << L";\n"
                << L"        if (v3 > " << var1 << L") { int v4 = v3.Second + " << var1 << L".Minute; }\n";
            break;
        case 1:
            ss << L"        int " << var1 << L" = " << (rng() % 100 + 1) << L";\n"
                << L"        int " << var2 << L" = 0;\n"
                << L"        switch (" << var1 << L" % 4) {\n"
                << L"            case 0: " << var2 << L" = " << var1 << L" * 3; break;\n"
                << L"            case 1: " << var2 << L" = " << var1 << L" * 5; break;\n"
                << L"            case 2: " << var2 << L" = " << var1 << L" * 7; break;\n"
                << L"            default: " << var2 << L" = " << var1 << L" * 11; break;\n        }\n";
            break;
        }
        ss << L"    }\n";
        return ss.str();
    }

    wstring generateJunkFunctionCpp() {
        wstring funcName = ng->generateFreshName();
        wstring var1 = ng->generateFreshName(), var2 = ng->generateFreshName(), var3 = ng->generateFreshName();
        uniform_int_distribution<int> typeDist(0, 2);
        int type = typeDist(rng);
        wstringstream ss;
        ss << L"    int " << funcName << L"() {\n";
        switch (type) {
        case 0:
            ss << L"        int " << var1 << L" = " << (rng() % 900 + 100) << L";\n"
                << L"        int " << var2 << L" = " << (rng() % 900 + 100) << L";\n"
                << L"        int " << var3 << L" = (" << var1 << L" + " << var2 << L") * " << (rng() % 10 + 2) << L";\n"
                << L"        if (" << var3 << L" < 0) { return -1; }\n"
                << L"        else if (" << var3 << L" > 999999) { return 1; }\n"
                << L"        return " << var3 << L" % 7;\n";
            break;
        case 1:
            ss << L"        int " << var1 << L"[8];\n"
                << L"        int " << var2 << L" = 0;\n"
                << L"        for (int " << var3 << L" = 0; " << var3 << L" < 8; " << var3 << L"++) {\n"
                << L"            " << var1 << L"[" << var3 << L"] = (" << var3 << L" * " << (rng() % 7 + 3) << L") % 256;\n"
                << L"            " << var2 << L" += " << var1 << L"[" << var3 << L"];\n        }\n"
                << L"        if (" << var2 << L" > 1000) { return " << var2 << L" / 10; }\n"
                << L"        return " << var2 << L" % 13;\n";
            break;
        case 2:
            ss << L"        int " << var1 << L" = 0;\n"
                << L"        for (int " << var2 << L" = 1; " << var2 << L" <= " << (rng() % 5 + 3) << L"; " << var2 << L"++) {\n"
                << L"            for (int " << var3 << L" = 1; " << var3 << L" <= " << (rng() % 5 + 3) << L"; " << var3 << L"++) {\n"
                << L"                " << var1 << L" += " << var2 << L" * " << var3 << L";\n"
                << L"                if (" << var1 << L" > 500) break;\n            }\n"
                << L"            if (" << var1 << L" > 500) break;\n        }\n"
                << L"        return " << var1 << L" % 17;\n";
            break;
        }
        ss << L"    }\n";
        return ss.str();
    }

    wstring generateJunkSubCpp() {
        wstring subName = ng->generateFreshName();
        wstring var1 = ng->generateFreshName(), var2 = ng->generateFreshName();
        uniform_int_distribution<int> typeDist(0, 1);
        int type = typeDist(rng);
        wstringstream ss;
        ss << L"    void " << subName << L"() {\n";
        switch (type) {
        case 0:
            ss << L"        double " << var1 << L" = " << (rng() % 100 + 1) << L"." << (rng() % 99 + 1) << L";\n"
                << L"        double " << var2 << L" = " << var1 << L" * " << var1 << L";\n"
                << L"        if (" << var2 << L" > " << (rng() % 50 + 1) << L") { double v3 = " << var2 << L" - " << var1 << L"; }\n";
            break;
        case 1:
            ss << L"        int " << var1 << L" = " << (rng() % 100 + 1) << L";\n"
                << L"        int " << var2 << L" = 0;\n"
                << L"        switch (" << var1 << L" % 4) {\n"
                << L"            case 0: " << var2 << L" = " << var1 << L" * 3; break;\n"
                << L"            case 1: " << var2 << L" = " << var1 << L" * 5; break;\n"
                << L"            case 2: " << var2 << L" = " << var1 << L" * 7; break;\n"
                << L"            default: " << var2 << L" = " << var1 << L" * 11; break;\n        }\n";
            break;
        }
        ss << L"    }\n";
        return ss.str();
    }

    vector<wstring> generateJunkBlock(int count, Lang lang = Lang::VB) {
        vector<wstring> blocks;
        uniform_int_distribution<int> typeDist(0, 1);
        for (int i = 0; i < count; i++) {
            bool wantFunc = (typeDist(rng) == 0);
            switch (lang) {
            case Lang::CSharp:
                blocks.push_back(wantFunc ? generateJunkFunctionCS() : generateJunkSubCS());
                break;
            case Lang::Cpp:
                blocks.push_back(wantFunc ? generateJunkFunctionCpp() : generateJunkSubCpp());
                break;
            default:
                blocks.push_back(wantFunc ? generateJunkFunction() : generateJunkSub());
                break;
            }
        }
        return blocks;
    }
};

struct PreScanResult {
    vector<wstring> classNames;
    vector<wstring> controlNames;
    vector<wstring> publicNames;
};

// ============================================================================
// OBFUSCATION ENGINE
// ============================================================================
class ObfuscationEngine {
    GlobalNameGenerator globalNG;
    map<wstring, wstring, less<>> classMap, methodMap, fieldMap;
    // Project-wide virtual/abstract declarations are collected before rename
    // extraction so override handling is independent of source-file order.
    set<wstring> projectVirtualMethods;
    set<wstring> preserveSet;
    set<wstring> localClasses;
    // All project-defined type identities, including types intentionally kept
    // unrenamed by attributes such as [StructLayout]. A preserved type name
    // must still resolve its renamed fields through typed receivers.
    set<wstring> localTypeNames;
    set<wstring> localMethods;
    set<wstring> localFields;
    // Unique project declarations that were recognized as identifiers but did
    // not receive a rename. Written to ignored.txt after project extraction.
    set<wstring> ignoredSymbols;
    set<wstring> controlNames;
    wstring source;
    vector<wstring> lines;
    Lang lang;

    // ------------------------------------------------------------------
    // FIX (VB case-insensitivity): VB.NET identifiers are case-insensitive,
    // but classMap/methodMap/fieldMap are plain case-SENSITIVE maps keyed on
    // whatever spelling was first seen at the declaration site. A usage site
    // that capitalizes the name differently ("Counter" vs "counter") used to
    // miss the map entirely and stay un-renamed, leaving a dangling reference
    // to a name that no longer exists after the declaration got renamed.
    //
    // vbCanonicalCase maps lower(name) -> the exact spelling under which the
    // one real classMap/methodMap/fieldMap entry for that symbol lives. Every
    // VB token is normalized through canonicalizeVBToken() before any of the
    // ordinary (case-sensitive) map/set lookups run, so any casing variant of
    // a renamed symbol resolves to the same entry. This only applies when
    // lang == Lang::VB; C#/C++ remain case-sensitive as the language requires.
    map<wstring, wstring, less<>> vbCanonicalCase;

    void registerVBCanonicalCase(const wstring& name) {
        if (lang != Lang::VB) return;
        vbCanonicalCase[toLowerW(name)] = name;
    }

    wstring canonicalizeVBToken(const wstring& token) const {
        if (lang != Lang::VB) return token;
        auto it = vbCanonicalCase.find(toLowerW(token));
        return (it != vbCanonicalCase.end()) ? it->second : token;
    }

    // Every "<map>[name] = globalNG.getObfuscatedName(name);" site should
    // route through here instead, so the case-insensitive index always stays
    // in sync with what's actually renameable. (Call sites that alias an
    // EXISTING obfuscated name, like registerAttributeAlias, call
    // registerVBCanonicalCase directly since there is no fresh name to mint.)
    wstring registerRename(map<wstring, wstring, less<>>& m, const wstring& name) {
        wstring obf = globalNG.getObfuscatedName(name);
        m[name] = obf;
        registerVBCanonicalCase(name);
        return obf;
    }

public:
    struct SourceUnit {
        wstring text;
        wstring label;
        Lang lang;
        vector<wstring> lines;
    };
    vector<SourceUnit> sources;

    static vector<wstring> splitLinesOf(const wstring& text) {
        vector<wstring> out;
        wstringstream ss(text);
        wstring line;
        while (getline(ss, line)) {
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            out.push_back(line);
        }
        return out;
    }

    void addSource(const wstring& src, const wstring& label, Lang fileLang) {
        SourceUnit u;
        u.text = src;
        u.label = label;
        u.lang = fileLang;
        u.lines = splitLinesOf(src);
        sources.push_back(std::move(u));
    }

    void setActiveLang(Lang l) { lang = l; }
    Lang getActiveLang() const { return lang; }

private:

    bool inBlockComment = false;
    bool inDirectiveContinuation = false;
    // Receiver types are scoped per source file. A name such as "item" may
    // have unrelated types in different files; a project-global name-only map
    // incorrectly marked both files ambiguous and prevented valid member rewrites.
    map<pair<wstring, wstring>, wstring> sourceVarToType;
    set<pair<wstring, wstring>> ambiguousSourceVarTypes;
    // Cross-file fallback for uniquely named project variables/fields. A receiver
    // declared in one source unit may be referenced in another; the old strictly
    // per-file lookup then treated its type as unknown and left struct members
    // (for example DATA_BLOB.cbData / DATA_BLOB.pbData) unchanged. Keep this
    // fallback name-only but fail closed whenever the same identifier is observed
    // with conflicting types anywhere in the project. Same-file information always
    // takes precedence, so ordinary shadowing remains source-scoped.
    map<wstring, wstring> projectVarToType;
    set<wstring> ambiguousProjectVarTypes;
    wstring activeSourceLabel;
    vector<wstring> callTargetStack;
    vector<wstring> callTargetTypeStack;
    // Type-scoped members prevent a same-spelled field in an unrelated class
    // from authorizing a label rewrite. Keys are original declaration names.
    map<wstring, set<wstring>> classMembersByType;
    map<wstring, map<wstring, set<wstring>>> methodParamsByType;
    map<wstring, set<wstring>> methodOwnerTypes;
    // One brace-context entry per C# '{'. Empty entries represent ordinary
    // blocks; nonempty entries identify an object/property-pattern target type.
    vector<wstring> csharpBraceTargetTypes;
    vector<wstring> csharpClassScopeStack;
    wstring pendingCSharpBraceTargetType;
    vector<wstring> vbWithTypeStack;
    int csharpAttributeBracketDepth = 0;
    vector<bool> csharpBracketIsAttribute;

    // Anonymous-type properties have declaration identity scoped to the
    // inferred local variable, not a project-global field spelling.
    map<wstring, map<wstring, wstring>> anonymousMemberRenames;

    // FIX (gap 5): bare (pre-rename) name of a method/indexer declaration ->
    // the ordinal (0-based) position of its [CallerMemberName] parameter.
    // Populated by prescanCallerMemberName(), consumed by
    // protectCallerMemberNameCallers() -- see both below.
    map<wstring, int, less<>> callerMemberNameParamIndex;

    set<wstring> preservedQtSignalsSlots;

public:
    bool excludeClassNames = false;
    bool excludeControlNames = false;
    bool excludePublicDeclarations = false;

    bool preserveMacros = true;
    bool preserveLabels = true;
    bool preserveOperators = true;
    bool preserveNamespaces = true;
    bool preserveTypedefs = true;
    bool preserveTemplates = true;
    bool skipPreprocessorZero = true;

    bool enableStringEncryption = false;
    bool shouldInjectDecryptModule = false;
    bool enableStringExclusion = false;
    bool excludeAlreadyEncrypted = true;
    vector<pair<wstring, wstring>> stringEncryptionLog;
    vector<wstring> stringExclusionList;

    bool enableDeadCode = false;
    bool mixDeadCode = false;
    int  deadCodeCount = 0;

    bool skipExternalMembers = false;

    void setCharset(ObfuscationCharset cs) { globalNG.setCharset(cs); }

private:
    set<wstring> preservedMacroParams;
    set<wstring> preservedLabels;
    set<wstring> preservedOperators;
    set<wstring> preservedNamespaces;
    set<wstring> preservedTypedefs;
    set<wstring> preservedTemplateParams;
    set<wstring> preservedAttributes;
    set<wstring> preservedConcepts;
    map<wstring, wstring, less<>> funcReturnTypes;
    // FIX: method name (bare, pre-rename) -> the set of its own declared parameter
    // names (also bare/pre-rename). Needed so a named-argument label at a call site
    // can be checked against the SPECIFIC resolved declaration it targets, rather
    // than "is this name a parameter of ANY method/lambda/indexer anywhere in the
    // project" (the old flat fieldMap/localFields check) -- two unrelated methods
    // can easily share a common parameter name like "value" or "index", and a
    // label must agree with the one declaration the call actually resolves to.
    map<wstring, set<wstring>> methodParams;

    bool inIfZero = false;
    int  ifZeroDepth = 0;
    bool inRawString = false;
    wstring rawStringEndSeq;

public:
    ObfuscationEngine(const wstring& src, Lang language = Lang::VB)
        : source(src), lang(language) {
        splitLines();
        addSource(src, L"", language);
    }

    void addPreserveList(const vector<wstring>& names) {
        for (const auto& n : names) {
            wstring trimmed = trimW(n);
            if (!trimmed.empty()) preserveSet.insert(trimmed);
        }
    }

    void prescanQtSignalsSlots(const string& src) {
        try {
            regex sectionRe(R"(\b(?:signals|slots|Q_SIGNALS|Q_SLOTS)\s*:)");
            for (sregex_iterator it(src.begin(), src.end(), sectionRe), end; it != end; ++it) {
                size_t pos = (size_t)(it->position() + it->length());
                int parenDepth = 0;
                for (size_t i = pos; i < src.size(); i++) {
                    char c = src[i];
                    if (c == '(') parenDepth++;
                    else if (c == ')') { if (parenDepth > 0) parenDepth--; }
                    else if ((c == '{' || c == '}') && parenDepth == 0) break;
                    if (isalpha((unsigned char)c) || c == '_') {
                        size_t s = i;
                        while (i < src.size() && (isalnum((unsigned char)src[i]) || src[i] == '_')) i++;
                        string word = src.substr(s, i - s);
                        size_t j = i;
                        while (j < src.size() && isspace((unsigned char)src[j])) j++;
                        if (j < src.size() && src[j] == '(') {
                            wstring w = stringToWstring(word);
                            if (!w.empty() && !isReservedHere(w)) {
                                preservedQtSignalsSlots.insert(w);
                                preserveSet.insert(w);
                            }
                        }
                        if (i > pos) i--;
                    }
                }
            }

            regex qSlotRe(R"(\b(?:Q_SLOT|Q_SIGNAL)\b\s+[\w:<>,\s\*&]+?([A-Za-z_]\w*)\s*\()");
            for (sregex_iterator it(src.begin(), src.end(), qSlotRe), end; it != end; ++it) {
                wstring w = stringToWstring((*it)[1]);
                if (!w.empty() && !isReservedHere(w)) {
                    preservedQtSignalsSlots.insert(w);
                    preserveSet.insert(w);
                }
            }

            regex qPropBlockRe(R"(\bQ_PROPERTY\s*\(([^)]*)\))");
            for (sregex_iterator it(src.begin(), src.end(), qPropBlockRe), end; it != end; ++it) {
                string inner = (*it)[1].str();
                regex nameRe(R"(^\s*[^,)]+?\s+([A-Za-z_]\w*)\s*(?:$|,))");
                smatch nm;
                if (regex_search(inner, nm, nameRe)) {
                    wstring w = stringToWstring(nm[1].str());
                    if (!w.empty() && !isReservedHere(w)) {
                        preservedQtSignalsSlots.insert(w);
                        preserveSet.insert(w);
                    }
                }
                regex accessorRe(R"(\b(?:READ|WRITE|MEMBER|NOTIFY)\s+([A-Za-z_]\w*))");
                for (sregex_iterator ait(inner.begin(), inner.end(), accessorRe), aend; ait != aend; ++ait) {
                    wstring w = stringToWstring((*ait)[1]);
                    if (!w.empty() && !isReservedHere(w)) {
                        preservedQtSignalsSlots.insert(w);
                        preserveSet.insert(w);
                    }
                }
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: Qt signals/slots prescan failed", CLR_WARN); }
    }

    const set<wstring>& getLocalClasses() const { return localClasses; }
    const set<wstring>& getLocalMethods() const { return localMethods; }
    const set<wstring>& getLocalFields() const { return localFields; }
    const set<wstring>& getIgnoredSymbols() const { return ignoredSymbols; }

    bool writeIgnoredSymbols(const wstring& path) const {
        ofstream out(wstringToString(path), ios::binary | ios::trunc);
        if (!out) return false;
        // UTF-8 BOM keeps the report readable in Windows Notepad and editors.
        const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
        out.write(reinterpret_cast<const char*>(bom), sizeof(bom));
        for (const auto& name : ignoredSymbols) {
            string line = wstringToString(name) + "\r\n";
            out.write(line.data(), (streamsize)line.size());
        }
        return (bool)out;
    }

    const map<wstring, wstring, less<>>& getClassMap() const { return classMap; }
    const map<wstring, wstring, less<>>& getMethodMap() const { return methodMap; }
    const map<wstring, wstring, less<>>& getFieldMap() const { return fieldMap; }
    const set<wstring>& getPreserveSet() const { return preserveSet; }

    // ------------------------------------------------------------------
    // FIX (gap 4): nameof arguments are source identifier tokens, not strings,
    // and therefore flow through the ordinary token-renaming branches below.
    // The requested regression inputs are nameof(LocalClass.LocalMethod),
    // nameof(localField), and nameof(SomeFrameworkType). This translation unit
    // does not bundle or execute a standalone regression harness, so these exact
    // rendered outputs still require a build-and-run test in the Windows app.
    // Do not interpret this explanation as a verified test result.
    //
    // Run AFTER obfuscate(), on the already-renamed output. Identifier
    // references (including inside nameof(...)) are renamed correctly by
    // the normal token-rename pass, since nameof's argument is a real
    // identifier token, not a string. What that pass can never safely
    // touch is a plain string LITERAL that happens to spell out a member's
    // ORIGINAL name for a runtime, name-by-string lookup -- rewriting
    // arbitrary string contents on a guess would corrupt unrelated text,
    // so those call sites need their own explicit, narrowly-scoped patch:
    // [JsonProperty("Foo")], [DataMember(Name = "Foo")], [XmlElement("Foo")],
    // new Binding("Foo"), new PropertyPath("Foo"), FindResource("Foo"),
    // TryFindResource("Foo"), and a raw-string (non-nameof)
    // DependencyProperty.Register("Foo", ...) first argument. Each pattern
    // below captures just the bare identifier inside the quotes and looks
    // it up by its ORIGINAL spelling across all three rename maps,
    // replacing it with the obfuscated name when found -- everything else
    // in the string, and every other string in the file, is left alone.
    // ------------------------------------------------------------------
    wstring patchReflectionStrings(const wstring& code) {
        wstring out = code;
        auto lookupAny = [&](const wstring& name, wstring& obf) -> bool {
            auto qf = fieldMap.find(name); if (qf != fieldMap.end()) { obf = qf->second; return true; }
            auto qm = methodMap.find(name); if (qm != methodMap.end()) { obf = qm->second; return true; }
            auto qc = classMap.find(name); if (qc != classMap.end()) { obf = qc->second; return true; }
            return false;
            };
        auto patchOne = [&](const wregex& re) {
            wstring result; size_t last = 0;
            try {
                for (wsregex_iterator it(out.begin(), out.end(), re), end; it != end; ++it) {
                    size_t gs = (size_t)(*it).position(1);
                    wstring name = (*it)[1].str();
                    result.append(out, last, gs - last);
                    wstring obf;
                    result += lookupAny(name, obf) ? obf : name;
                    last = gs + name.size();
                }
                result.append(out, last, out.size() - last);
                out.swap(result);
            }
            catch (const regex_error&) { LogMessage(L"WARN: reflection-string patch: a pattern failed to compile/run.", CLR_WARN); }
            };
        patchOne(wregex(LR"XN(\[\s*JsonProperty\s*\(\s*"([A-Za-z_]\w*)")XN"));
        patchOne(wregex(LR"XN(\[\s*DataMember\s*\(\s*Name\s*=\s*"([A-Za-z_]\w*)")XN"));
        patchOne(wregex(LR"XN(\[\s*XmlElement\s*\(\s*"([A-Za-z_]\w*)")XN"));
        // FIX: XmlElement's siblings use the exact same [Attr("Name")] shape and
        // were simply never added alongside it.
        patchOne(wregex(LR"XN(\[\s*XmlRoot\s*\(\s*"([A-Za-z_]\w*)")XN"));
        patchOne(wregex(LR"XN(\[\s*XmlAttribute\s*\(\s*"([A-Za-z_]\w*)")XN"));
        patchOne(wregex(LR"XN(\[\s*XmlArray\s*\(\s*"([A-Za-z_]\w*)")XN"));
        patchOne(wregex(LR"XN(\bnew\s+Binding\s*\(\s*"([A-Za-z_]\w*)")XN"));
        patchOne(wregex(LR"XN(\bnew\s+PropertyPath\s*\(\s*"([A-Za-z_]\w*)")XN"));
        patchOne(wregex(LR"XN(\b(?:Try)?FindResource\s*\(\s*"([A-Za-z_]\w*)")XN"));
        patchOne(wregex(LR"XN(\bDependencyProperty\s*\.\s*Register\s*\(\s*"([A-Za-z_]\w*)")XN"));
        // FIX: the classic pre-nameof() INotifyPropertyChanged pattern. Applies
        // the same in C# and VB.NET -- it's just a method call with a string
        // literal argument, no language-specific syntax involved.
        patchOne(wregex(LR"XN(\b(?:On|Raise)PropertyChanged\s*\(\s*"([A-Za-z_]\w*)")XN"));

        // FIX (VB coverage): every pattern above assumes C# syntax -- square-
        // bracket attributes and "Name=" -- which never appears in real VB.NET
        // source. VB attributes use angle brackets and ":=" for named
        // arguments, and VB's reflection-by-string call is CallByName, which
        // has no C# equivalent at all and previously had zero coverage here.
        if (lang == Lang::VB) {
            patchOne(wregex(LR"XN(\<\s*JsonProperty\s*\(\s*"([A-Za-z_]\w*)")XN"));
            patchOne(wregex(LR"XN(\<\s*DataMember\s*\(\s*Name\s*:=\s*"([A-Za-z_]\w*)")XN"));
            patchOne(wregex(LR"XN(\<\s*XmlElement\s*\(\s*"([A-Za-z_]\w*)")XN"));
            patchOne(wregex(LR"XN(\<\s*XmlRoot\s*\(\s*"([A-Za-z_]\w*)")XN"));
            patchOne(wregex(LR"XN(\<\s*XmlAttribute\s*\(\s*"([A-Za-z_]\w*)")XN"));
            patchOne(wregex(LR"XN(\<\s*XmlArray\s*\(\s*"([A-Za-z_]\w*)")XN"));
            patchOne(wregex(LR"XN(\bNew\s+Binding\s*\(\s*"([A-Za-z_]\w*)")XN"));
            patchOne(wregex(LR"XN(\bNew\s+PropertyPath\s*\(\s*"([A-Za-z_]\w*)")XN"));
            // CallByName(obj, "MemberName", CallType, args) -- patch the
            // member-name string argument (the second parameter) only.
            patchOne(wregex(LR"XN(\bCallByName\s*\(\s*[^,"()]+,\s*"([A-Za-z_]\w*)")XN"));
        }
        return out;
    }

    wstring patchResolvedLabels(const wstring& code) {
        wstring out = code;
        if (lang == Lang::CSharp && !anonymousMemberRenames.empty()) {
            try {
                auto esc = [](const wstring& x) -> wstring {
                    wstring r;
                    for (wchar_t c : x) {
                        if (c == L'\\' || c == L'.' || c == L'+' || c == L'*' || c == L'?' ||
                            c == L'^' || c == L'$' || c == L'(' || c == L')' || c == L'[' ||
                            c == L']' || c == L'{' || c == L'}' || c == L'|') r += L'\\';
                        r += c;
                    }
                    return r;
                    };
                for (const auto& variableEntry : anonymousMemberRenames) {
                    const wstring& originalVar = variableEntry.first;
                    auto vit = fieldMap.find(originalVar);
                    wstring outputVar = vit == fieldMap.end() ? originalVar : vit->second;
                    wregex declRe(LR"(\bvar\s+)" + esc(outputVar) + LR"(\s*=\s*new\s*\{([\s\S]*?)\})");
                    wstring rebuilt;
                    size_t last = 0;
                    for (wsregex_iterator di(out.begin(), out.end(), declRe), de; di != de; ++di) {
                        size_t bodyStart = (size_t)di->position(1);
                        size_t bodyLen = (size_t)di->length(1);
                        wstring body = (*di)[1].str();
                        for (const auto& prop : variableEntry.second) {
                            vector<wstring> candidates = { prop.first };
                            auto fm = fieldMap.find(prop.first);
                            if (fm != fieldMap.end() && fm->second != prop.first) candidates.push_back(fm->second);
                            for (const auto& candidate : candidates) {
                                wregex labelRe(LR"(\b)" + esc(candidate) + LR"((\s*=))");
                                body = regex_replace(body, labelRe, prop.second + L"$1");
                            }
                        }
                        rebuilt.append(out, last, bodyStart - last);
                        rebuilt += body;
                        last = bodyStart + bodyLen;
                    }
                    if (!rebuilt.empty()) { rebuilt.append(out, last, out.size() - last); out.swap(rebuilt); }
                    for (const auto& prop : variableEntry.second) {
                        vector<wstring> candidates = { prop.first };
                        auto fm = fieldMap.find(prop.first);
                        if (fm != fieldMap.end() && fm->second != prop.first) candidates.push_back(fm->second);
                        for (const auto& candidate : candidates) {
                            wregex accessRe(LR"(\b)" + esc(outputVar) + LR"((\s*\.\s*))" + esc(candidate) + LR"(\b)");
                            out = regex_replace(out, accessRe, outputVar + L"$1" + prop.second);
                        }
                    }
                }
            }
            catch (const regex_error&) { LogMessage(L"WARN: anonymous-type resolved-label patch regex failed; affected properties are left unchanged.", CLR_WARN); }
        }
        return out;
    }

    PreScanResult preScan() {
        PreScanResult merged;
        for (auto& u : sources) {
            source = u.text; lines = u.lines; lang = u.lang;
            PreScanResult r = preScanOneFile();
            merged.classNames.insert(merged.classNames.end(), r.classNames.begin(), r.classNames.end());
            merged.controlNames.insert(merged.controlNames.end(), r.controlNames.begin(), r.controlNames.end());
            merged.publicNames.insert(merged.publicNames.end(), r.publicNames.begin(), r.publicNames.end());
        }
        return merged;
    }

    void extractIdentifiers() {
        // Pass 0: discover virtual/abstract base methods across every project
        // source before any override is considered for renaming. Interface
        // methods are implicitly abstract and are included as well.
        projectVirtualMethods.clear();
        for (const auto& unit : sources) {
            if (unit.lang != Lang::CSharp) continue;
            const string src = wstringToString(unit.text);
            try {
                static const regex virtualMethodRe(
                    R"(\b(?:virtual|abstract)\s+(?:[A-Za-z_]\w*(?:\s*<[^;{}()]*>)?\s+)*([A-Za-z_]\w*)\s*\()");
                for (sregex_iterator it(src.begin(), src.end(), virtualMethodRe), end; it != end; ++it)
                    projectVirtualMethods.insert(stringToWstring((*it)[1].str()));

                static const regex interfaceRe(R"(\binterface\s+[A-Za-z_]\w*(?:\s*<[^>{}]*>)?\s*\{)");
                static const regex interfaceMethodRe(
                    R"(\b[A-Za-z_]\w*(?:\s*<[^;{}()]*>)?(?:\[\])?\s+([A-Za-z_]\w*)\s*\([^;{}]*\)\s*;)");
                for (sregex_iterator it(src.begin(), src.end(), interfaceRe), end; it != end; ++it) {
                    size_t open = (size_t)(it->position() + it->length() - 1);
                    size_t close = open;
                    int depth = 0;
                    for (; close < src.size(); ++close) {
                        if (src[close] == '{') ++depth;
                        else if (src[close] == '}' && --depth == 0) break;
                    }
                    if (close <= open || close >= src.size()) continue;
                    string body = src.substr(open + 1, close - open - 1);
                    for (sregex_iterator mi(body.begin(), body.end(), interfaceMethodRe), mend; mi != mend; ++mi)
                        projectVirtualMethods.insert(stringToWstring((*mi)[1].str()));
                }
            }
            catch (const regex_error&) {
                LogMessage(L"WARN: virtual/interface pre-scan failed; uncertain overrides will be preserved.", CLR_WARN);
            }
        }

        for (auto& u : sources) {
            source = u.text; lines = u.lines; lang = u.lang; activeSourceLabel = u.label;
            extractIdentifiersOneFile();
        }

        // Build the type -> member ownership index after every source unit has
        // populated fieldMap/localFields. Static references such as
        // Data.SPL_SOCKET therefore resolve independently of file order.
        prescanProjectClassMembers();
        collectIgnoredProjectDeclarations();
    }

private:
    void collectIgnoredProjectDeclarations() {
        ignoredSymbols.clear();
        static const set<wstring> extraNoise = {
            L"public", L"private", L"protected", L"internal", L"friend", L"static",
            L"readonly", L"const", L"volatile", L"virtual", L"override", L"abstract",
            L"sealed", L"partial", L"async", L"unsafe", L"extern", L"required",
            L"void", L"var", L"int", L"string", L"bool", L"double", L"float",
            L"long", L"short", L"byte", L"char", L"decimal", L"object", L"dynamic",
            L"class", L"struct", L"interface", L"enum", L"record", L"event", L"property",
            L"get", L"set", L"init", L"new", L"return", L"if", L"else", L"for", L"foreach",
            L"while", L"switch", L"case", L"break", L"continue", L"throw", L"try", L"catch",
            L"using", L"namespace", L"this", L"base", L"true", L"false", L"null"
        };
        auto wasRenamed = [&](const wstring& n) {
            return classMap.count(n) || methodMap.count(n) || fieldMap.count(n);
            };
        auto consider = [&](const wstring& raw) {
            wstring n = trimW(raw);
            if (n.empty() || extraNoise.count(n) || wasRenamed(n) || isReservedHere(n)) return;
            // Do not report type keywords or generic language syntax as skipped symbols.
            if (isStatementWord(n, lang)) return;
            ignoredSymbols.insert(n);
            };

        for (const auto& u : sources) {
            lang = u.lang;
            string src = wstringToString(u.text);
            try {
                if (lang == Lang::CSharp) {
                    static const regex typeDecl(R"(\b(?:class|struct|interface|enum|record)\s+([A-Za-z_]\w*))");
                    for (sregex_iterator i(src.begin(), src.end(), typeDecl), e; i != e; ++i)
                        consider(stringToWstring((*i)[1]));
                    // Line-oriented declaration candidates: fields, properties, events,
                    // and methods. Restrict to declaration-shaped lines to avoid listing
                    // arbitrary framework/member references from method bodies.
                    static const regex memberDecl(
                        R"(^\s*(?:(?:public|private|protected|internal|static|readonly|const|volatile|required|new|virtual|override|abstract|sealed|extern|async|unsafe|partial|event)\s+)*(?:[A-Za-z_]\w*(?:\s*<[^;{}()]*>)?(?:\[\])?[?*&]?\s+)([A-Za-z_]\w*)\s*(?=\(|\{|=>|=|;))");
                    istringstream ss(src); string line;
                    while (getline(ss, line)) {
                        smatch m;
                        if (regex_search(line, m, memberDecl)) consider(stringToWstring(m[1]));
                    }
                }
                else if (lang == Lang::VB) {
                    static const regex typeDecl(R"(\b(?:Class|Structure|Interface|Enum|Module)\s+([A-Za-z_]\w*))", regex::icase);
                    static const regex memberDecl(R"(^\s*(?:(?:Public|Private|Protected|Friend|Shared|ReadOnly|WriteOnly|Overrides|Overridable|Shadows|Overloads|Partial|Async|Default|Dim|Event|Property)\s+)*([A-Za-z_]\w*)\s*(?:As\b|\())", regex::icase);
                    for (sregex_iterator i(src.begin(), src.end(), typeDecl), e; i != e; ++i) consider(stringToWstring((*i)[1]));
                    istringstream ss(src); string line;
                    while (getline(ss, line)) { smatch m; if (regex_search(line, m, memberDecl)) consider(stringToWstring(m[1])); }
                }
                else {
                    static const regex typeDecl(R"(\b(?:class|struct|enum|union|namespace)\s+([A-Za-z_]\w*))");
                    static const regex memberDecl(R"(^\s*(?:(?:public|private|protected|static|virtual|inline|constexpr|const|extern|friend)\s+)*(?:[A-Za-z_]\w*(?:\s*<[^;{}()]*>)?(?:\s*[*&]+)?\s+)([A-Za-z_]\w*)\s*(?=\(|;|=|\{))");
                    for (sregex_iterator i(src.begin(), src.end(), typeDecl), e; i != e; ++i) consider(stringToWstring((*i)[1]));
                    istringstream ss(src); string line;
                    while (getline(ss, line)) { smatch m; if (regex_search(line, m, memberDecl)) consider(stringToWstring(m[1])); }
                }
            }
            catch (const regex_error&) {
                LogMessage(L"WARN: ignored.txt declaration scan failed for " + u.label, CLR_WARN);
            }
        }
    }

    PreScanResult preScanOneFile() {
        if (lang == Lang::VB) return preScanVB();
        return preScanCLike();
    }

    void extractIdentifiersOneFile() {
        if (lang == Lang::VB) extractIdentifiersVB();
        else extractIdentifiersCLike();

        try {
            string srcUtf8b = wstringToString(source);
            bool crossTU = false;
            static const regex externCRe(R"(\bextern\s*"C"\b)");
            static const regex dllExportRe(R"(__declspec\s*\(\s*dllexport\s*\))");
            static const regex dllImportAttrRe(R"(\[\s*DllImport\b)");
            static const regex internalsVisibleRe(R"(\[\s*assembly\s*:\s*InternalsVisibleTo\b)");
            static const regex quotedIncludeRe(R"(^\s*#\s*include\s*"[^"]+"\s*$)");
            static const regex projectRefRe(R"(<\s*ProjectReference|ProjectReference\s*=)");
            static const regex usingNsRe(R"(\busing\s+([A-Za-z_][\w.]*)\s*;)");
            static const set<string> kSystemNsPrefixes = {
                "System", "Microsoft", "Windows", "std"
            };
            if (regex_search(srcUtf8b, externCRe) || regex_search(srcUtf8b, dllExportRe) ||
                regex_search(srcUtf8b, dllImportAttrRe) || regex_search(srcUtf8b, internalsVisibleRe) ||
                regex_search(srcUtf8b, projectRefRe)) {
                crossTU = true;
            }
            if (!crossTU) {
                for (const auto& ln : lines) {
                    string lnUtf8 = wstringToString(ln);
                    if (regex_search(lnUtf8, quotedIncludeRe)) { crossTU = true; break; }
                }
            }
            if (!crossTU && lang == Lang::CSharp) {
                smatch um;
                string::const_iterator s0(srcUtf8b.cbegin());
                while (regex_search(s0, srcUtf8b.cend(), um, usingNsRe)) {
                    string ns = um[1].str();
                    string head = ns.substr(0, ns.find('.'));
                    if (!kSystemNsPrefixes.count(head)) { crossTU = true; break; }
                    s0 = um.suffix().first;
                }
            }
            if (crossTU) {
                LogMessage(L"WARN: this file declares or consumes external symbols. "
                    L"Single-file obfuscation will break references from other files/assemblies. "
                    L"Use whole-project mode when available, or obfuscate all related files together.", CLR_WARN);
            }
        }
        catch (const regex_error&) {}
    }

public:
    wstring obfuscateOne(size_t index) {
        source = sources[index].text;
        lines = sources[index].lines;
        lang = sources[index].lang;
        activeSourceLabel = sources[index].label;

        wstringstream result;
        inBlockComment = false;
        inDirectiveContinuation = false;
        inIfZero = false;
        ifZeroDepth = 0;
        inRawString = false;
        rawStringEndSeq.clear();
        callTargetStack.clear();
        callTargetTypeStack.clear();
        csharpBraceTargetTypes.clear();
        csharpClassScopeStack.clear();
        pendingCSharpBraceTargetType.clear();
        vbWithTypeStack.clear();
        csharpAttributeBracketDepth = 0;
        csharpBracketIsAttribute.clear();

        for (size_t i = 0; i < lines.size(); i++) {
            const wstring& line = lines[i];
            wstring trimmed = trimW(line);

            if (inIfZero) {
                if (!trimmed.empty() && trimmed[0] == L'#') {
                    wstring rest = trimW(trimmed.substr(1));
                    wstring dir = firstWordOf(rest);
                    if (iequals(dir, L"if") || iequals(dir, L"ifdef") || iequals(dir, L"ifndef"))
                        ifZeroDepth++;
                    else if (iequals(dir, L"endif")) {
                        ifZeroDepth--;
                        if (ifZeroDepth <= 0) inIfZero = false;
                    }
                }
                result << line << L"\n";
                continue;
            }

            if (skipPreprocessorZero && !inBlockComment &&
                !trimmed.empty() && trimmed[0] == L'#') {
                wstring rest = trimW(trimmed.substr(1));
                if (iequals(firstWordOf(rest), L"if")) {
                    wstring cond = trimW(rest.substr(2));
                    size_t c1 = cond.find(L"//");
                    if (c1 != wstring::npos) cond = trimW(cond.substr(0, c1));
                    size_t c2 = cond.find(L"/*");
                    if (c2 != wstring::npos) cond = trimW(cond.substr(0, c2));
                    while (cond.size() >= 2 && cond.front() == L'(' && cond.back() == L')')
                        cond = trimW(cond.substr(1, cond.size() - 2));
                    if (cond == L"0") {
                        inIfZero = true;
                        ifZeroDepth = 1;
                        result << line << L"\n";
                        continue;
                    }
                }
            }

            if (!inBlockComment && trimmed.empty()) { result << line << L"\n"; continue; }
            if (!inBlockComment && lang == Lang::VB && trimmed[0] == L'\'') { result << line << L"\n"; continue; }
            if (!inBlockComment && (lang == Lang::CSharp || lang == Lang::Cpp) &&
                trimmed.size() >= 2 && trimmed[0] == L'/' && trimmed[1] == L'/') {
                result << line << L"\n"; continue;
            }

            if (inDirectiveContinuation) {
                result << line << L"\n";
                wstring rtrim = line;
                while (!rtrim.empty() && iswspace(rtrim.back())) rtrim.pop_back();
                if (rtrim.empty() || rtrim.back() != L'\\') inDirectiveContinuation = false;
                continue;
            }

            if (!trimmed.empty() && trimmed[0] == L'#') {
                wstring rest = trimW(trimmed.substr(1));
                wstring dir = firstWordOf(rest);
                if (iequals(dir, L"ifdef") || iequals(dir, L"ifndef") || iequals(dir, L"undef")) {
                    wstring arg = trimW(rest.substr(dir.size()));
                    size_t sp = arg.find_first_of(L" \t/");
                    if (sp != wstring::npos) arg = arg.substr(0, sp);
                    if (!arg.empty()) preserveSet.insert(arg);
                }
                result << line << L"\n";
                wstring rtrim = line;
                while (!rtrim.empty() && iswspace(rtrim.back())) rtrim.pop_back();
                if (!rtrim.empty() && rtrim.back() == L'\\') inDirectiveContinuation = true;
                continue;
            }

            result << processLine(line) << L"\n";
        }
        return result.str();
    }

    wstring obfuscate() { return obfuscateOne(0); }

    wstring encryptStringsVB(const wstring& input) {
        if (!enableStringEncryption) return input;
        if (lang != Lang::VB) return encryptStringsCFamily(input);
        StringEncryptionHelper helper(&globalNG);
        set<wstring, less<>> exclusionSet;
        if (enableStringExclusion)
            for (const auto& ex : stringExclusionList) exclusionSet.insert(toLowerW(ex));

        vector<wstring> srcLines;
        { wstringstream ss(input); wstring l; while (getline(ss, l)) { if (!l.empty() && l.back() == L'\r') l.pop_back(); srcLines.push_back(l); } }

        vector<wstring> result;
        bool inMethod = false;
        vector<wstring> methodLines;
        vector<tuple<wstring, wstring>> topDeclarations;
        vector<tuple<wstring, wstring>> decryptCalls;
        vector<pair<size_t, wstring>> stringReplacements;
        wstring methodIndent;
        int resourceSkips = 0;

        wregex methodStartRe(L"^\\s*(?:Public|Private|Friend|Protected)\\s+(?:Shared\\s+|Overrides\\s+|Overridable\\s+|MustOverride\\s+|Overloads\\s+|Shadows\\s+|Iterator\\s+|Async\\s+|Partial\\s+)*(?:Sub|Function)\\s+\\w+", regex::icase);
        wregex methodEndRe(L"^\\s*End\\s+(?:Sub|Function)\\b", regex::icase);
        auto isBase64Encoded = [](const wstring& str) -> bool {
            if (str.length() < 4) return false;
            for (wchar_t c : str) if (!(iswalnum(c) || c == L'+' || c == L'/' || c == L'=')) return false;
            return (str.length() % 4 == 0);
            };
        auto isAlreadyEncrypted = [&](const wstring& content) -> bool {
            if (!excludeAlreadyEncrypted) return false;
            if (isBase64Encoded(content)) return true;
            if (content.length() > 20) {
                size_t firstEq = content.find(L'=');
                if (firstEq != wstring::npos) {
                    bool tailOnly = true;
                    for (size_t i = firstEq; i < content.size(); i++) {
                        if (content[i] != L'=') { tailOnly = false; break; }
                    }
                    if (tailOnly && (content.size() - firstEq) <= 2 && isBase64Encoded(content))
                        return true;
                }
            }
            return false;
            };
        auto flushMethod = [&]() {
            if (methodLines.empty()) return;
            wstringstream out;
            out << methodLines[0] << L"\n";
            for (const auto& decl : topDeclarations)
                out << methodIndent << L"Dim " << get<0>(decl) << L" As String = \"" << get<1>(decl) << L"\"\n";
            for (const auto& call : decryptCalls)
                out << methodIndent << L"Dim " << get<0>(call) << L" As String = mxcncbs.ertrymn(" << get<1>(call) << L")\n";
            for (size_t i = 1; i < methodLines.size(); i++) {
                wstring modifiedLine = methodLines[i];
                for (const auto& r : stringReplacements) if (r.first == i) { modifiedLine = r.second; break; }
                out << modifiedLine << L"\n";
            }
            result.push_back(out.str());
            methodLines.clear(); topDeclarations.clear(); decryptCalls.clear(); stringReplacements.clear(); methodIndent.clear();
            };

        for (size_t idx = 0; idx < srcLines.size(); idx++) {
            wstring line = srcLines[idx];
            wstring trimmed = trimW(line);
            if (trimmed.empty() || trimmed[0] == L'\'' || trimmed[0] == L'#' ||
                trimmed.find(L"Imports ") == 0 || trimmed.find(L"<") == 0 || trimmed.find(L"DllImport") == 0) {
                if (inMethod) methodLines.push_back(line); else result.push_back(line);
                continue;
            }
            bool isMethodStart = regex_search(trimmed, methodStartRe);
            bool isMethodEnd = regex_search(trimmed, methodEndRe);
            if (isMethodStart && !inMethod) {
                inMethod = true;
                methodIndent = line.substr(0, line.find_first_not_of(L" \t")) + L"    ";
                methodLines.push_back(line);
                continue;
            }
            if (isMethodEnd && inMethod) {
                methodLines.push_back(line);
                flushMethod();
                result.push_back(line);
                inMethod = false;
                continue;
            }
            if (!inMethod) { result.push_back(line); continue; }

            bool skipLineForResources = false;
            if (line.find(L"resources.GetObject") != wstring::npos ||
                line.find(L"ApplyResources") != wstring::npos ||
                line.find(L"GetString") != wstring::npos ||
                line.find(L"ComponentResourceManager") != wstring::npos) {
                skipLineForResources = true;
                resourceSkips++;
            }

            // FIX (issue 2): a local "Const Name As String = \"literal\"" needs a literal
            // (or another compile-time constant) on the right of "=" -- VB rejects a
            // function call there with "Constant expression is required". Nothing below
            // distinguished a Const declaration from an ordinary statement, so its literal
            // was swapped for a mxcncbs.ertrymn(...) decrypt call like any other string,
            // corrupting the constant. Skip the whole line when it declares one.
            {
                static const wregex localConstRe(L"^\\s*(?:Public\\s+|Private\\s+|Friend\\s+|Protected\\s+|Shared\\s+)*Const\\b", regex::icase);
                if (regex_search(line, localConstRe)) skipLineForResources = true;
            }

            wstring rebuilt; bool changed = false;
            size_t pos = 0;
            while (pos < line.size()) {
                if (line[pos] == L'"') {
                    size_t start = pos + 1, end = start;
                    wstring content;
                    bool closed = false;
                    while (end < line.size()) {
                        if (line[end] == L'"') {
                            if (end + 1 < line.size() && line[end + 1] == L'"') { content += L'"'; end += 2; continue; }
                            closed = true; break;
                        }
                        content += line[end]; end++;
                    }
                    if (closed && !content.empty() && !skipLineForResources &&
                        !(enableStringExclusion && exclusionSet.count(toLowerW(content))) &&
                        !isAlreadyEncrypted(content)) {
                        wstring encB64 = helper.encryptStringW(content);
                        stringEncryptionLog.push_back({ content, encB64 });
                        wstring var1name = helper.generateVarName();
                        wstring var2name = helper.generateVarName();
                        topDeclarations.push_back({ var1name, encB64 });
                        decryptCalls.push_back({ var2name, var1name });
                        rebuilt += var2name;
                        changed = true;
                        pos = end + 1;
                        continue;
                    }
                    rebuilt += L'"' + content + (closed ? L'"' + wstring() : wstring());
                    pos = closed ? end + 1 : line.size();
                    continue;
                }
                rebuilt += line[pos]; pos++;
            }
            methodLines.push_back(line);
            if (changed) stringReplacements.push_back({ methodLines.size() - 1, rebuilt });
        }
        if (inMethod) flushMethod();

        wstringstream finalOut;
        for (auto& l : result) finalOut << l << L"\n";
        if (resourceSkips > 0) {
            LogMessage(L"String encryption: skipped " + to_wstring(resourceSkips) +
                L" resource-key literal(s) in this file.", CLR_TEXT_DIM);
        }
        return finalOut.str();
    }

    wstring encryptStringsCFamily(const wstring& input) {
        StringEncryptionHelper helper(&globalNG);
        set<wstring, less<>> exclusionSet;
        if (enableStringExclusion)
            for (const auto& ex : stringExclusionList) exclusionSet.insert(toLowerW(ex));

        vector<wstring> srcLines;
        { wstringstream ss(input); wstring l; while (getline(ss, l)) { if (!l.empty() && l.back() == L'\r') l.pop_back(); srcLines.push_back(l); } }

        vector<wstring> result;
        bool inTopSetBlock = false;
        bool inTopCSharpStringArray = false;
        bool inMethod = false;
        int methodBraceDepth = 0;
        bool inBlockComment = false;
        vector<wstring> methodLines;
        vector<tuple<wstring, wstring>> topDeclarations;
        vector<tuple<wstring, wstring, bool>> decryptCalls;
        vector<pair<size_t, wstring>> stringReplacements;
        wstring methodIndent;

        const wstring declKeyword = (lang == Lang::Cpp) ? L"std::string " : L"string ";
        const wstring wideDeclKeyword = L"std::wstring ";
        const wstring callNs = (lang == Lang::Cpp) ? L"mxcncbs::" : L"mxcncbs.";

        wregex methodSigRe(LR"(^[\w.:<>\[\],\?&*~]+(?:\s+[\w.:<>\[\],\?&*~]+)*\s+[\w~]+\s*\([^;{}]*\)\s*(?:const\s*)?(?:override\s*)?(?:noexcept\s*)?(?:\s*:\s*[^{};]*)?(\{)?\s*$)");

        static const wchar_t* kCtrlKeywords[] = {
    L"if", L"else", L"for", L"foreach", L"while", L"switch", L"catch", L"do",
    L"using", L"lock", L"fixed", L"try", L"finally", L"case", L"break",
    L"continue", L"goto", L"throw"
        };

        auto looksLikeControl = [&](const wstring& trimmed) -> bool {
            for (auto* kw : kCtrlKeywords) {
                size_t klen = wcslen(kw);
                if (trimmed.size() == klen && trimmed == kw) return true;
                if (trimmed.size() > klen && trimmed.compare(0, klen, kw) == 0) {
                    wchar_t after = trimmed[klen];
                    if (!iswalnum(after) && after != L'_') return true;
                }
            }
            return false;
            };

        auto isBase64Encoded = [](const wstring& str) -> bool {
            if (str.length() < 16) return false;
            if (str.length() % 4 != 0) return false;
            for (wchar_t c : str) if (!(iswalnum(c) || c == L'+' || c == L'/' || c == L'=')) return false;
            return true;
            };

        auto isAlreadyEncrypted = [&](const wstring& content) -> bool {
            if (!excludeAlreadyEncrypted) return false;
            if (isBase64Encoded(content)) return true;
            if (content.length() > 20) {
                size_t firstEq = content.find(L'=');
                if (firstEq != wstring::npos) {
                    bool tailOnly = true;
                    for (size_t i = firstEq; i < content.size(); i++) {
                        if (content[i] != L'=') { tailOnly = false; break; }
                    }
                    if (tailOnly && (content.size() - firstEq) <= 2 && isBase64Encoded(content))
                        return true;
                }
            }
            return false;
            };

        // C# top-level/member initializers are outside method processing. Encrypt
        // ordinary C# string tokens there without touching verbatim/interpolated strings.
        auto encryptCSharpLiteralsOnLine = [&](const wstring& ln) -> wstring {
            if (lang != Lang::CSharp) return ln;
            wstring out;
            size_t p = 0;
            while (p < ln.size()) {
                if (ln[p] == L'@' && p + 1 < ln.size() && ln[p + 1] == L'"') {
                    size_t q = p + 2;
                    while (q < ln.size()) {
                        if (ln[q] == L'"') {
                            if (q + 1 < ln.size() && ln[q + 1] == L'"') { q += 2; continue; }
                            q++; break;
                        }
                        q++;
                    }
                    out += ln.substr(p, q - p); p = q; continue;
                }
                if (ln[p] == L'$' && p + 1 < ln.size() && ln[p + 1] == L'"') {
                    size_t q = p + 2;
                    while (q < ln.size()) {
                        if (ln[q] == L'\\' && q + 1 < ln.size()) { q += 2; continue; }
                        if (ln[q++] == L'"') break;
                    }
                    out += ln.substr(p, q - p); p = q; continue;
                }
                if (ln[p] == L'"') {
                    size_t q = p + 1; wstring content; bool closed = false;
                    while (q < ln.size()) {
                        if (ln[q] == L'\\' && q + 1 < ln.size()) { content += ln[q]; content += ln[q + 1]; q += 2; continue; }
                        if (ln[q] == L'"') { closed = true; break; }
                        content += ln[q++];
                    }
                    if (closed) {
                        wstring realValue = UnescapeCFamilyString(content);
                        if (!realValue.empty() && !isAlreadyEncrypted(content) &&
                            !(enableStringExclusion && exclusionSet.count(toLowerW(realValue)))) {
                            wstring encB64 = helper.encryptStringW(realValue);
                            stringEncryptionLog.push_back({ realValue, encB64 });
                            out += callNs + L"ertrymn(\"" + encB64 + L"\")";
                        }
                        else out += ln.substr(p, q + 1 - p);
                        p = q + 1; continue;
                    }
                }
                out += ln[p++];
            }
            return out;
            };


        auto isFormatStringLiteral = [&](const wstring& ln, size_t quotePos) -> bool {
            size_t j = quotePos;
            while (j > 0 && iswspace(ln[j - 1])) j--;
            if (j == 0 || ln[j - 1] != L'(') return false;
            j--;
            while (j > 0 && iswspace(ln[j - 1])) j--;
            size_t idEnd = j;
            while (j > 0 && (iswalnum(ln[j - 1]) || ln[j - 1] == L'_')) j--;
            wstring callee = ln.substr(j, idEnd - j);
            static const set<wstring> kFormatCallees = {
                L"format", L"vformat", L"vformat_to", L"format_to", L"format_to_n",
                L"formatted_size", L"print", L"println",
                L"printf", L"printf_s", L"sprintf", L"sprintf_s", L"snprintf", L"snprintf_s",
                L"fprintf", L"wsprintf", L"wsprintfW", L"wsprintfA", L"swprintf",
                L"_snprintf", L"_snwprintf", L"_stprintf", L"_stprintf_s",
                L"CString", L"Format", L"FormatV", L"AppendFormat",
                L"append_format", L"make_format_args", L"make_wformat_args",
                L"GetObject", L"ApplyResources", L"GetString", L"ComponentResourceManager"
            };
            return kFormatCallees.count(callee) != 0;
            };

        auto encryptLiteralsOnLine = [&](const wstring& ln) -> wstring {
            wstring out; size_t p = 0;
            while (p < ln.size()) {
                {
                    size_t q0 = p;
                    while (q0 > 0 && iswalpha(ln[q0 - 1])) q0--;
                    wstring pfx = ln.substr(q0, p - q0);
                    if (p < ln.size() && ln[p] == L'"' &&
                        (pfx == L"R" || pfx == L"LR" || pfx == L"uR" ||
                            pfx == L"UR" || pfx == L"u8R"))
                    {
                        size_t q = p + 1;
                        while (q < ln.size() && ln[q] != L'(') q++;
                        wstring delim = ln.substr(p + 1, q - (p + 1));
                        wstring endSeq = L")" + delim + L"\"";
                        size_t e = ln.find(endSeq, q);
                        if (e == wstring::npos) { out += ln.substr(p); p = ln.size(); }
                        else { size_t stop = e + endSeq.size(); out += ln.substr(p, stop - p); p = stop; }
                        continue;
                    }
                }
                if (ln[p] == L'L' && p + 1 < ln.size() && ln[p + 1] == L'"') {
                    size_t q = p + 2; wstring content; bool closed = false;
                    while (q < ln.size()) {
                        if (ln[q] == L'\\' && q + 1 < ln.size()) { content += ln[q]; content += ln[q + 1]; q += 2; continue; }
                        if (ln[q] == L'"') { closed = true; break; }
                        content += ln[q]; q++;
                    }
                    if (closed) {
                        wstring realValue = UnescapeCFamilyString(content);
                        if (!realValue.empty() && !isAlreadyEncrypted(content) &&
                            !(enableStringExclusion && exclusionSet.count(toLowerW(realValue)))) {
                            wstring encB64 = helper.encryptStringW(realValue);
                            stringEncryptionLog.push_back({ realValue, encB64 });
                            out += callNs + L"widen(" + callNs + L"ertrymn(\"" + encB64 + L"\"))";
                            p = q + 1;
                            continue;
                        }
                        out += L"L\"" + content + L"\"";
                        p = q + 1;
                        continue;
                    }
                }
                out += ln[p]; p++;
            }
            return out;
            };

        auto flushMethod = [&]() {
            if (methodLines.empty()) return;
            wstringstream out;
            out << methodLines[0] << L"\n";
            for (const auto& decl : topDeclarations)
                out << methodIndent << declKeyword << get<0>(decl) << L" = \"" << get<1>(decl) << L"\";\n";
            for (const auto& call : decryptCalls) {
                if (lang == Lang::Cpp && get<2>(call))
                    out << methodIndent << L"static const " << wideDeclKeyword << get<0>(call) << L" = " << callNs << L"widen(" << callNs << L"ertrymn(" << get<1>(call) << L"));\n";
                else
                    out << methodIndent << declKeyword << get<0>(call) << L" = " << callNs << L"ertrymn(" << get<1>(call) << L");\n";
            }
            for (size_t i = 1; i < methodLines.size(); i++) {
                wstring modifiedLine = methodLines[i];
                for (const auto& r : stringReplacements) if (r.first == i) { modifiedLine = r.second; break; }
                out << modifiedLine << L"\n";
            }
            result.push_back(out.str());
            methodLines.clear(); topDeclarations.clear(); decryptCalls.clear(); stringReplacements.clear(); methodIndent.clear();
            };

        for (size_t idx = 0; idx < srcLines.size(); idx++) {
            wstring line = srcLines[idx];
            wstring trimmed = trimW(line);

            if (!inMethod) {
                int delta = BraceDelta(line, inBlockComment);

                if (lang == Lang::CSharp) {
                    // A const field cannot contain a runtime decrypt call. Convert it
                    // to static readonly so the initializer can decrypt at runtime.
                    static const wregex csConstStringRe(LR"(^((?:(?:public|private|protected|internal|new)\s+)*)const\s+string\s+(\w+)\s*=\s*("(?:[^"\\]|\\.)*")\s*;\s*$)");
                    wsmatch cm;
                    if (regex_match(trimmed, cm, csConstStringRe)) {
                        wstring modifiers = cm[1].str();
                        wstring varName = cm[2].str();
                        wstring token = cm[3].str();
                        wstring content = token.substr(1, token.size() - 2);
                        wstring realValue = UnescapeCFamilyString(content);
                        if (!realValue.empty() && !isAlreadyEncrypted(content) &&
                            !(enableStringExclusion && exclusionSet.count(toLowerW(realValue)))) {
                            wstring encB64 = helper.encryptStringW(realValue);
                            stringEncryptionLog.push_back({ realValue, encB64 });
                            result.push_back(L"" + modifiers + L"static readonly string " + varName +
                                L" = " + callNs + L"ertrymn(\"" + encB64 + L"\");");
                            continue;
                        }
                    }

                    static const wregex csStringArrayOpenRe(
                        LR"(^\s*(?:(?:public|private|protected|internal|static|readonly|new)\s+)*string\s*\[\s*\]\s+\w+\s*=\s*(?:new\s+string\s*\[\s*\]\s*)?\{)");
                    if (inTopCSharpStringArray) {
                        result.push_back(encryptCSharpLiteralsOnLine(line));
                        if (line.find(L"};") != wstring::npos) inTopCSharpStringArray = false;
                        continue;
                    }
                    if (regex_search(trimmed, csStringArrayOpenRe)) {
                        result.push_back(encryptCSharpLiteralsOnLine(line));
                        if (line.find(L"};") == wstring::npos) inTopCSharpStringArray = true;
                        continue;
                    }
                }

                if (lang == Lang::Cpp && trimmed.rfind(L"#define", 0) == 0 && trimmed.find(L"L\\\"") != wstring::npos) {
                    static const wregex defineRe(LR"(^#define\s+(\w+)\s+(L"(?:[^"\\]|\\.)*")\s*$)");
                    wsmatch dm;
                    if (regex_search(trimmed, dm, defineRe)) {
                        wstring macroName = dm[1].str();
                        wstring litToken = dm[2].str();
                        wstring content = litToken.substr(2, litToken.size() - 3);
                        wstring realValue = UnescapeCFamilyString(content);
                        if (!realValue.empty() && !isAlreadyEncrypted(content) &&
                            !(enableStringExclusion && exclusionSet.count(toLowerW(realValue)))) {
                            wstring encB64 = helper.encryptStringW(realValue);
                            stringEncryptionLog.push_back({ realValue, encB64 });
                            wstring backingName = L"k" + macroName + L"Src";
                            result.push_back(L"static const std::wstring " + backingName + L" = " + callNs + L"widen(" + callNs + L"ertrymn(\"" + encB64 + L"\"));");
                            result.push_back(L"#define " + macroName + L" " + backingName + L".c_str()");
                            continue;
                        }
                    }
                }

                {
                    static const wregex topVarRe(LR"(^static\s+const\s+(?:wchar_t\s*\*|wstring|std::wstring)\s+(\w+)\s*=\s*(L"(?:[^"\\]|\\.)*")\s*;\s*$)");
                    wsmatch vm;
                    if (regex_search(trimmed, vm, topVarRe)) {
                        wstring varName = vm[1].str();
                        wstring litToken = vm[2].str();
                        wstring content = litToken.substr(2, litToken.size() - 3);
                        wstring realValue = UnescapeCFamilyString(content);
                        if (!realValue.empty() && !isAlreadyEncrypted(content) &&
                            !(enableStringExclusion && exclusionSet.count(toLowerW(realValue)))) {
                            wstring encB64 = helper.encryptStringW(realValue);
                            stringEncryptionLog.push_back({ realValue, encB64 });
                            result.push_back(L"static const std::wstring " + varName + L" = " + callNs + L"widen(" + callNs + L"ertrymn(\"" + encB64 + L"\"));");
                            continue;
                        }
                    }
                }

                if (inTopSetBlock) {
                    result.push_back(encryptLiteralsOnLine(line));
                    if (line.find(L"};") != wstring::npos) inTopSetBlock = false;
                    continue;
                }

                {
                    static const wregex topSetOpenRe(LR"(^static\s+const\s+set\s*<\s*wstring\s*(?:,[^>]*)?>\s*\w+\s*=\s*\{)");
                    if (regex_search(trimmed, topSetOpenRe)) {
                        result.push_back(encryptLiteralsOnLine(line));
                        if (line.find(L"};") == wstring::npos) inTopSetBlock = true;
                        continue;
                    }
                }

                if (trimmed.empty() || trimmed[0] == L'#' ||
                    (trimmed.size() >= 2 && trimmed[0] == L'/' && (trimmed[1] == L'/' || trimmed[1] == L'*')) ||
                    trimmed.find(L"using ") == 0 || trimmed[0] == L'[' || trimmed.find(L"extern ") == 0) {
                    result.push_back(line);
                    continue;
                }

                wsmatch m;
                bool sigMatch = !looksLikeControl(trimmed) && regex_search(trimmed, m, methodSigRe);

                if (sigMatch && m[1].matched && delta == 1) {
                    inMethod = true; methodBraceDepth = 1;
                    methodIndent = line.substr(0, line.find_first_not_of(L" \t")) + L"    ";
                    methodLines.push_back(line);
                    continue;
                }

                if (sigMatch && !m[1].matched && delta == 0 && idx + 1 < srcLines.size()) {
                    wstring nextTrimmed = trimW(srcLines[idx + 1]);
                    if (nextTrimmed == L"{") {
                        bool savedComment = inBlockComment;
                        int braceLineDelta = BraceDelta(srcLines[idx + 1], inBlockComment);
                        if (braceLineDelta == 1) {
                            inMethod = true; methodBraceDepth = 1;
                            methodIndent = line.substr(0, line.find_first_not_of(L" \t")) + L"    ";
                            methodLines.push_back(line + L"\n" + srcLines[idx + 1]);
                            idx++;
                            continue;
                        }
                        inBlockComment = savedComment;
                    }
                }

                result.push_back(line);
                continue;
            }

            // FIX (issue 2): a "const" local (C#) or a "case" label (C#/C++) both require
            // a compile-time constant on the right-hand side; a field/parameter attribute
            // argument does too. Swapping the literal for a mxcncbs::ertrymn(...) decrypt
            // call turned a valid constant into a runtime call, producing "a constant value
            // is expected" / "expression must be constant". None of the string-literal
            // scanning below knew the difference, so skip encryption on the whole line
            // whenever it is one of these three shapes.
            bool skipLineForConstContext = false;
            if (lang == Lang::CSharp) {
                static const wregex localConstRe(LR"(^\s*(?:public\s+|private\s+|protected\s+|internal\s+|static\s+|readonly\s+)*const\b)");
                if (regex_search(trimmed, localConstRe)) skipLineForConstContext = true;
            }
            if (!skipLineForConstContext) {
                static const wregex caseLabelRe(LR"(^\s*case\b)");
                if (regex_search(trimmed, caseLabelRe)) skipLineForConstContext = true;
            }
            if (!skipLineForConstContext && !trimmed.empty() && trimmed[0] == L'[') {
                skipLineForConstContext = true;   // attribute usage/argument, e.g. [Foo("...")]
            }
            if (skipLineForConstContext) {
                methodLines.push_back(line);
                continue;
            }

            wstring rebuilt; bool changed = false;
            size_t pos = 0;
            while (pos < line.size()) {
                {
                    size_t p = pos;
                    while (p > 0 && iswalpha(line[p - 1])) p--;
                    wstring pfx = line.substr(p, pos - p);
                    if (line[pos] == L'"' &&
                        (pfx == L"R" || pfx == L"LR" || pfx == L"uR" ||
                            pfx == L"UR" || pfx == L"u8R"))
                    {
                        size_t q = pos + 1;
                        while (q < line.size() && line[q] != L'(') q++;
                        wstring delim = line.substr(pos + 1, q - (pos + 1));
                        wstring endSeq = L")" + delim + L"\"";
                        size_t e = line.find(endSeq, q);
                        if (e == wstring::npos) { rebuilt += line.substr(pos); pos = line.size(); }
                        else { size_t stop = e + endSeq.size(); rebuilt += line.substr(pos, stop - pos); pos = stop; }
                        continue;
                    }
                }

                {
                    size_t p = pos;
                    bool hasDollar = false, hasAt = false;
                    size_t q = p;
                    while (q < line.size() && q - p < 2 && (line[q] == L'$' || line[q] == L'@')) {
                        if (line[q] == L'$') hasDollar = true; else hasAt = true;
                        q++;
                    }
                    if ((hasDollar || hasAt) && q < line.size() && line[q] == L'"') {
                        bool verbatim = hasAt;
                        bool interpolated = hasDollar;
                        size_t i = q + 1;
                        int exprDepth = 0;
                        while (i < line.size()) {
                            wchar_t c = line[i];
                            if (exprDepth > 0) {
                                if (c == L'"') {
                                    size_t j = i + 1;
                                    while (j < line.size()) {
                                        if (line[j] == L'\\' && j + 1 < line.size()) { j += 2; continue; }
                                        if (line[j] == L'"') { j++; break; }
                                        j++;
                                    }
                                    i = j; continue;
                                }
                                if (c == L'{') { exprDepth++; i++; continue; }
                                if (c == L'}') { exprDepth--; i++; continue; }
                                i++; continue;
                            }
                            if (interpolated && c == L'{') {
                                if (i + 1 < line.size() && line[i + 1] == L'{') { i += 2; continue; }
                                exprDepth = 1; i++; continue;
                            }
                            if (interpolated && c == L'}' && i + 1 < line.size() && line[i + 1] == L'}') { i += 2; continue; }
                            if (c == L'"') {
                                if (verbatim && i + 1 < line.size() && line[i + 1] == L'"') { i += 2; continue; }
                                i++; break;
                            }
                            if (!verbatim && c == L'\\' && i + 1 < line.size()) { i += 2; continue; }
                            i++;
                        }
                        rebuilt += line.substr(pos, i - pos);
                        pos = i;
                        continue;
                    }
                }

                if (line[pos] == L'\'') {
                    size_t j = pos + 1;
                    if (j < line.size() && line[j] == L'\\') j++;
                    if (j < line.size()) j++;
                    if (j < line.size() && line[j] == L'\'') j++;
                    rebuilt += line.substr(pos, j - pos);
                    pos = j;
                    continue;
                }

                if (line[pos] == L'"') {

                    {
                        size_t pfxStart = pos;
                        while (pfxStart > 0 && iswalpha(line[pfxStart - 1])) pfxStart--;
                        wstring pfxRaw = line.substr(pfxStart, pos - pfxStart);
                        if (pfxRaw == L"R" || pfxRaw == L"LR" || pfxRaw == L"uR" ||
                            pfxRaw == L"UR" || pfxRaw == L"u8R")
                        {
                            size_t delimStart = pos + 1, j = delimStart;
                            while (j < line.size() && line[j] != L'(') j++;
                            if (j < line.size() && line[j] == L'(') {
                                wstring delim = line.substr(delimStart, j - delimStart);
                                wstring endSeq = L")" + delim + L"\"";
                                size_t endMarker = line.find(endSeq, j + 1);
                                size_t rawStart = pfxStart;
                                if (endMarker == wstring::npos) {
                                    rebuilt += line.substr(rawStart);
                                    pos = line.size();
                                }
                                else {
                                    size_t endPos = endMarker + endSeq.size();
                                    rebuilt += line.substr(rawStart, endPos - rawStart);
                                    pos = endPos;
                                }
                                continue;
                            }
                        }
                    }

                    size_t start = pos + 1, end = start;
                    wstring content;
                    bool closed = false;

                    while (end < line.size()) {
                        if (line[end] == L'\\' && end + 1 < line.size()) { content += line[end]; content += line[end + 1]; end += 2; continue; }
                        if (line[end] == L'"') { closed = true; break; }
                        content += line[end]; end++;
                    }

                    wstring realValue = UnescapeCFamilyString(content);

                    bool hasFmtPercent = false;
                    for (size_t k = 0; k + 1 < content.size(); k++) {
                        if (content[k] == L'%' && (iswalpha(content[k + 1]) || iswdigit(content[k + 1]) || content[k + 1] == L'%')) {
                            hasFmtPercent = true; break;
                        }
                    }
                    bool hasFmtBrace = false;
                    for (size_t k = 0; k + 1 < content.size(); k++) {
                        if (content[k] == L'{' && content[k + 1] != L'{') {
                            for (size_t j = k + 1; j < content.size(); j++) {
                                if (content[j] == L'}') { hasFmtBrace = true; break; }
                                if (content[j] == L'{') break;
                            }
                            if (hasFmtBrace) break;
                        }
                    }
                    bool hasBraceOrPercent = hasFmtBrace || hasFmtPercent;

                    bool isFormatLiteral = isFormatStringLiteral(line, pos) || hasBraceOrPercent;

                    bool isRawString = false;
                    bool isPlainWide = false;
                    bool skipLiteral = false;
                    size_t pfxStart = pos;
                    {
                        size_t pfxEnd = pos;
                        pfxStart = pfxEnd;
                        while (pfxStart > 0 && iswalpha(line[pfxStart - 1])) pfxStart--;
                        wstring pfx = line.substr(pfxStart, pfxEnd - pfxStart);
                        if (pfx == L"R" || pfx == L"LR" || pfx == L"uR" || pfx == L"UR" || pfx == L"u8R")
                            isRawString = true;
                        else if (pfx == L"$" || pfx == L"$@" || pfx == L"@$" || pfx == L"@")
                            skipLiteral = true;
                        else if (pfx == L"L")
                            isPlainWide = true;
                        else if (pfx == L"u" || pfx == L"U" || pfx == L"u8")
                            skipLiteral = true;
                    }

                    bool isConcatFragment = false;
                    bool isArrayInit = false;
                    bool isCaseLabel = false;
                    if (closed) {
                        wstring before = line.substr(0, pfxStart);
                        wstring after = line.substr(end + 1);
                        bool beforeBlank = trimW(before).empty();
                        bool afterBlank = trimW(after).empty();
                        auto endsWithLiteral = [](const wstring& t) -> bool {
                            size_t e = t.size();
                            while (e > 0 && iswspace(t[e - 1])) e--;
                            while (e > 0 && (t[e - 1] == L';' || t[e - 1] == L')' || t[e - 1] == L',')) e--;
                            while (e > 0 && iswspace(t[e - 1])) e--;
                            return e > 0 && t[e - 1] == L'"';
                            };
                        auto startsWithLiteral = [](const wstring& t) -> bool {
                            size_t b = 0;
                            while (b < t.size() && iswspace(t[b])) b++;
                            size_t k = b;
                            while (k < t.size() && iswalpha(t[k])) k++;
                            return k < t.size() && t[k] == L'"';
                            };
                        auto prevCodeLine = [&](int p0) -> wstring {
                            int scanned = 0;
                            for (int p = p0; p >= 0 && scanned < 3; p--, scanned++) {
                                wstring pt = trimW(srcLines[p]);
                                if (pt.empty() || (pt.size() >= 2 && pt[0] == L'/' && pt[1] == L'/')) continue;
                                return pt;
                            }
                            return L"";
                            };
                        auto nextCodeLine = [&](size_t n0) -> wstring {
                            size_t scanned = 0;
                            for (size_t n = n0; n < srcLines.size() && scanned < 3; n++, scanned++) {
                                wstring nt = trimW(srcLines[n]);
                                if (nt.empty() || (nt.size() >= 2 && nt[0] == L'/' && nt[1] == L'/')) continue;
                                return nt;
                            }
                            return L"";
                            };
                        {
                            size_t j = end + 1;
                            while (j < line.size() && iswspace(line[j])) j++;
                            size_t k = j;
                            while (k < line.size() && iswalpha(line[k])) k++;
                            if (k < line.size() && line[k] == L'"') isConcatFragment = true;
                        }
                        if (!isConcatFragment) {
                            if (beforeBlank && afterBlank)
                                isConcatFragment = endsWithLiteral(prevCodeLine((int)idx - 1))
                                || startsWithLiteral(nextCodeLine(idx + 1));
                            else if (afterBlank)
                                isConcatFragment = startsWithLiteral(nextCodeLine(idx + 1));
                            else if (beforeBlank)
                                isConcatFragment = endsWithLiteral(prevCodeLine((int)idx - 1));
                        }

                        wstring beforeTrim = trimW(before);
                        if (!beforeTrim.empty() && beforeTrim.back() == L'=') {
                            wstring lhs = trimW(beforeTrim.substr(0, beforeTrim.size() - 1));
                            if (!lhs.empty() && lhs.back() == L']') isArrayInit = true;
                            if (!isArrayInit) {
                                wstring afterTrim3 = trimW(after);
                                if (!afterTrim3.empty() && afterTrim3[0] == L'[') isArrayInit = true;
                            }
                        }

                        if (!isArrayInit && !beforeTrim.empty() && beforeTrim.back() == L'[') {
                            wstring preL = trimW(beforeTrim.substr(0, beforeTrim.size() - 1));
                            if (!preL.empty() && preL.back() == L'=') isArrayInit = true;
                        }

                        if (beforeTrim.size() >= 4 && beforeTrim.compare(beforeTrim.size() - 4, 4, L"case") == 0 &&
                            (beforeTrim.size() == 4 || !(iswalnum(beforeTrim[beforeTrim.size() - 5]) || beforeTrim[beforeTrim.size() - 5] == L'_'))) {
                            wstring afterTrim2 = trimW(after);
                            if (!afterTrim2.empty() && afterTrim2[0] == L':' && (afterTrim2.size() < 2 || afterTrim2[1] != L':'))
                                isCaseLabel = true;
                        }
                    }

                    if (closed && !realValue.empty() && !isRawString && !skipLiteral && !isConcatFragment && !isArrayInit && !isCaseLabel && !isFormatLiteral &&
                        !(enableStringExclusion && exclusionSet.count(toLowerW(realValue))) &&
                        !isAlreadyEncrypted(content)) {

                        if (isPlainWide && !rebuilt.empty() && rebuilt.back() == L'L') rebuilt.pop_back();

                        wstring encB64 = helper.encryptStringW(realValue);
                        stringEncryptionLog.push_back({ realValue, encB64 });
                        wstring var1name = helper.generateVarName();
                        wstring var2name = helper.generateVarName();
                        topDeclarations.push_back({ var1name, encB64 });
                        decryptCalls.push_back({ var2name, var1name, isPlainWide });
                        rebuilt += (lang == Lang::Cpp) ? (var2name + L".c_str()") : var2name;
                        changed = true;
                        pos = end + 1;
                        continue;
                    }

                    rebuilt += L'"' + content + (closed ? L'"' + wstring() : wstring());
                    pos = closed ? end + 1 : line.size();
                    continue;
                }

                rebuilt += line[pos]; pos++;
            }

            methodLines.push_back(line);
            if (changed) stringReplacements.push_back({ methodLines.size() - 1, rebuilt });

            methodBraceDepth += BraceDelta(line, inBlockComment);
            if (methodBraceDepth <= 0) {
                flushMethod();
                inMethod = false;
            }
        }
        if (inMethod) flushMethod();

        wstringstream finalOut;
        for (auto& l : result) finalOut << l << L"\n";
        return finalOut.str();
    }

    // ---------------------------------------------------------------------
    // v3.3: standalone-decryptor support (project mode)
    // ---------------------------------------------------------------------
public:
    struct StandaloneDecryptor {
        wstring fileName;    // e.g. "ObfDecryptor.cs"
        wstring content;     // file body to write
        wstring includeLine; // for C++: '#include "mxcncbs.h"'; empty otherwise
    };

    StandaloneDecryptor makeStandaloneDecryptor(Lang l) {
        StandaloneDecryptor d;
        if (l == Lang::CSharp) {
            d.fileName = L"ObfDecryptor.cs";
            d.content =
                L"// Auto-generated by obfuscator -- single source of string-decryption logic.\n"
                L"// Do not rename: obfuscated call sites reference mxcncbs.ertrymn(...).\n"
                + injectDecryptModuleCS();
            return d;
        }
        if (l == Lang::VB) {
            d.fileName = L"ObfDecryptor.vb";
            d.content =
                L"' Auto-generated by obfuscator -- single source of string-decryption logic.\n"
                L"' Do not rename: obfuscated call sites reference mxcncbs.ertrymn(...).\n"
                + injectDecryptModuleVB();
            return d;
        }
        // C++: header-only so any number of TUs can include it without ODR issues.
        d.fileName = L"mxcncbs.h";
        d.includeLine = L"#include \"mxcncbs.h\"";
        d.content = makeStandaloneDecryptorCppHeader();
        return d;
    }

    // v3.4: body-only decryptor text, injected into ONE existing project file
    // chosen by the user (no standalone decryptor file is written).
    wstring makeHostDecryptorBody(Lang l) {
        if (l == Lang::CSharp) {
            return L"\n// Auto-generated by obfuscator -- public string-decryption class.\n"
                L"// Do not rename: obfuscated call sites reference mxcncbs.ertrymn(...).\n"
                + injectDecryptModuleCS();
        }
        if (l == Lang::VB) {
            return L"\n' Auto-generated by obfuscator -- public string-decryption module.\n"
                L"' Do not rename: obfuscated call sites reference mxcncbs.ertrymn(...).\n"
                + injectDecryptModuleVB();
        }
        wstring cpp = makeStandaloneDecryptorCppHeader();
        size_t pp = cpp.find(L"#pragma once\n");
        if (pp != wstring::npos) cpp.erase(pp, wcslen(L"#pragma once\n"));
        return cpp;
    }

private:
    wstring makeStandaloneDecryptorCppHeader() {
        wstring s;
        s += L"// Auto-generated by obfuscator -- single source of string-decryption logic.\n";
        s += L"// Header-only: safe to include from any number of translation units.\n";
        s += L"// Do not rename: obfuscated call sites reference mxcncbs::ertrymn(...).\n";
        s += L"#pragma once\n";
        s += L"#ifndef MXCNCBS_H\n";
        s += L"#define MXCNCBS_H\n";
        s += L"#include <windows.h>\n";
        s += L"#include <wincrypt.h>\n";
        s += L"#include <string>\n";
        s += L"#include <vector>\n";
        s += L"#include <cstring>\n";
        s += L"#pragma comment(lib, \"advapi32.lib\")\n";
        s += L"\n";
        s += L"class mxcncbs {\n";
        s += L"    static const unsigned char ms[16];\n";
        s += L"    static const unsigned char key[32];\n";
        s += L"\n";
        s += L"    static std::vector<unsigned char> base64Decode(const std::string& s) {\n";
        s += L"        static const std::string b64 = \"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/\";\n";
        s += L"        std::vector<unsigned char> out;\n";
        s += L"        int val = 0, bits = -8;\n";
        s += L"        for (unsigned char c : s) {\n";
        s += L"            if (c == '=') break;\n";
        s += L"            size_t p = b64.find((char)c);\n";
        s += L"            if (p == std::string::npos) continue;\n";
        s += L"            val = (val << 6) + (int)p; bits += 6;\n";
        s += L"            if (bits >= 0) { out.push_back((unsigned char)((val >> bits) & 0xFF)); bits -= 8; }\n";
        s += L"        }\n";
        s += L"        return out;\n";
        s += L"    }\n";
        s += L"\n";
        s += L"    static bool aesEncryptBlock(HCRYPTKEY hKey, const unsigned char* in, unsigned char* out) {\n";
        s += L"        DWORD len = 16; unsigned char temp[16]; std::memcpy(temp, in, 16);\n";
        s += L"        if (!CryptEncrypt(hKey, 0, FALSE, 0, temp, &len, 16)) return false;\n";
        s += L"        std::memcpy(out, temp, 16); return true;\n";
        s += L"    }\n";
        s += L"\n";
        s += L"    static std::wstring utf8ToWide(const std::string& s) {\n";
        s += L"        std::wstring out; out.reserve(s.size()); size_t i = 0;\n";
        s += L"        while (i < s.size()) {\n";
        s += L"            unsigned char c = (unsigned char)s[i]; unsigned int cp = 0; int extra = 0;\n";
        s += L"            if (c < 0x80) { cp = c; extra = 0; }\n";
        s += L"            else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }\n";
        s += L"            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }\n";
        s += L"            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }\n";
        s += L"            else { i++; continue; }\n";
        s += L"            if (i + (size_t)extra >= s.size()) break;\n";
        s += L"            bool ok = true;\n";
        s += L"            for (int k = 1; k <= extra; k++) {\n";
        s += L"                unsigned char cc = (unsigned char)s[i + k];\n";
        s += L"                if ((cc & 0xC0) != 0x80) { ok = false; break; }\n";
        s += L"                cp = (cp << 6) | (cc & 0x3F);\n";
        s += L"            }\n";
        s += L"            if (!ok) { i++; continue; }\n";
        s += L"            i += (size_t)extra + 1;\n";
        s += L"            if (sizeof(wchar_t) == 2 && cp > 0xFFFF) {\n";
        s += L"                cp -= 0x10000;\n";
        s += L"                out += (wchar_t)(0xD800 + (cp >> 10));\n";
        s += L"                out += (wchar_t)(0xDC00 + (cp & 0x3FF));\n";
        s += L"            } else { out += (wchar_t)cp; }\n";
        s += L"        }\n";
        s += L"        return out;\n";
        s += L"    }\n";
        s += L"\n";
        s += L"public:\n";
        s += L"    static std::wstring widen(const std::string& ewwer) { return utf8ToWide(ewwer); }\n";
        s += L"\n";
        s += L"    static std::string ertrymn(const std::string& ewwer) {\n";
        s += L"        if (ewwer.empty()) return \"\";\n";
        s += L"        std::vector<unsigned char> cipher = base64Decode(ewwer);\n";
        s += L"        if (cipher.empty()) return \"\";\n";
        s += L"        HCRYPTPROV hProv = NULL; HCRYPTKEY hKey = NULL;\n";
        s += L"        if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return \"\";\n";
        s += L"        struct { BLOBHEADER header; DWORD keySize; unsigned char keyBytes[32]; } keyBlob;\n";
        s += L"        keyBlob.header.bType = PLAINTEXTKEYBLOB;\n";
        s += L"        keyBlob.header.bVersion = CUR_BLOB_VERSION;\n";
        s += L"        keyBlob.header.reserved = 0;\n";
        s += L"        keyBlob.header.aiKeyAlg = CALG_AES_256;\n";
        s += L"        keyBlob.keySize = 32;\n";
        s += L"        std::memcpy(keyBlob.keyBytes, key, 32);\n";
        s += L"        if (!CryptImportKey(hProv, (BYTE*)&keyBlob, sizeof(keyBlob), 0, 0, &hKey)) { CryptReleaseContext(hProv, 0); return \"\"; }\n";
        s += L"        DWORD mode = CRYPT_MODE_ECB;\n";
        s += L"        CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&mode, 0);\n";
        s += L"        unsigned char feedback[16]; std::memcpy(feedback, ms, 16);\n";
        s += L"        std::vector<unsigned char> plain; plain.reserve(cipher.size());\n";
        s += L"        for (size_t i = 0; i < cipher.size(); i++) {\n";
        s += L"            unsigned char encFeedback[16];\n";
        s += L"            if (!aesEncryptBlock(hKey, feedback, encFeedback)) break;\n";
        s += L"            unsigned char cb = cipher[i]; unsigned char pb = cb ^ encFeedback[0];\n";
        s += L"            plain.push_back(pb);\n";
        s += L"            std::memmove(feedback, feedback + 1, 15);\n";
        s += L"            feedback[15] = cb;\n";
        s += L"        }\n";
        s += L"        CryptDestroyKey(hKey); CryptReleaseContext(hProv, 0);\n";
        s += L"        if (plain.empty()) return \"\";\n";
        s += L"        unsigned char padLen = plain.back();\n";
        s += L"        if (padLen > 0 && padLen <= 16 && (size_t)padLen <= plain.size()) plain.resize(plain.size() - padLen);\n";
        s += L"        return std::string(plain.begin(), plain.end());\n";
        s += L"    }\n";
        s += L"};\n";
        // Out-of-line definitions (header-only safe: they are inline-able because
        // the compiler sees them in every TU that includes the header; the linker
        // emits one copy).
        s += L"inline const unsigned char mxcncbs::ms[16] = { 0xAD, 0xFB, 0x58, 0x5A, 0xEB, 0x1C, 0x42, 0x23, 0xE2, 0x34, 0x8E, 0x3D, 0x6C, 0x61, 0xA8, 0x9D };\n";
        s += L"inline const unsigned char mxcncbs::key[32] = { 0x2E, 0xBE, 0x5C, 0xCD, 0x94, 0x6C, 0xB2, 0xB5, 0x78, 0x3A, 0x20, 0x26, 0x49, 0x87, 0x5F, 0x26, 0x23, 0xB8, 0x59, 0x1E, 0xCF, 0x9D, 0x27, 0x0A, 0xE3, 0x92, 0x1C, 0x89, 0x45, 0x25, 0x26, 0x09 };\n";
        s += L"#endif // MXCNCBS_H\n";
        return s;
    }

public:
    wstring injectDecryptModule(const wstring& code) {
        if (!enableStringEncryption || !shouldInjectDecryptModule) return code;
        if (lang == Lang::CSharp) return code + injectDecryptModuleCS();
        if (lang == Lang::Cpp) {
            wstring module = injectDecryptModuleCpp();
            size_t insertPos = 0;
            wstringstream ss(code);
            wstring ln;
            size_t off = 0;
            while (getline(ss, ln)) {
                wstring t = trimW(ln);
                if (!t.empty() && t[0] == L'\uFEFF') t.erase(0, 1);
                bool isDirective = t.rfind(L"#include", 0) == 0 ||
                    t.rfind(L"#pragma", 0) == 0 ||
                    t.rfind(L"using namespace", 0) == 0;
                bool isSkippable = t.empty() ||
                    (t.size() >= 2 && t[0] == L'/' && (t[1] == L'/' || t[1] == L'*')) ||
                    t[0] == L'#';
                if (isDirective) {
                    insertPos = off + ln.size() + 1;
                }
                else if (!isSkippable) {
                    break;
                }
                off += ln.size() + 1;
            }
            return code.substr(0, insertPos) + module + code.substr(insertPos);
        }
        return code + injectDecryptModuleVB();
    }

private:
    wstring injectDecryptModuleVB() {
        wstring module;
        module += L"\n' ==========================================\n";
        module += L"' Decryption Module (Injected by Obfuscator)\n";
        module += L"' Matches the AES-256-CFB8 logic used to encrypt strings above\n";
        module += L"' ==========================================\n";
        module += L"Public Module mxcncbs\n";
        module += L"    Private Shared ReadOnly ms As Byte() = New Byte() {\n";
        module += L"        &HAD, &HFB, &H58, &H5A, &HEB, &H1C, &H42, &H23,\n";
        module += L"        &HE2, &H34, &H8E, &H3D, &H6C, &H61, &HA8, &H9D\n";
        module += L"    }\n";
        module += L"    Private Shared ReadOnly key As Byte() = New Byte() {\n";
        module += L"        &H2E, &HBE, &H5C, &HCD, &H94, &H6C, &HB2, &HB5,\n";
        module += L"        &H78, &H3A, &H20, &H26, &H49, &H87, &H5F, &H26,\n";
        module += L"        &H23, &HB8, &H59, &H1E, &HCF, &H9D, &H27, &HA,\n";
        module += L"        &HE3, &H92, &H1C, &H89, &H45, &H25, &H26, &H9\n";
        module += L"    }\n";
        module += L"    Private Shared aesAlg As System.Security.Cryptography.Aes = Nothing\n";
        module += L"\n";
        module += L"    Shared Sub New()\n";
        module += L"        aesAlg = System.Security.Cryptography.Aes.Create()\n";
        module += L"        aesAlg.Key = key\n";
        module += L"        aesAlg.IV = ms\n";
        module += L"        aesAlg.Mode = System.Security.Cryptography.CipherMode.CFB\n";
        module += L"        aesAlg.FeedbackSize = 8\n";
        module += L"        aesAlg.Padding = System.Security.Cryptography.PaddingMode.PKCS7\n";
        module += L"    End Sub\n";
        module += L"\n";
        module += L"    Public Function ertrymn(ByVal ewwer As String) As String\n";
        module += L"        Try\n";
        module += L"            If ewwer Is Nothing OrElse ewwer.Length <= 0 Then\n";
        module += L"                Return \"\"\n";
        module += L"            End If\n";
        module += L"            Dim mnmbnbn As Byte() = Convert.FromBase64String(ewwer)\n";
        module += L"            Dim sfds As System.Security.Cryptography.ICryptoTransform = aesAlg.CreateDecryptor(aesAlg.Key, aesAlg.IV)\n";
        module += L"            Using wewesd As New System.IO.MemoryStream(mnmbnbn)\n";
        module += L"                Using uiiiu As New System.Security.Cryptography.CryptoStream(wewesd, sfds, System.Security.Cryptography.CryptoStreamMode.Read)\n";
        module += L"                    Using errt As New System.IO.StreamReader(uiiiu)\n";
        module += L"                        Return errt.ReadToEnd()\n";
        module += L"                    End Using\n";
        module += L"                End Using\n";
        module += L"            End Using\n";
        module += L"        Catch ex As Exception\n";
        module += L"            Return \"\"\n";
        module += L"        End Try\n";
        module += L"    End Function\n";
        module += L"End Module\n";
        return module;
    }

    wstring injectDecryptModuleCS() {
        wstring s;
        s += L"\n\n// ==========================================\n";
        s += L"// Decryption Module (Injected by Obfuscator)\n";
        s += L"// Matches the AES-256-CFB8 logic used to encrypt strings above\n";
        s += L"// ==========================================\n";
        s += L"public static class mxcncbs\n";
        s += L"{\n";
        s += L"    private static readonly byte[] ms = new byte[] {\n";
        s += L"        0xAD, 0xFB, 0x58, 0x5A, 0xEB, 0x1C, 0x42, 0x23,\n";
        s += L"        0xE2, 0x34, 0x8E, 0x3D, 0x6C, 0x61, 0xA8, 0x9D\n";
        s += L"    };\n";
        s += L"    private static readonly byte[] key = new byte[] {\n";
        s += L"        0x2E, 0xBE, 0x5C, 0xCD, 0x94, 0x6C, 0xB2, 0xB5,\n";
        s += L"        0x78, 0x3A, 0x20, 0x26, 0x49, 0x87, 0x5F, 0x26,\n";
        s += L"        0x23, 0xB8, 0x59, 0x1E, 0xCF, 0x9D, 0x27, 0x0A,\n";
        s += L"        0xE3, 0x92, 0x1C, 0x89, 0x45, 0x25, 0x26, 0x09\n";
        s += L"    };\n";
        s += L"    private static readonly System.Security.Cryptography.Aes aesAlg;\n";
        s += L"\n";
        s += L"    static mxcncbs()\n";
        s += L"    {\n";
        s += L"        aesAlg = System.Security.Cryptography.Aes.Create();\n";
        s += L"        aesAlg.Key = key;\n";
        s += L"        aesAlg.IV = ms;\n";
        s += L"        aesAlg.Mode = System.Security.Cryptography.CipherMode.CFB;\n";
        s += L"        aesAlg.FeedbackSize = 8;\n";
        s += L"        aesAlg.Padding = System.Security.Cryptography.PaddingMode.PKCS7;\n";
        s += L"    }\n";
        s += L"\n";
        s += L"    public static string ertrymn(string ewwer)\n";
        s += L"    {\n";
        s += L"        try\n";
        s += L"        {\n";
        s += L"            if (string.IsNullOrEmpty(ewwer)) return \"\";\n";
        s += L"            byte[] mnmbnbn = System.Convert.FromBase64String(ewwer);\n";
        s += L"            using (var sfds = aesAlg.CreateDecryptor(aesAlg.Key, aesAlg.IV))\n";
        s += L"            using (var wewesd = new System.IO.MemoryStream(mnmbnbn))\n";
        s += L"            {\n";
        s += L"                byte[] plain;\n";
        s += L"                using (var uiiiu = new System.Security.Cryptography.CryptoStream(wewesd, sfds, System.Security.Cryptography.CryptoStreamMode.Read))\n";
        s += L"                using (var buf = new System.IO.MemoryStream())\n";
        s += L"                {\n";
        s += L"                    uiiiu.CopyTo(buf);\n";
        s += L"                    plain = buf.ToArray();\n";
        s += L"                }\n";
        s += L"                if (plain.Length > 0)\n";
        s += L"                {\n";
        s += L"                    int pad = plain[plain.Length - 1];\n";
        s += L"                    if (pad > 0 && pad <= 16 && pad <= plain.Length)\n";
        s += L"                    {\n";
        s += L"                        bool ok = true;\n";
        s += L"                        for (int i = plain.Length - pad; i < plain.Length; i++)\n";
        s += L"                            if (plain[i] != pad) { ok = false; break; }\n";
        s += L"                        if (ok) System.Array.Resize(ref plain, plain.Length - pad);\n";
        s += L"                    }\n";
        s += L"                }\n";
        s += L"                return System.Text.Encoding.UTF8.GetString(plain);\n";
        s += L"            }\n";
        s += L"        }\n";
        s += L"        catch (System.Exception)\n";
        s += L"        {\n";
        s += L"            return \"\";\n";
        s += L"        }\n";
        s += L"    }\n";
        s += L"}\n";
        return s;
    }

    wstring injectDecryptModuleCpp() {
        wstring s;
        s += L"\n// ==========================================\n";
        s += L"// Decryption Module (Injected by Obfuscator)\n";
        s += L"// Matches the AES-256-CFB8 logic used to encrypt strings above.\n";
        s += L"// Requires <windows.h>, <wincrypt.h>, <string>, <vector>.\n";
        s += L"// ==========================================\n";
        s += L"#pragma comment(lib, \"advapi32.lib\")\n";
        s += L"#include <wincrypt.h>\n";
        s += L"class mxcncbs {\n";
        s += L"    static const unsigned char ms[16];\n";
        s += L"    static const unsigned char key[32];\n";
        s += L"\n";
        s += L"    static std::vector<unsigned char> base64Decode(const std::string& s) {\n";
        s += L"        static const std::string b64 = \"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/\";\n";
        s += L"        std::vector<unsigned char> out;\n";
        s += L"        int val = 0, bits = -8;\n";
        s += L"        for (unsigned char c : s) {\n";
        s += L"            if (c == '=') break;\n";
        s += L"            size_t p = b64.find((char)c);\n";
        s += L"            if (p == std::string::npos) continue;\n";
        s += L"            val = (val << 6) + (int)p;\n";
        s += L"            bits += 6;\n";
        s += L"            if (bits >= 0) {\n";
        s += L"                out.push_back((unsigned char)((val >> bits) & 0xFF));\n";
        s += L"                bits -= 8;\n";
        s += L"            }\n";
        s += L"        }\n";
        s += L"        return out;\n";
        s += L"    }\n";
        s += L"\n";
        s += L"    static bool aesEncryptBlock(HCRYPTKEY hKey, const unsigned char* in, unsigned char* out) {\n";
        s += L"        DWORD len = 16;\n";
        s += L"        unsigned char temp[16];\n";
        s += L"        memcpy(temp, in, 16);\n";
        s += L"        if (!CryptEncrypt(hKey, 0, FALSE, 0, temp, &len, 16)) return false;\n";
        s += L"        memcpy(out, temp, 16);\n";
        s += L"        return true;\n";
        s += L"    }\n";
        s += L"\n";
        s += L"    static std::wstring utf8ToWide(const std::string& s) {\n";
        s += L"        std::wstring out;\n";
        s += L"        out.reserve(s.size());\n";
        s += L"        size_t i = 0;\n";
        s += L"        while (i < s.size()) {\n";
        s += L"            unsigned char c = (unsigned char)s[i];\n";
        s += L"            unsigned int cp = 0;\n";
        s += L"            int extra = 0;\n";
        s += L"            if (c < 0x80) { cp = c; extra = 0; }\n";
        s += L"            else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }\n";
        s += L"            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }\n";
        s += L"            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }\n";
        s += L"            else { i++; continue; }\n";
        s += L"            if (i + (size_t)extra >= s.size()) break;\n";
        s += L"            bool ok = true;\n";
        s += L"            for (int k = 1; k <= extra; k++) {\n";
        s += L"                unsigned char cc = (unsigned char)s[i + k];\n";
        s += L"                if ((cc & 0xC0) != 0x80) { ok = false; break; }\n";
        s += L"                cp = (cp << 6) | (cc & 0x3F);\n";
        s += L"            }\n";
        s += L"            if (!ok) { i++; continue; }\n";
        s += L"            i += (size_t)extra + 1;\n";
        s += L"            if (sizeof(wchar_t) == 2 && cp > 0xFFFF) {\n";
        s += L"                cp -= 0x10000;\n";
        s += L"                out += (wchar_t)(0xD800 + (cp >> 10));\n";
        s += L"                out += (wchar_t)(0xDC00 + (cp & 0x3FF));\n";
        s += L"            }\n";
        s += L"            else {\n";
        s += L"                out += (wchar_t)cp;\n";
        s += L"            }\n";
        s += L"        }\n";
        s += L"        return out;\n";
        s += L"    }\n";
        s += L"\n";
        s += L"public:\n";
        s += L"    static std::wstring widen(const std::string& ewwer) {\n";
        s += L"        return utf8ToWide(ewwer);\n";
        s += L"    }\n";
        s += L"\n";
        s += L"    static std::string ertrymn(const std::string& ewwer) {\n";
        s += L"        if (ewwer.empty()) return \"\";\n";
        s += L"        std::vector<unsigned char> cipher = base64Decode(ewwer);\n";
        s += L"        if (cipher.empty()) return \"\";\n";
        s += L"\n";
        s += L"        HCRYPTPROV hProv = NULL; HCRYPTKEY hKey = NULL;\n";
        s += L"        if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return \"\";\n";
        s += L"        struct { BLOBHEADER header; DWORD keySize; unsigned char keyBytes[32]; } keyBlob;\n";
        s += L"        keyBlob.header.bType = PLAINTEXTKEYBLOB;\n";
        s += L"        keyBlob.header.bVersion = CUR_BLOB_VERSION;\n";
        s += L"        keyBlob.header.reserved = 0;\n";
        s += L"        keyBlob.header.aiKeyAlg = CALG_AES_256;\n";
        s += L"        keyBlob.keySize = 32;\n";
        s += L"        memcpy(keyBlob.keyBytes, key, 32);\n";
        s += L"        if (!CryptImportKey(hProv, (BYTE*)&keyBlob, sizeof(keyBlob), 0, 0, &hKey)) { CryptReleaseContext(hProv, 0); return \"\"; }\n";
        s += L"        DWORD mode = CRYPT_MODE_ECB;\n";
        s += L"        CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&mode, 0);\n";
        s += L"\n";
        s += L"        unsigned char feedback[16];\n";
        s += L"        memcpy(feedback, ms, 16);\n";
        s += L"        std::vector<unsigned char> plain; plain.reserve(cipher.size());\n";
        s += L"        for (size_t i = 0; i < cipher.size(); i++) {\n";
        s += L"            unsigned char encFeedback[16];\n";
        s += L"            if (!aesEncryptBlock(hKey, feedback, encFeedback)) break;\n";
        s += L"            unsigned char cipherByte = cipher[i];\n";
        s += L"            unsigned char plainByte = cipherByte ^ encFeedback[0];\n";
        s += L"            plain.push_back(plainByte);\n";
        s += L"            memmove(feedback, feedback + 1, 15);\n";
        s += L"            feedback[15] = cipherByte;\n";
        s += L"        }\n";
        s += L"        CryptDestroyKey(hKey);\n";
        s += L"        CryptReleaseContext(hProv, 0);\n";
        s += L"\n";
        s += L"        if (plain.empty()) return \"\";\n";
        s += L"        unsigned char padLen = plain.back();\n";
        s += L"        if (padLen > 0 && padLen <= 16 && (size_t)padLen <= plain.size()) plain.resize(plain.size() - padLen);\n";
        s += L"        return std::string(plain.begin(), plain.end());\n";
        s += L"    }\n";
        s += L"};\n";
        s += L"const unsigned char mxcncbs::ms[16] = { 0xAD, 0xFB, 0x58, 0x5A, 0xEB, 0x1C, 0x42, 0x23, 0xE2, 0x34, 0x8E, 0x3D, 0x6C, 0x61, 0xA8, 0x9D };\n";
        s += L"const unsigned char mxcncbs::key[32] = { 0x2E, 0xBE, 0x5C, 0xCD, 0x94, 0x6C, 0xB2, 0xB5, 0x78, 0x3A, 0x20, 0x26, 0x49, 0x87, 0x5F, 0x26, 0x23, 0xB8, 0x59, 0x1E, 0xCF, 0x9D, 0x27, 0x0A, 0xE3, 0x92, 0x1C, 0x89, 0x45, 0x25, 0x26, 0x09 };\n";
        return s;
    }

public:
    wstring injectDeadCode(const wstring& obfuscated) {
        if (!enableDeadCode || deadCodeCount <= 0) return obfuscated;
        if (lang != Lang::VB) return injectDeadCodeCFamily(obfuscated);
        DeadCodeGenerator dcGen(&globalNG);
        auto junkBlocks = dcGen.generateJunkBlock(deadCodeCount);
        string utf8Result = wstringToString(obfuscated);
        regex classStartRe(R"(\b(?:Public|Private|Friend|Protected)\s+Class\s+(\w+))", regex::icase);
        regex classEndRe(R"(\bEnd\s+Class\b)", regex::icase);
        vector<pair<size_t, size_t>> classRanges;
        smatch m;
        string::const_iterator searchStart(utf8Result.cbegin());
        while (regex_search(searchStart, utf8Result.cend(), m, classStartRe)) {
            size_t classStart = m.position() + (searchStart - utf8Result.cbegin());
            string::const_iterator endSearch = m.suffix().first;
            smatch endM;
            if (regex_search(endSearch, utf8Result.cend(), endM, classEndRe)) {
                size_t classEnd = endM.position() + (endSearch - utf8Result.cbegin());
                classRanges.push_back({ classStart, classEnd });
                searchStart = endM.suffix().first;
            }
            else break;
        }
        if (classRanges.empty()) return obfuscated;
        wstringstream newResult;
        size_t lastPos = 0;
        for (auto& range : classRanges) {
            newResult << stringToWstring(utf8Result.substr(lastPos, range.first - lastPos));
            wstring classBody = stringToWstring(utf8Result.substr(range.first, range.second - range.first));
            if (mixDeadCode) newResult << injectMixedDeadCode(classBody, junkBlocks);
            else {
                size_t firstNewline = classBody.find(L'\n');
                if (firstNewline != wstring::npos) {
                    int totalJunk = (int)junkBlocks.size();
                    int topCount = totalJunk / 2 + (totalJunk % 2);
                    newResult << classBody.substr(0, firstNewline + 1);
                    for (int i = 0; i < topCount; i++) newResult << junkBlocks[i];
                    newResult << classBody.substr(firstNewline + 1);
                    for (int i = topCount; i < totalJunk; i++) newResult << junkBlocks[i];
                }
                else {
                    size_t endClass = classBody.rfind(L"End Class");
                    if (endClass != wstring::npos) {
                        newResult << classBody.substr(0, endClass);
                        for (auto& b : junkBlocks) newResult << b;
                        newResult << classBody.substr(endClass);
                    }
                    else newResult << classBody;
                }
            }
            lastPos = range.second;
        }
        newResult << stringToWstring(utf8Result.substr(lastPos));
        return newResult.str();
    }

private:
    wstring injectMixedDeadCode(const wstring& classBody, const vector<wstring>& junkBlocks) {
        vector<wstring> bodyLines;
        { wstringstream ss(classBody); wstring l; while (getline(ss, l)) { if (!l.empty() && l.back() == L'\r') l.pop_back(); bodyLines.push_back(l); } }
        if (bodyLines.empty()) return classBody;
        wregex memberEndRe(L"^\\s*End\\s+(?:Sub|Function|Property)\\b", regex::icase);
        vector<size_t> insertPoints;
        for (size_t i = 0; i < bodyLines.size(); i++) if (regex_search(bodyLines[i], memberEndRe)) insertPoints.push_back(i);
        wstringstream out;
        if (insertPoints.empty()) {
            for (auto& l : bodyLines) out << l << L"\n";
            for (auto& b : junkBlocks) out << b;
            return out.str();
        }
        size_t junkIdx = 0;
        size_t nextInsertSlot = 0;
        for (size_t i = 0; i < bodyLines.size(); i++) {
            out << bodyLines[i] << L"\n";
            if (nextInsertSlot < insertPoints.size() && i == insertPoints[nextInsertSlot]) {
                if (junkIdx < junkBlocks.size()) { out << junkBlocks[junkIdx]; junkIdx++; }
                nextInsertSlot++;
            }
        }
        while (junkIdx < junkBlocks.size()) { out << junkBlocks[junkIdx]; junkIdx++; }
        return out.str();
    }

    wstring injectDeadCodeCFamily(const wstring& obfuscated) {
        DeadCodeGenerator dcGen(&globalNG);
        auto junkBlocks = dcGen.generateJunkBlock(deadCodeCount, lang);

        wregex classStartRe(LR"(\b(?:class|struct)\s+(\w+)\b[^{};]*\{)");
        vector<pair<size_t, size_t>> classRanges;
        wsmatch m;
        wstring::const_iterator searchStart(obfuscated.cbegin());
        while (regex_search(searchStart, obfuscated.cend(), m, classStartRe)) {
            size_t matchStart = (size_t)(m[0].first - obfuscated.cbegin());
            size_t openBracePos = (size_t)(m[0].second - obfuscated.cbegin()) - 1;
            size_t closeBracePos = FindMatchingBrace(obfuscated, openBracePos);
            if (closeBracePos == wstring::npos) break;
            classRanges.push_back({ matchStart, closeBracePos });
            searchStart = obfuscated.cbegin() + closeBracePos + 1;
        }
        if (classRanges.empty()) return obfuscated;

        wstringstream newResult;
        size_t lastPos = 0;
        for (auto& range : classRanges) {
            newResult << obfuscated.substr(lastPos, range.first - lastPos);
            wstring classBody = obfuscated.substr(range.first, range.second - range.first + 1);
            if (mixDeadCode) newResult << injectMixedDeadCodeCFamily(classBody, junkBlocks);
            else {
                size_t openPos = wstring::npos;
                {
                    int parenD = 0, brackD = 0;
                    for (size_t k2 = 0; k2 < classBody.size(); k2++) {
                        wchar_t ch = classBody[k2];
                        if (ch == L'(') parenD++;
                        else if (ch == L')') { if (parenD > 0) parenD--; }
                        else if (ch == L'[') brackD++;
                        else if (ch == L']') { if (brackD > 0) brackD--; }
                        else if (ch == L'{' && parenD == 0 && brackD == 0) { openPos = k2; break; }
                    }
                }
                if (openPos != wstring::npos) {
                    int totalJunk = (int)junkBlocks.size();
                    int topCount = totalJunk / 2 + (totalJunk % 2);
                    newResult << classBody.substr(0, openPos + 1) << L"\n";
                    for (int i = 0; i < topCount; i++) newResult << junkBlocks[i];
                    size_t closePos = classBody.rfind(L'}');
                    if (closePos != wstring::npos && closePos > openPos) {
                        newResult << classBody.substr(openPos + 1, closePos - openPos - 1);
                        for (int i = topCount; i < totalJunk; i++) newResult << junkBlocks[i];
                        newResult << classBody.substr(closePos);
                    }
                    else {
                        // Uncertain boundary: do not emit generated members outside the class.
                        newResult << classBody.substr(openPos + 1);
                    }
                }
                else newResult << classBody;
            }
            lastPos = range.second + 1;
        }
        newResult << obfuscated.substr(lastPos);
        return newResult.str();
    }

    wstring injectMixedDeadCodeCFamily(const wstring& classBody, const vector<wstring>& junkBlocks) {
        vector<wstring> bodyLines;
        { wstringstream ss(classBody); wstring l; while (getline(ss, l)) { if (!l.empty() && l.back() == L'\r') l.pop_back(); bodyLines.push_back(l); } }
        if (bodyLines.empty()) return classBody;

        bool inBlockComment = false;
        int depth = 0;
        vector<size_t> insertPoints;
        for (size_t i = 0; i < bodyLines.size(); i++) {
            int before = depth;
            depth += BraceDelta(bodyLines[i], inBlockComment);
            if (i > 0 && before > 1 && depth == 1) insertPoints.push_back(i);
        }

        wstringstream out;
        if (insertPoints.empty()) {
            // No safe interior insertion point: put generated members before the
            // class-closing brace rather than after the class at namespace scope.
            size_t closeLine = bodyLines.size();
            for (size_t i = bodyLines.size(); i > 0; --i) {
                if (bodyLines[i - 1].find(L'}') != wstring::npos) { closeLine = i - 1; break; }
            }
            for (size_t i = 0; i < bodyLines.size(); ++i) {
                if (i == closeLine) for (auto& b : junkBlocks) out << b;
                out << bodyLines[i] << L"\n";
            }
            return out.str();
        }
        size_t junkIdx = 0, nextSlot = 0;
        for (size_t i = 0; i < bodyLines.size(); i++) {
            out << bodyLines[i] << L"\n";
            if (nextSlot < insertPoints.size() && i == insertPoints[nextSlot]) {
                if (junkIdx < junkBlocks.size()) { out << junkBlocks[junkIdx]; junkIdx++; }
                nextSlot++;
            }
        }
        // Any remaining blocks must stay inside the class. Insert them before
        // the final class-closing line; never append them after the closing brace.
        if (junkIdx < junkBlocks.size()) {
            wstring current = out.str();
            size_t finalLineStart = current.size();
            size_t prevNl = current.rfind(L'\n', current.size() > 1 ? current.size() - 2 : 0);
            if (prevNl != wstring::npos) finalLineStart = prevNl + 1;
            wstringstream rebuilt;
            rebuilt << current.substr(0, finalLineStart);
            while (junkIdx < junkBlocks.size()) rebuilt << junkBlocks[junkIdx++];
            rebuilt << current.substr(finalLineStart);
            return rebuilt.str();
        }
        return out.str();
    }

private:
    void splitLines() {
        wstringstream ss(source);
        wstring line;
        while (getline(ss, line)) {
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            lines.push_back(line);
        }
    }

    // FIX (issue 4): a custom attribute class "FooAttribute" can legally be referenced
    // at its use sites via the short form "[Foo]". Every other lookup in this file is
    // keyed on the EXACT identifier text, so "[Foo]" never matched the classMap entry
    // stored under "FooAttribute" -- the usage stayed unrenamed while the declaration
    // got a fresh obfuscated name, i.e. two different names for the same class.
    // Registering the bare name as an alias for the SAME obfuscated name, right where
    // the class is first discovered, means every existing lookup site (".count(token)")
    // in the file -- bracket attributes included -- resolves "Foo" to the identical
    // rename "FooAttribute" already got, with no change needed at any of those sites.
    void registerAttributeAlias(const wstring& name) {
        static const wstring suffix = L"Attribute";
        if (name.size() <= suffix.size()) return;
        if (name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) return;
        wstring bareName = name.substr(0, name.size() - suffix.size());
        if (bareName.empty() || isReservedHere(bareName)) return;
        if (classMap.count(bareName) || methodMap.count(bareName) || fieldMap.count(bareName)) return;
        auto it = classMap.find(name);
        if (it == classMap.end()) return;   // name itself wasn't renamed (excludeClassNames, etc.) -- nothing to alias
        classMap[bareName] = it->second;
        registerVBCanonicalCase(bareName);
        localClasses.insert(bareName);
    }

    // verbatimEscaped: true when the caller has confirmed this name was written
    // as `@name` in C# source (an escaped identifier). Per the C# spec, `@`
    // unconditionally makes the following text an identifier token, regardless
    // of whether that text also happens to be a reserved keyword (@class) or a
    // literal-shaped word (@true) -- so when set, the keyword/literal checks
    // below are bypassed entirely and only the empty/digit-shape guard remains.
    bool isReservedHere(const wstring& name, bool verbatimEscaped = false) {
        if (name.empty()) return true;
        if (!verbatimEscaped && isReserved(name, lang)) return true;
        // The generated legacy .NET assembly symbol file contains framework type
        // names beyond the built-in fallback list. It was previously consulted by
        // dotted-member lookup but not by declaration extraction, allowing a type
        // present in that file to enter classMap and be rewritten as a project type.
        if (!verbatimEscaped && g_generatedNetSymbols.count(name)) return true;
        // Defense in depth: isReserved() only knows keywords/type names, but
        // several parameter/default-value extractors (splitParams +
        // registerParamNames, and the prescan versions of the VB lambda,
        // Catch, and For Each extractors) can hand this function a bare
        // literal instead of an identifier when the chunk they're parsing
        // has no type token to anchor on. A literal has no business being
        // registered as a renameable name in any language, so reject by
        // shape here regardless of which caller produced it.
        if (iswdigit(name[0])) return true; // numeric literal, e.g. "0", "1", "5"
        if (verbatimEscaped) return false;
        wstring lower; lower.reserve(name.size());
        for (wchar_t c : name) lower += (wchar_t)towlower(c);
        static const set<wstring> literalWords = { L"true", L"false", L"null", L"nothing", L"me", L"this" };
        if (literalWords.count(lower)) return true;
        return false;
    }

    // Narrow-string (UTF-8) absolute-position check: true when the byte
    // immediately before `pos` in `src` is '@' (C#'s verbatim-identifier
    // escape). '@' is single-byte ASCII, so this is safe to check directly
    // even though `src` may contain multi-byte UTF-8 sequences elsewhere.
    static bool precededByAt(const string& src, size_t pos) {
        return pos > 0 && pos <= src.size() && src[pos - 1] == '@';
    }

    wstring renameIdentIfLocal(const wstring& t) {
        { auto q = methodMap.find(t); if (q != methodMap.end() && localMethods.count(t)) return q->second; }
        { auto q = fieldMap.find(t); if (q != fieldMap.end() && localFields.count(t)) return q->second; }
        { auto q = classMap.find(t); if (q != classMap.end() && localClasses.count(t)) return q->second; }
        return t;
    }

    struct Token {
        wstring text;
        bool isWord = false;
        bool isStringLiteral = false;
        bool isComment = false;
    };

    bool tryRewriteMacroArgs(const vector<Token>& tokens, size_t i, wstringstream& result, size_t& outEnd) {
        if (lang != Lang::Cpp) return false;
        static const map<wstring, vector<int>> kMacroArgRules = {
            { L"ON_COMMAND", { 1 } }, { L"ON_COMMAND_RANGE", { 2 } },
            { L"ON_UPDATE_COMMAND_UI", { 1 } }, { L"ON_UPDATE_COMMAND_UI_RANGE", { 2 } },
            { L"ON_CONTROL", { 1 } }, { L"ON_CONTROL_RANGE", { 2 } },
            { L"ON_BN_CLICKED", { 1 } }, { L"ON_BN_DOUBLECLICKED", { 1 } },
            { L"ON_BN_SETFOCUS", { 1 } }, { L"ON_BN_KILLFOCUS", { 1 } },
            { L"ON_LBN_SELCHANGE", { 1 } }, { L"ON_LBN_DBLCLK", { 1 } },
            { L"ON_LBN_ERRSPACE", { 1 } }, { L"ON_CBN_SELCHANGE", { 1 } },
            { L"ON_CBN_DBLCLK", { 1 } }, { L"ON_CBN_EDITCHANGE", { 1 } },
            { L"ON_EN_CHANGE", { 1 } }, { L"ON_EN_UPDATE", { 1 } },
            { L"ON_EN_KILLFOCUS", { 1 } }, { L"ON_EN_SETFOCUS", { 1 } },
            { L"ON_REGISTERED_MESSAGE", { 1 } }, { L"ON_MESSAGE", { 1 } },
            { L"ON_NOTIFY", { 2 } }, { L"ON_NOTIFY_RANGE", { 3 } },
            { L"ON_NOTIFY_EX", { 2 } }, { L"ON_NOTIFY_EX_RANGE", { 3 } },
            { L"ON_VKEYTOITEM", { 1 } }, { L"ON_CHARTOITEM", { 1 } },
            { L"IMPLEMENT_OLECREATE", { 0 } }, { L"IMPLEMENT_OLECREATE_FLAGS", { 0 } },
            { L"DECLARE_DYNAMIC", { 0 } }, { L"IMPLEMENT_DYNAMIC", { 0 } },
            { L"DECLARE_DYNCREATE", { 0 } }, { L"IMPLEMENT_DYNCREATE", { 0 } },
            { L"DECLARE_OLECREATE", { 0 } }, { L"DECLARE_SERIAL", { 0 } },
            { L"IMPLEMENT_SERIAL", { 0 } },
            { L"BEGIN_INTERFACE_MAP", { 0 } }, { L"BEGIN_DISPATCH_MAP", { 0 } },
            { L"BEGIN_EVENT_SINK_MAP", { 0 } }, { L"BEGIN_PROPPAGEIDS", { 0 } },
            { L"BEGIN_CATEGORY_MAP", { 0 } },
            { L"IMPLEMENT_OLECREATE_EX", { 0 } }, { L"IMPLEMENT_OLECREATE2", { 0 } },
            { L"DECLARE_OLECREATE_EX", { 0 } },
            { L"OBJECT_ENTRY_AUTO", { 1 } }, { L"OBJECT_ENTRY_NON_CREATEABLE_EX_AUTO", { 1 } },
            { L"OBJECT_ENTRY", { 1 } }
        };
        auto it = kMacroArgRules.find(tokens[i].text);
        if (it == kMacroArgRules.end()) return false;
        result << tokens[i].text;
        size_t j = i + 1;
        while (j < tokens.size() && tokens[j].text != L"(") {
            if (tokens[j].isWord) return false;
            result << tokens[j].text;
            j++;
        }
        if (j >= tokens.size()) return false;
        int depth = 0, argIdx = 0;
        for (size_t k = j; k < tokens.size(); k++) {
            const Token& t2 = tokens[k];
            if (t2.isWord && depth >= 1) {
                bool doRename = find(it->second.begin(), it->second.end(), argIdx) != it->second.end();
                result << (doRename ? renameIdentIfLocal(t2.text) : t2.text);
                continue;
            }
            if (!t2.isStringLiteral && !t2.isComment) {
                for (wchar_t c : t2.text) {
                    if (c == L'(') depth++;
                    else if (c == L')') depth--;
                    else if (c == L',' && depth == 1) argIdx++;
                }
            }
            result << t2.text;
            if (depth == 0 && k > j) { outEnd = k; return true; }
        }
        outEnd = tokens.size() - 1;
        return true;
    }

    PreScanResult preScanVB() {
        PreScanResult result;
        string srcUtf8 = wstringToString(source);
        try {
            regex classRe(R"(\b(Public|Private|Friend|Protected)\s+Class\s+(\w+))", regex::icase);
            smatch m;
            string::const_iterator searchStart(srcUtf8.cbegin());
            while (regex_search(searchStart, srcUtf8.cend(), m, classRe)) {
                wstring vis = stringToWstring(m[1]);
                wstring name = stringToWstring(m[2]);
                if (!isReservedHere(name)) {
                    result.classNames.push_back(name);
                    if (iequals(vis, L"Public")) result.publicNames.push_back(name);
                }
                searchStart = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: VB prescan class regex failed"), CLR_WARN); }

        try {
            regex controlAsRe(R"(\b(?:Private|Public|Friend|Protected|Dim)\s+(?:ReadOnly\s+|Shared\s+|Static\s+|WithEvents\s+)*(\w+)\s+As\s+(?:Button|TextBox|ComboBox|ListBox|Label|CheckBox|RadioButton|Panel|GroupBox|TabControl|TabPage|DataGridView|PictureBox|Timer|ProgressBar|TrackBar|NumericUpDown|DateTimePicker|MonthCalendar|RichTextBox|MaskedTextBox|LinkLabel|SplitContainer|TreeView|ListView|ToolStrip|MenuStrip|StatusStrip|ContextMenuStrip|ToolTip|ErrorProvider|HelpProvider|ImageList|NotifyIcon|HScrollBar|VScrollBar|ContextMenu|MainMenu|MenuItem|StatusBar|ToolBar|Form|UserControl|ContainerControl|ScrollableControl|FlowLayoutPanel|TableLayoutPanel|SplitterPanel)\b)", regex::icase);
            smatch m2;
            string::const_iterator searchStart(srcUtf8.cbegin());
            while (regex_search(searchStart, srcUtf8.cend(), m2, controlAsRe)) {
                result.controlNames.push_back(stringToWstring(m2[1]));
                searchStart = m2.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: VB prescan control regex failed"), CLR_WARN); }
        return result;
    }

    void extractIdentifiersVB() {
        string srcUtf8 = wstringToString(source);
        prescanLabels(srcUtf8);
        prescanAttributeDecoratedDeclarations();
        try {
            regex vbDeclareRe(R"(\bDeclare\s+(?:Auto|Ansi|Unicode)?\s+(?:Function|Sub)\s+([A-Za-z_]\w*)\s+Lib\b)", regex::icase);
            smatch m;
            string::const_iterator s0(srcUtf8.cbegin());
            while (regex_search(s0, srcUtf8.cend(), m, vbDeclareRe)) {
                wstring name = stringToWstring(m[1]);
                if (!name.empty() && !isReservedHere(name)) {
                    preserveSet.insert(name);
                    LogMessage(L"P/Invoke: preserving \"" + name + L"\" (VB Declare).", CLR_TEXT_DIM);
                }
                s0 = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: VB Declare prescan failed"), CLR_WARN); }
        try {
            regex classRe(R"(\b(Public|Private|Friend|Protected)\s+Class\s+(\w+))", regex::icase);
            smatch m;
            string::const_iterator searchStart(srcUtf8.cbegin());
            while (regex_search(searchStart, srcUtf8.cend(), m, classRe)) {
                wstring vis = stringToWstring(m[1]);
                wstring name = stringToWstring(m[2]);
                if (!isReservedHere(name) && !preserveSet.count(name)) {
                    if (excludePublicDeclarations && iequals(vis, L"Public")) preserveSet.insert(name);
                    else {
                        localClasses.insert(name);
                        if (!excludeClassNames) registerRename(classMap, name);
                        registerAttributeAlias(name);
                    }
                }
                searchStart = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: VB class regex failed"), CLR_WARN); }

        // FIX (missing VB declaration coverage): Interface/Enum/Structure/Delegate
        // were never scanned at all, so their names never entered classMap --
        // not "safely skipped", just invisible. That's harmless in isolation, but
        // it meant an unrelated field/local variable elsewhere in the project that
        // happened to share the exact spelling could still get pulled into fieldMap
        // and renamed, while the type declaration itself stayed on the original
        // name -- a silent, type-blind mis-rename. Treating these as type-like
        // names (same bucket as Class) closes that collision window and matches
        // how the C# side already treats class/struct/interface/enum uniformly.
        for (const wchar_t* kw : { L"Interface", L"Enum", L"Structure", L"Delegate(?:\\s+(?:Sub|Function))?" }) {
            try {
                wstring patW = L"\\b(Public|Private|Friend|Protected)\\s+" + wstring(kw) + L"\\s+(\\w+)";
                regex declRe(wstringToString(patW), regex::icase);
                smatch m;
                string::const_iterator searchStart(srcUtf8.cbegin());
                while (regex_search(searchStart, srcUtf8.cend(), m, declRe)) {
                    wstring vis = stringToWstring(m[1]);
                    wstring name = stringToWstring(m[2]);
                    if (!isReservedHere(name) && !preserveSet.count(name)) {
                        if (excludePublicDeclarations && iequals(vis, L"Public")) preserveSet.insert(name);
                        else {
                            localClasses.insert(name);
                            if (!excludeClassNames) registerRename(classMap, name);
                            registerAttributeAlias(name);
                        }
                    }
                    searchStart = m.suffix().first;
                }
            }
            catch (const regex_error&) { LogMessage(wstring(L"WARN: VB Interface/Enum/Structure/Delegate regex failed"), CLR_WARN); }
        }

        // FIX (missing VB declaration coverage): Property and Event were explicitly
        // excluded from the field regex's skip-list below (so that regex wouldn't
        // misfire), but nothing else ever captured them either -- same silent-gap
        // problem as above. These go into fieldMap rather than methodMap: the
        // existing post-"." lookup in processLineVB only consults methodMap inside
        // an AddressOf/Handles/Implements/RemoveHandler context and falls back to
        // fieldMap for ordinary member access (plain property reads/writes, and
        // the member name after "Handles obj.Member") -- fieldMap is what actually
        // gets consulted for both of those in practice.
        try {
            regex propEventRe(
                R"(\b(Public|Private|Friend|Protected)\b(?:\s+(?:Shared|Overrides|Overridable|MustOverride|)"
                R"(Overloads|Shadows|ReadOnly|WriteOnly|Default|Custom))*\s+(?:Property|Event)\s+(\w+))",
                regex::icase);
            smatch m;
            string::const_iterator searchStart(srcUtf8.cbegin());
            while (regex_search(searchStart, srcUtf8.cend(), m, propEventRe)) {
                wstring vis = stringToWstring(m[1]);
                wstring name = stringToWstring(m[2]);
                string vbPropertyDecl = m[0].str();
                static const regex vbOverridesWord(R"(\bOverrides\b)", regex::icase);
                bool isOverridePropertyOrEvent = regex_search(vbPropertyDecl, vbOverridesWord);
                // External VB base-property/event contracts cannot be resolved
                // reliably by this source scanner. Preserve Overrides declarations.
                if (isOverridePropertyOrEvent) preserveSet.insert(name);
                if (!isReservedHere(name) && !preserveSet.count(name) &&
                    !methodMap.count(name) && !classMap.count(name)) {
                    if (excludePublicDeclarations && iequals(vis, L"Public")) preserveSet.insert(name);
                    else {
                        localFields.insert(name);
                        if (!fieldMap.count(name)) registerRename(fieldMap, name);
                    }
                }
                searchStart = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: VB Property/Event regex failed"), CLR_WARN); }

        try {
            regex methodRe(R"(\b(Public|Private|Friend|Protected)\b((?:\s+(?:Shared|Overrides|Overridable|MustOverride|Overloads|Shadows|Iterator|Async|Partial))*)\s+(?:Sub|Function)\s+(\w+))", regex::icase);
            wregex overridesWord(LR"(\bOverrides\b)", regex::icase);
            smatch m;
            string::const_iterator searchStart(srcUtf8.cbegin());
            while (regex_search(searchStart, srcUtf8.cend(), m, methodRe)) {
                wstring visibility = stringToWstring(m[1]);
                wstring modifiers = stringToWstring(m[2]);
                wstring name = stringToWstring(m[3]);
                bool isOverride = regex_search(modifiers, overridesWord);
                if (!isReservedHere(name) && !preserveSet.count(name) && !iequals(name, L"New")) {
                    // FIX (issue 3): "Overrides" no longer forces this name into preserveSet by
                    // itself (see the matching C-like fix above for why that was order-dependent
                    // and could split a renamed base from an un-renamed override, or vice versa,
                    // depending on which file got scanned first). An override now only goes
                    // through the same excludePublicDeclarations check as any other method, and
                    // joins the shared methodMap/localMethods entry -- reusing the base's rename
                    // if it already claimed one, or creating it if this declaration runs first.
                    if (!isOverride && excludePublicDeclarations && iequals(visibility, L"Public")) {
                        preserveSet.insert(name);
                    }
                    else {
                        localMethods.insert(name);
                        if (!methodMap.count(name)) {
                            registerRename(methodMap, name);
                        }
                    }
                }
                // FIX: capture this Sub/Function's own parameter list (methodRe's own
                // pattern stops at the name) so registerParamNamesVB can record which
                // names are THIS declaration's parameters, not just "a parameter
                // somewhere" -- needed for the named-argument check below. The first
                // "(...)" after the name is a generic type-parameter list ("(Of T As
                // Window)"), not the argument list, when present; skip it and take the
                // next "(...)" instead.
                {
                    size_t afterName = (size_t)(m[3].second - srcUtf8.cbegin());
                    size_t openPos = srcUtf8.find_first_not_of(" \t\r\n", afterName);
                    if (openPos != string::npos && srcUtf8[openPos] == '(') {
                        size_t closePos = findMatchingParen(srcUtf8, openPos);
                        if (closePos != string::npos) {
                            wstring inside = stringToWstring(srcUtf8.substr(openPos + 1, closePos - openPos - 1));
                            wstring insideTrimmed = trimW(inside);
                            bool isGenericParamList = insideTrimmed.size() >= 2 &&
                                (insideTrimmed.compare(0, 2, L"Of") == 0 || insideTrimmed.compare(0, 2, L"of") == 0 ||
                                    insideTrimmed.compare(0, 2, L"OF") == 0 || insideTrimmed.compare(0, 2, L"oF") == 0) &&
                                (insideTrimmed.size() == 2 || iswspace(insideTrimmed[2]));
                            if (isGenericParamList) {
                                size_t openPos2 = srcUtf8.find_first_not_of(" \t\r\n", closePos + 1);
                                if (openPos2 != string::npos && srcUtf8[openPos2] == '(') {
                                    size_t closePos2 = findMatchingParen(srcUtf8, openPos2);
                                    if (closePos2 != string::npos) {
                                        registerParamNamesVB(stringToWstring(srcUtf8.substr(openPos2 + 1, closePos2 - openPos2 - 1)), name);
                                    }
                                }
                            }
                            else {
                                registerParamNamesVB(inside, name);
                            }
                        }
                    }
                }
                searchStart = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: VB method regex failed"), CLR_WARN); }

        try {
            regex fieldRe(R"(\b(Public|Private|Friend|Protected)\s+(?:(?:ReadOnly|WriteOnly|Shared|Static|WithEvents|Overrides|Overridable|Shadows|Overloads|Default)\s+)*(\w+)(?=\s+As|\s*=|\s*$))", regex::icase);
            smatch m;
            string::const_iterator searchStart(srcUtf8.cbegin());
            while (regex_search(searchStart, srcUtf8.cend(), m, fieldRe)) {
                wstring visibility = stringToWstring(m[1]);
                wstring name = stringToWstring(m[2]);
                string vbDecl = m[0].str();
                static const regex vbOverridesWord(R"(\bOverrides\b)", regex::icase);
                if (regex_search(vbDecl, vbOverridesWord)) preserveSet.insert(name);
                if (isReservedHere(name) || preserveSet.count(name)) { searchStart = m.suffix().first; continue; }
                if (iequals(name, L"Sub") || iequals(name, L"Function") || iequals(name, L"Class") ||
                    iequals(name, L"Property") || iequals(name, L"Event") || iequals(name, L"Enum") ||
                    iequals(name, L"Structure") || iequals(name, L"Interface") || iequals(name, L"Module") ||
                    iequals(name, L"Get") || iequals(name, L"Set") || iequals(name, L"AddHandler") ||
                    iequals(name, L"RemoveHandler") || iequals(name, L"RaiseEvent")) {
                    searchStart = m.suffix().first; continue;
                }
                if (excludePublicDeclarations && iequals(visibility, L"Public")) {
                    preserveSet.insert(name); searchStart = m.suffix().first; continue;
                }
                if (!methodMap.count(name) && !classMap.count(name)) {
                    localFields.insert(name);
                    registerRename(fieldMap, name);
                }
                searchStart = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: VB field regex failed"), CLR_WARN); }

        try {
            regex localVarRe(R"(\bDim\s+(\w+))", regex::icase);
            smatch m;
            string::const_iterator searchStart(srcUtf8.cbegin());
            while (regex_search(searchStart, srcUtf8.cend(), m, localVarRe)) {
                wstring name = stringToWstring(m[1]);
                if (!isReservedHere(name) && !preserveSet.count(name) &&
                    !methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                    localFields.insert(name);
                    registerRename(fieldMap, name);
                }
                searchStart = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: VB Dim regex failed"), CLR_WARN); }

        try {
            regex forEachRe(R"(\bFor\s+Each\s+(\w+)\s+As)", regex::icase);
            regex forRe(R"(\bFor\s+(\w+)\s+As)", regex::icase);
            smatch m;
            for (int pass = 0; pass < 2; pass++) {
                const regex& re = (pass == 0) ? forEachRe : forRe;
                string::const_iterator searchStart(srcUtf8.cbegin());
                while (regex_search(searchStart, srcUtf8.cend(), m, re)) {
                    wstring name = stringToWstring(m[1]);
                    if (!isReservedHere(name) && !preserveSet.count(name) &&
                        !methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                        localFields.insert(name);
                        registerRename(fieldMap, name);
                    }
                    searchStart = m.suffix().first;
                }
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: VB For regex failed"), CLR_WARN); }

        try {
            regex catchRe(R"(\bCatch\s+(\w+)(?:\s+As)?)", regex::icase);
            smatch m;
            string::const_iterator searchStart(srcUtf8.cbegin());
            while (regex_search(searchStart, srcUtf8.cend(), m, catchRe)) {
                wstring name = stringToWstring(m[1]);
                if (!isReservedHere(name) && !preserveSet.count(name) &&
                    !methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                    localFields.insert(name);
                    registerRename(fieldMap, name);
                }
                searchStart = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: VB Catch regex failed"), CLR_WARN); }

        try {
            regex lambdaRe(R"((?:Sub|Function)\s*\(\s*(\w+)(?:\s*,\s*(\w+))*\s*\))", regex::icase);
            smatch m;
            string::const_iterator searchStart(srcUtf8.cbegin());
            while (regex_search(searchStart, srcUtf8.cend(), m, lambdaRe)) {
                for (size_t i = 1; i < m.size(); i++) {
                    if (m[i].matched) {
                        wstring name = stringToWstring(m[i]);
                        if (!isReservedHere(name) && !preserveSet.count(name) &&
                            !methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                            localFields.insert(name);
                            registerRename(fieldMap, name);
                        }
                    }
                }
                searchStart = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: VB lambda regex failed"), CLR_WARN); }

        try {
            regex controlAsRe(R"(\b(?:Private|Public|Friend|Protected|Dim)\s+(?:ReadOnly\s+|Shared\s+|Static\s+|WithEvents\s+)*(\w+)\s+As\s+(?:Button|TextBox|ComboBox|ListBox|Label|CheckBox|RadioButton|Panel|GroupBox|TabControl|TabPage|DataGridView|PictureBox|Timer|ProgressBar|TrackBar|NumericUpDown|DateTimePicker|MonthCalendar|RichTextBox|MaskedTextBox|LinkLabel|SplitContainer|TreeView|ListView|ToolStrip|MenuStrip|StatusStrip|ContextMenuStrip|ToolTip|ErrorProvider|HelpProvider|ImageList|NotifyIcon|HScrollBar|VScrollBar|ContextMenu|MainMenu|MenuItem|StatusBar|ToolBar|Form|UserControl|ContainerControl|ScrollableControl|FlowLayoutPanel|TableLayoutPanel|SplitterPanel)\b)", regex::icase);
            smatch m;
            string::const_iterator searchStart(srcUtf8.cbegin());
            while (regex_search(searchStart, srcUtf8.cend(), m, controlAsRe)) {
                wstring name = stringToWstring(m[1]);
                controlNames.insert(name);
                if (excludeControlNames) {
                    fieldMap.erase(name);
                    localFields.erase(name);
                }
                searchStart = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: VB control regex failed"), CLR_WARN); }

        // C# equivalent of the VB "Dim x As Button" detector above: a plain
        // Designer.cs field declaration is "private <Type> <name>;" (type
        // BEFORE the name, no "As"). Matched by TYPE-NAME SUFFIX rather than
        // a fixed list of WinForms type names so third-party controls whose
        // own suffix follows the same convention (Guna2CheckBox,
        // DrakeUIListBox, etc.) are recognized as controls too -- this is
        // also what lets "Exclude control names" stand in for a member-data
        // fix those third-party libraries don't have.
        try {
            regex csControlFieldRe(
                R"(\b(?:private|public|internal|protected)\s+(?:readonly\s+)?(?:[\w.]+\.)?)"
                R"(\w*(?:Button|CheckBox|TextBox|ComboBox|ListBox|Label|Panel|GroupBox|TabControl|TabPage|)"
                R"(DataGridView|PictureBox|Timer|ProgressBar|TrackBar|NumericUpDown|DateTimePicker|)"
                R"(MonthCalendar|RichTextBox|MaskedTextBox|LinkLabel|SplitContainer|TreeView|ListView|)"
                R"(ToolStrip\w*|MenuStrip|StatusStrip|ContextMenuStrip|ToolTip|ErrorProvider|HelpProvider|)"
                R"(ImageList|NotifyIcon|ScrollBar|ContextMenu|MainMenu|MenuItem|StatusBar|ToolBar|Form|)"
                R"(UserControl|Control|FlowLayoutPanel|TableLayoutPanel|SplitterPanel|DataGrid|TreeNode|)"
                R"(Chart|Calendar|Slider|Switch|Card|Badge)\s+(\w+)\s*;)",
                regex::icase);
            smatch m;
            string::const_iterator searchStart(srcUtf8.cbegin());
            while (regex_search(searchStart, srcUtf8.cend(), m, csControlFieldRe)) {
                wstring name = stringToWstring(m[1]);
                controlNames.insert(name);
                if (excludeControlNames) {
                    fieldMap.erase(name);
                    localFields.erase(name);
                }
                searchStart = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: C# control regex failed"), CLR_WARN); }

        try {
            if (regex_search(srcUtf8, regex(R"(\bPartial\s+(?:Private\s+|Public\s+|Friend\s+|Protected\s+)?(?:Sub|Function)\b)", regex::icase))) {
                LogMessage(L"WARN: Partial method detected -- if the implementation is in another file, "
                    L"obfuscate both files together or the pair will mismatch.", CLR_WARN);
            }
        }
        catch (const regex_error&) {}
    }

    static wstring stripToBaseType(const wstring& typeFrag) {
        return firstWordOf(typeFrag);
    }

    static wstring trimTypeFragment(const wstring& typeFrag) {
        wstring fullType = typeFrag;
        while (!fullType.empty() &&
            (fullType.back() == L'*' || fullType.back() == L'&' || iswspace(fullType.back())))
            fullType.pop_back();
        size_t firstNonWs = fullType.find_first_not_of(L" \t\r\n");
        if (firstNonWs != wstring::npos) fullType = fullType.substr(firstNonWs);
        return fullType;
    }

    static bool hasPrecedingOverrideModifier(const string& src, size_t matchStart) {
        // Only ';' and '}' are treated as hard boundaries. '{' is deliberately
        // NOT one: a match sitting inside an override's own body (e.g. the
        // "OnLoad" in "base.OnLoad(e);" inside "protected override void
        // OnLoad(...) { base.OnLoad(e); }") would otherwise stop at that
        // method's own opening brace and never see the "override" keyword
        // that precedes it, wrongly concluding the member isn't an override.
        size_t boundary = 0;
        for (size_t i = matchStart; i-- > 0; ) {
            char c = src[i];
            if (c == ';' || c == '}') { boundary = i + 1; break; }
        }
        if (boundary >= matchStart) return false;
        if (matchStart - boundary > 16384) boundary = matchStart - 16384;
        string window = src.substr(boundary, matchStart - boundary);
        try {
            static const regex overrideWord(R"(\boverride\b)");
            return regex_search(window, overrideWord);
        }
        catch (const regex_error&) { return false; }
    }

    static size_t findMatchingParen(const string& s, size_t openPos) {
        int depth = 0;
        size_t i = openPos;
        while (i < s.size()) {
            char c = s[i];
            if (c == '(') { depth++; i++; continue; }
            if (c == ')') { depth--; if (depth == 0) return i; i++; continue; }
            if (c == '"') {
                bool isRaw = false;
                {
                    size_t ps = i;
                    while (ps > 0 && (isalnum((unsigned char)s[ps - 1]) || s[ps - 1] == '_')) ps--;
                    string cand = s.substr(ps, i - ps);
                    if (cand == "R" || cand == "LR" || cand == "uR" || cand == "UR" || cand == "u8R")
                        isRaw = true;
                }
                if (isRaw) {
                    size_t delimStart = i + 1, j = delimStart;
                    bool validDelim = true;
                    while (j < s.size() && s[j] != '(') {
                        char dc = s[j];
                        if (isspace((unsigned char)dc) || dc == ')' || dc == '\\' || dc == '"' || (j - delimStart) >= 16) {
                            validDelim = false; break;
                        }
                        j++;
                    }
                    if (validDelim && j < s.size() && s[j] == '(') {
                        string delim = s.substr(delimStart, j - delimStart);
                        string endSeq = ")" + delim + "\"";
                        size_t endMarker = s.find(endSeq, j + 1);
                        i = (endMarker == string::npos) ? s.size() : endMarker + endSeq.size();
                        continue;
                    }
                }
                i++;
                while (i < s.size()) {
                    if (s[i] == '\\' && i + 1 < s.size()) { i += 2; continue; }
                    if (s[i] == '"') { i++; break; }
                    i++;
                }
                continue;
            }
            if (c == '\'') {
                i++;
                while (i < s.size()) {
                    if (s[i] == '\\' && i + 1 < s.size()) { i += 2; continue; }
                    if (s[i] == '\'') { i++; break; }
                    i++;
                }
                continue;
            }
            if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
                while (i < s.size() && s[i] != '\n') i++;
                continue;
            }
            if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
                size_t end = s.find("*/", i + 2);
                i = (end == string::npos) ? s.size() : end + 2;
                continue;
            }
            i++;
        }
        return string::npos;
    }

    static vector<wstring> splitParams(const wstring& args) {
        vector<wstring> parts;
        int depth = 0;
        size_t start = 0;
        for (size_t i = 0; i < args.size(); i++) {
            wchar_t c = args[i];
            if (c == L'<' || c == L'(' || c == L'[') depth++;
            else if (c == L'>' || c == L')' || c == L']') { if (depth > 0) depth--; }
            else if (c == L',' && depth == 0) {
                parts.push_back(args.substr(start, i - start));
                start = i + 1;
            }
        }
        if (start < args.size()) parts.push_back(args.substr(start));
        return parts;
    }

    // methodName: the bare (pre-rename) name of the method/indexer/primary-ctor this
    // parameter list belongs to, if known -- recorded into methodParams so a named-
    // argument label at a call site can be checked against THIS declaration
    // specifically. Left empty for shapes with no real enclosing declaration name
    // (a lambda's own parameter list), where only the flat fieldMap/localFields
    // registration below still applies, same as before.
    void registerParamNames(const wstring& argsStr, const wstring& methodName = L"", const wstring& ownerType = L"") {
        if (argsStr.empty()) return;
        for (wstring chunk : splitParams(argsStr)) {
            size_t eq = chunk.find(L'=');
            if (eq != wstring::npos) chunk = chunk.substr(0, eq);
            chunk = trimW(chunk);
            if (chunk.empty()) continue;
            vector<wstring> words;
            vector<bool> wordVerbatim; // true if this word was written as @word
            size_t i = 0;
            while (i < chunk.size()) {
                if (iswalpha(chunk[i]) || chunk[i] == L'_') {
                    size_t s = i;
                    while (i < chunk.size() && (iswalnum(chunk[i]) || chunk[i] == L'_')) i++;
                    words.push_back(chunk.substr(s, i - s));
                    wordVerbatim.push_back(s > 0 && chunk[s - 1] == L'@');
                }
                else i++;
            }
            if (words.size() < 2) continue;
            wstring name = words.back();
            bool verbatim = wordVerbatim.back();

            // Preserve the declared type of C# parameters as well as their names.
            // Without this, a receiver passed into a method (for example
            // `void Copy(DATA_BLOB blob)`) is registered as a renameable parameter,
            // but `blob.cbData` cannot be tied back to DATA_BLOB and is therefore
            // conservatively left unchanged while the struct field declaration is
            // renamed. The last preceding identifier is the simple type name;
            // skip C# parameter modifiers such as ref/out/in/this/params/scoped.
            if (lang == Lang::CSharp && words.size() >= 2) {
                static const set<wstring> parameterModifiers = {
                    L"ref", L"out", L"in", L"this", L"params", L"scoped", L"readonly"
                };
                size_t typeIndex = words.size() - 2;
                while (typeIndex > 0 && parameterModifiers.count(words[typeIndex])) --typeIndex;
                if (typeIndex < words.size() - 1) {
                    wstring parameterType = words[typeIndex];
                    if (!isStatementWord(parameterType, lang) && !isReservedHere(parameterType))
                        recordVarType(name, parameterType);
                }
            }

            if (isReservedHere(name, verbatim) || preserveSet.count(name)) continue;
            if (!methodName.empty()) {
                methodParams[methodName].insert(name);
                if (!ownerType.empty()) {
                    methodParamsByType[ownerType][methodName].insert(name);
                    methodOwnerTypes[methodName].insert(ownerType);
                }
            }
            if (!methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                localFields.insert(name);
                registerRename(fieldMap, name);
            }
        }
    }

    // VB parameter shape is "[ByVal|ByRef|Optional|ParamArray] name [As Type] [= default]",
// the reverse of C-like's "Type name" -- registerParamNames' words.back() heuristic
// would grab the TYPE, not the name, for a typed VB parameter, so VB needs its own
// extraction rather than reusing that function. Still records into the same
// fieldMap/localFields (so existing rename logic elsewhere keeps working) and the
// same methodParams map (so the named-argument check below can resolve against it).
    void registerParamNamesVB(const wstring& argsStr, const wstring& methodName) {
        if (argsStr.empty()) return;
        for (wstring chunk : splitParams(argsStr)) {
            chunk = trimW(chunk);
            if (chunk.empty()) continue;
            size_t asPos = wstring::npos;
            {
                wregex asRe(LR"(\bAs\b)", regex::icase);
                wsmatch wm;
                if (regex_search(chunk, wm, asRe)) asPos = (size_t)wm.position(0);
            }
            wstring beforeAs = asPos == wstring::npos ? chunk : chunk.substr(0, asPos);
            size_t eq = beforeAs.find(L'=');
            if (eq != wstring::npos) beforeAs = beforeAs.substr(0, eq);
            vector<wstring> words;
            size_t i = 0;
            while (i < beforeAs.size()) {
                if (iswalpha(beforeAs[i]) || beforeAs[i] == L'_') {
                    size_t s = i;
                    while (i < beforeAs.size() && (iswalnum(beforeAs[i]) || beforeAs[i] == L'_')) i++;
                    words.push_back(beforeAs.substr(s, i - s));
                }
                else i++;
            }
            if (words.empty()) continue;
            static const set<wstring> vbParamModifiers = { L"ByVal", L"ByRef", L"Optional", L"ParamArray" };
            wstring name;
            for (const auto& w : words) {
                bool isMod = false;
                for (const auto& pm : vbParamModifiers) if (iequals(pm, w)) { isMod = true; break; }
                if (!isMod) { name = w; break; }
            }
            if (name.empty() || isReservedHere(name) || preserveSet.count(name)) continue;
            if (!methodName.empty()) methodParams[methodName].insert(name);
            if (!methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                localFields.insert(name);
                registerRename(fieldMap, name);
            }
        }
    }

    // FIX (gap 5), part 1: locate every "[CallerMemberName] string name"
    // parameter in this file and record, by the declaration's own bare
    // (pre-rename) name, the ordinal (0-based) position that parameter sits
    // at -- the information protectCallerMemberNameCallers() below needs to
    // tell whether a given call site supplied it or left it to the compiler.
    void prescanCallerMemberName(const string& srcUtf8) {
        try {
            // Accept short, fully-qualified, and Attribute-suffixed spellings,
            // plus System.String aliases. This remains a declaration prescan: it
            // does not rename any attribute text.
            static const regex attrRe(R"(\[\s*(?:(?:global::)?[A-Za-z_]\w*(?:(?:::|\.)[A-Za-z_]\w*)*\.)?CallerMemberName(?:Attribute)?\s*\]\s*(?:(?:global::)?System\.)?String\s+\w+|\[\s*(?:(?:global::)?[A-Za-z_]\w*(?:(?:::|\.)[A-Za-z_]\w*)*\.)?CallerMemberName(?:Attribute)?\s*\]\s*string\s+\w+)");
            for (sregex_iterator it(srcUtf8.begin(), srcUtf8.end(), attrRe), end; it != end; ++it) {
                size_t attrPos = (size_t)it->position(0);
                // Balanced scan backward to the "(" that opens the parameter
                // list this attribute sits inside (an earlier parameter's own
                // "[...]"/"(...)"/"<...>" default-value shapes are skipped by
                // the same depth count).
                int depth = 0;
                size_t openParenPos = string::npos;
                for (size_t i = attrPos; i-- > 0; ) {
                    char c = srcUtf8[i];
                    if (c == ')' || c == ']' || c == '}') depth++;
                    else if (c == '(') { if (depth == 0) { openParenPos = i; break; } depth--; }
                    else if (c == '[' || c == '{') { if (depth > 0) depth--; }
                }
                if (openParenPos == string::npos) continue;
                int ordinal = 0;
                {
                    int d2 = 0;
                    for (size_t i = openParenPos + 1; i < attrPos; i++) {
                        char c = srcUtf8[i];
                        if (c == '(' || c == '[' || c == '{' || c == '<') d2++;
                        else if (c == ')' || c == ']' || c == '}' || c == '>') { if (d2 > 0) d2--; }
                        else if (c == ',' && d2 == 0) ordinal++;
                    }
                }
                size_t e = openParenPos;
                while (e > 0 && isspace((unsigned char)srcUtf8[e - 1])) e--;
                size_t s = e;
                while (s > 0 && (isalnum((unsigned char)srcUtf8[s - 1]) || srcUtf8[s - 1] == '_')) s--;
                if (s == e) continue;
                wstring declName = stringToWstring(srcUtf8.substr(s, e - s));
                if (!declName.empty()) callerMemberNameParamIndex[declName] = ordinal;
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: [CallerMemberName] attribute prescan failed", CLR_WARN); }
    }

    // FIX (gap 5), part 2: for every call to a method/indexer known (from
    // part 1) to have a [CallerMemberName] parameter at a given ordinal
    // position, where the call does not supply enough positional arguments
    // to reach that position, protect the CALL SITE's own enclosing member
    // from renaming -- not the declaration being called -- since that is
    // the name the compiler will inject there. Conservative by design: a
    // call that actually did supply the argument further along by name is
    // simply protected unnecessarily (never incorrectly) when the plain
    // positional count alone can't prove it was supplied.
    void protectCallerMemberNameCallers(const string& srcUtf8) {
        for (const auto& entry : callerMemberNameParamIndex) {
            const wstring& declNameW = entry.first;
            int ordinal = entry.second;
            string declName = wstringToString(declNameW);
            try {
                regex callRe("\\b" + declName + "\\s*\\(");
                for (sregex_iterator it(srcUtf8.begin(), srcUtf8.end(), callRe), end; it != end; ++it) {
                    size_t callPos = (size_t)it->position(0);
                    size_t openParenPos = callPos + it->length(0) - 1;
                    size_t closeParenPos = findMatchingParen(srcUtf8, openParenPos);
                    if (closeParenPos == string::npos) continue;
                    // Skip the declaration itself (its own header, e.g.
                    // "void Log([CallerMemberName] string m = null)" would
                    // otherwise match callRe too and get misread as a call):
                    // a real call is always followed by ";", ",", ")", or
                    // another operator -- never directly by a body "{" the
                    // way a method/indexer's own parameter list is.
                    {
                        size_t j = closeParenPos + 1;
                        while (j < srcUtf8.size() && isspace((unsigned char)srcUtf8[j])) j++;
                        if (j < srcUtf8.size() && srcUtf8[j] == '{') continue;
                    }
                    string argsStr = srcUtf8.substr(openParenPos + 1, closeParenPos - openParenPos - 1);
                    size_t ts = 0, te = argsStr.size();
                    while (ts < te && isspace((unsigned char)argsStr[ts])) ts++;
                    while (te > ts && isspace((unsigned char)argsStr[te - 1])) te--;
                    int partCount = (ts >= te) ? 0 : 1;
                    int d3 = 0;
                    for (size_t i = 0; i < argsStr.size(); i++) {
                        char c = argsStr[i];
                        if (c == '(' || c == '[' || c == '{' || c == '<') d3++;
                        else if (c == ')' || c == ']' || c == '}' || c == '>') { if (d3 > 0) d3--; }
                        else if (c == ',' && d3 == 0) partCount++;
                    }
                    if (partCount > ordinal) continue;   // positionally supplied -- not omitted
                    wstring enclosing = findEnclosingMemberName(srcUtf8, callPos);
                    if (enclosing.empty()) {
                        LogMessage(L"WARN: [CallerMemberName] could not resolve the enclosing caller at byte position " +
                            to_wstring(callPos) + L"; the compiler-injected caller-name string cannot be proven safe, so review this call manually.", CLR_WARN);
                        continue;
                    }
                    if (preserveSet.count(enclosing)) continue;
                    preserveSet.insert(enclosing);
                    localMethods.erase(enclosing);
                    methodMap.erase(enclosing);
                    LogMessage(L"[CallerMemberName]: preserving \"" + enclosing +
                        L"\" (omits the argument to \"" + declNameW +
                        L"\"; the compiler injects this member's own name there).", CLR_WARN);
                }
            }
            catch (const regex_error&) { LogMessage(L"WARN: [CallerMemberName] call-site prescan failed", CLR_WARN); }
        }
    }

    PreScanResult preScanCLike() {
        PreScanResult result;
        string srcUtf8 = wstringToString(source);
        smatch m;
        try {
            if (lang == Lang::CSharp) {
                regex classRe(R"(\b(?:public\s+|private\s+|protected\s+|internal\s+|static\s+|sealed\s+|abstract\s+|partial\s+)*\b(?:class|struct|interface|enum)\s+(\w+))");
                string::const_iterator s1(srcUtf8.cbegin());
                while (regex_search(s1, srcUtf8.cend(), m, classRe)) {
                    wstring name = stringToWstring(m[1]);
                    if (!isReservedHere(name)) result.classNames.push_back(name);
                    s1 = m.suffix().first;
                }
                try {
                    regex pubClassRe(R"(\bpublic\s+(?:static\s+|sealed\s+|abstract\s+|partial\s+)*(?:class|struct|interface|enum)\s+(\w+))");
                    string::const_iterator s2(srcUtf8.cbegin());
                    while (regex_search(s2, srcUtf8.cend(), m, pubClassRe)) {
                        wstring name = stringToWstring(m[1]);
                        if (!isReservedHere(name)) result.publicNames.push_back(name);
                        s2 = m.suffix().first;
                    }
                }
                catch (const regex_error&) { LogMessage(wstring(L"WARN: C# prescan public regex failed"), CLR_WARN); }
            }
            else {
                regex classRe(R"(\b(?:class|struct)\s+(\w+))");
                string::const_iterator s1(srcUtf8.cbegin());
                while (regex_search(s1, srcUtf8.cend(), m, classRe)) {
                    wstring name = stringToWstring(m[1]);
                    if (!isReservedHere(name)) result.classNames.push_back(name);
                    s1 = m.suffix().first;
                }
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: prescan class regex failed"), CLR_WARN); }
        return result;
    }

    void applyPublicExclusionCSharp() {
        string srcUtf8 = wstringToString(source);
        smatch m;
        try {
            regex r1(R"(\bpublic\s+(?:static\s+|sealed\s+|abstract\s+|partial\s+)*(?:class|struct|interface|enum)\s+(\w+))");
            string::const_iterator s1(srcUtf8.cbegin());
            while (regex_search(s1, srcUtf8.cend(), m, r1)) {
                wstring name = stringToWstring(m[1]);
                if (!isReservedHere(name)) preserveSet.insert(name);
                s1 = m.suffix().first;
            }

            regex r2(R"(\bpublic\s+(?:static\s+|virtual\s+|override\s+|abstract\s+|sealed\s+|async\s+|extern\s+|unsafe\s+|new\s+|partial\s+)*[A-Za-z_][\w\.<>\[\],\?\s]{0,256}?\s+(\w+)\s*\()");
            s1 = srcUtf8.cbegin();
            while (regex_search(s1, srcUtf8.cend(), m, r2)) {
                wstring name = stringToWstring(m[1]);
                if (!isReservedHere(name)) preserveSet.insert(name);
                s1 = m.suffix().first;
            }

            regex r3(R"(\bpublic\s+(?:static\s+|readonly\s+|const\s+|volatile\s+|new\s+)*[A-Za-z_][\w\.<>\[\],\?]{0,256}\s+(\w+)\s*(?:=[^;]*;|\{))");
            s1 = srcUtf8.cbegin();
            while (regex_search(s1, srcUtf8.cend(), m, r3)) {
                wstring name = stringToWstring(m[1]);
                if (!isReservedHere(name)) preserveSet.insert(name);
                s1 = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: C# public-exclude regex failed"), CLR_WARN); }
    }

    void preserveDefineNames() {
        if (lang == Lang::VB) return;
        try {
            wregex defineRe(LR"(^\s*#\s*define\s+([A-Za-z_]\w*))");
            for (const auto& srcLine : lines) {
                wsmatch dm;
                if (regex_search(srcLine, dm, defineRe)) {
                    if (dm.size() >= 2 && dm[1].matched && !dm[1].str().empty())
                        preserveSet.insert(dm[1].str());
                }
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: #define preserve regex failed"), CLR_WARN); }
    }

    void prescanMacros() {
        if (!preserveMacros) return;
        bool inDefine = false;
        try {
            wregex wordRe(LR"([A-Za-z_]\w*)");
            for (const auto& srcLine : lines) {
                wstring trimmed = trimW(srcLine);
                if (!inDefine && trimmed.size() >= 2 && trimmed[0] == L'#' &&
                    iequals(firstWordOf(trimmed.substr(1)), L"define")) {
                    inDefine = true;
                }
                if (inDefine) {
                    for (auto it = wsregex_iterator(srcLine.begin(), srcLine.end(), wordRe);
                        it != wsregex_iterator(); ++it) {
                        wstring word = (*it)[0];
                        if (!word.empty()) {
                            preservedMacroParams.insert(word);
                            preserveSet.insert(word);
                        }
                    }
                    wstring rtrim = srcLine;
                    while (!rtrim.empty() && iswspace(rtrim.back())) rtrim.pop_back();
                    if (rtrim.empty() || rtrim.back() != L'\\') inDefine = false;
                }
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: #define prescan regex failed", CLR_WARN); }
    }

    void prescanLabels(const string& src) {
        if (!preserveLabels) return;
        try {
            regex gotoRe(R"(\bgoto\s+([A-Za-z_]\w*)\s*;)");
            for (sregex_iterator it(src.begin(), src.end(), gotoRe), end; it != end; ++it) {
                wstring name = stringToWstring((*it)[1]);
                preservedLabels.insert(name);
                preserveSet.insert(name);
            }
            regex vbGotoRe(R"(\b(?:GoTo|Resume)\s+([A-Za-z_]\w*)\b)", regex::icase);
            for (sregex_iterator it(src.begin(), src.end(), vbGotoRe), end; it != end; ++it) {
                wstring name = stringToWstring((*it)[1]);
                if (isReservedHere(name)) continue;
                preservedLabels.insert(name);
                preserveSet.insert(name);
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: goto prescan regex failed", CLR_WARN); }
    }

    void prescanOperators(const string& src) {
        if (!preserveOperators) return;
        try {
            regex opRe(R"(\boperator\s*([A-Za-z_]\w*(?:\s*<[^<>]*>)?|[<>=!+\-*/%^&|~]+))");
            for (sregex_iterator it(src.begin(), src.end(), opRe), end; it != end; ++it) {
                wstring name = stringToWstring((*it)[1]);
                if (name.empty() || !iswalpha(name[0])) continue;
                if (iequals(name, L"new") || iequals(name, L"delete")) continue;
                preservedOperators.insert(name);
                preserveSet.insert(name);
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: operator prescan regex failed", CLR_WARN); }
    }

    void prescanNamespaces(const string& src) {
        if (!preserveNamespaces) return;
        try {
            regex nsRe(R"(\bnamespace\s+([A-Za-z_]\w*))");
            for (sregex_iterator it(src.begin(), src.end(), nsRe), end; it != end; ++it) {
                wstring name = stringToWstring((*it)[1]);
                if (isReservedHere(name)) continue;
                preservedNamespaces.insert(name);
                preserveSet.insert(name);
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: namespace prescan regex failed", CLR_WARN); }
    }

    // C# equivalent of prescanNamespaces/prescanUsingTypedef above, which only
    // ran for Lang::Cpp. A plain "using Guna.UI2.WinForms;" (importing a
    // third-party control library's namespace) had NO protection at all: if
    // any segment of that dotted path happened to match a name the renamer
    // otherwise wanted to touch, it got renamed, and since the namespace is
    // defined outside this project entirely there's no declaration here for
    // that rename to also apply to -- hence CS0234 ("type or namespace name
    // '<renamed>' does not exist"). Fix: collect every namespace this source
    // itself DECLARES (so renaming our own namespaces, which a later pass may
    // still want to do, is unaffected), then for every "using A.B.C;" import
    // whose root segment "A" isn't one of those declared-locally roots,
    // protect every segment of "A.B.C" from being renamed anywhere in the file.
    void prescanCSharpExternalUsings(const string& src) {
        set<wstring> declaredRoots;
        try {
            regex nsDeclRe(R"(\bnamespace\s+([A-Za-z_]\w*(?:\s*\.\s*[A-Za-z_]\w*)*))");
            for (sregex_iterator it(src.begin(), src.end(), nsDeclRe), end; it != end; ++it) {
                wstring full = stringToWstring((*it)[1]);
                size_t st = 0;
                for (size_t q = 0; q <= full.size(); q++) {
                    if (q != full.size() && !(full[q] == L'.' || iswspace(full[q]))) continue;
                    wstring seg = full.substr(st, q - st);
                    st = q + 1;
                    if (!seg.empty() && !iswspace(seg[0])) { declaredRoots.insert(seg); break; }
                }
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: C# namespace-declaration prescan failed", CLR_WARN); }

        try {
            // Deliberately excludes "using X = Y;" (alias), "using static X;",
            // and "using (...)" (the using-STATEMENT, which starts a block and
            // is never followed directly by ";") -- those aren't plain
            // namespace imports and are handled (or deliberately ignored)
            // elsewhere.
            regex usingNsRe(R"(\busing\s+(?!static\b)([A-Za-z_]\w*(?:\s*\.\s*[A-Za-z_]\w*)*)\s*;)");
            for (sregex_iterator it(src.begin(), src.end(), usingNsRe), end; it != end; ++it) {
                wstring full = stringToWstring((*it)[1]);
                size_t st = 0;
                vector<wstring> segs;
                for (size_t q = 0; q <= full.size(); q++) {
                    if (q != full.size() && !(full[q] == L'.' || iswspace(full[q]))) continue;
                    wstring seg = full.substr(st, q - st);
                    st = q + 1;
                    if (!seg.empty() && !iswspace(seg[0])) segs.push_back(seg);
                }
                if (segs.empty()) continue;
                if (declaredRoots.count(segs[0])) continue; // one of our own namespaces -- leave to the normal rename logic
                for (auto& seg : segs) { preservedNamespaces.insert(seg); preserveSet.insert(seg); }
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: C# using-directive prescan failed", CLR_WARN); }
    }

    void prescanUsingTypedef(const string& src) {
        if (!preserveTypedefs) return;
        try {
            regex usingRe(R"(\busing\s+([A-Za-z_]\w*)\s*=)");
            for (sregex_iterator it(src.begin(), src.end(), usingRe), end; it != end; ++it) {
                wstring name = stringToWstring((*it)[1]);
                preservedTypedefs.insert(name);
                preserveSet.insert(name);
            }
            regex typedefRe(R"(\btypedef\s+[\w:<>,\s\*&]{1,1024}?\b([A-Za-z_]\w*)\s*;)");
            for (sregex_iterator it(src.begin(), src.end(), typedefRe), end; it != end; ++it) {
                wstring name = stringToWstring((*it)[1]);
                preservedTypedefs.insert(name);
                preserveSet.insert(name);
            }
            regex usingMemberRe(R"(\busing\s+(?:typename\s+|enum\s+)?([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*;)");
            for (sregex_iterator it(src.begin(), src.end(), usingMemberRe), end; it != end; ++it) {
                wstring full = stringToWstring((*it)[1]);
                size_t pp = full.rfind(L"::");
                wstring name = (pp == wstring::npos) ? full : full.substr(pp + 2);
                preservedTypedefs.insert(name);
                preserveSet.insert(name);
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: typedef/using prescan regex failed", CLR_WARN); }
    }

    void prescanTemplateParams(const string& src) {
        if (!preserveTemplates) return;
        try {
            regex tmplRe(R"(\btemplate\s*<((?:[^<>]|<(?:[^<>]|<[^<>]*>)*>)*)>)");
            for (sregex_iterator it(src.begin(), src.end(), tmplRe), end; it != end; ++it) {
                wstring params = stringToWstring((*it)[1]);
                vector<wstring> parts;
                int depth = 0;
                wstring cur;
                for (wchar_t c : params) {
                    if (c == L'<') { depth++; cur += c; }
                    else if (c == L'>') { depth--; cur += c; }
                    else if (c == L',' && depth == 0) { parts.push_back(cur); cur.clear(); }
                    else cur += c;
                }
                if (!cur.empty()) parts.push_back(cur);
                for (auto& p : parts) {
                    wstring t = p;
                    size_t eq = t.find(L'=');
                    if (eq != wstring::npos) t = t.substr(0, eq);
                    vector<wstring> words;
                    size_t ii = 0;
                    while (ii < t.size()) {
                        if (iswalpha(t[ii]) || t[ii] == L'_') {
                            size_t s = ii;
                            while (ii < t.size() && (iswalnum(t[ii]) || t[ii] == L'_')) ii++;
                            words.push_back(t.substr(s, ii - s));
                        }
                        else ii++;
                    }
                    for (int w = (int)words.size() - 1; w >= 0; w--) {
                        const wstring& cand = words[w];
                        if (iequals(cand, L"typename") || iequals(cand, L"class") ||
                            iequals(cand, L"auto") || isReservedHere(cand) || cand.size() < 1)
                            continue;
                        preservedTemplateParams.insert(cand);
                        preserveSet.insert(cand);
                        break;
                    }
                }
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: template prescan regex failed", CLR_WARN); }
    }

    void prescanStructuredBindings(const string& src) {
        try {
            regex sbRe(R"(\bauto\s*(?:&&?|const)?\s*\[([^\]]+)\]\s*=)");
            for (sregex_iterator it(src.begin(), src.end(), sbRe), end; it != end; ++it) {
                wstring names = stringToWstring((*it)[1]);
                size_t start = 0;
                for (size_t i = 0; i <= names.size(); i++) {
                    if (i != names.size() && names[i] != L',') continue;
                    wstring n = trimW(names.substr(start, i - start));
                    start = i + 1;
                    while (!n.empty() && (n[0] == L'&' || n[0] == L'*' || iswspace(n[0]))) n.erase(0, 1);
                    if (n.empty() || isReservedHere(n)) continue;
                    if (methodMap.count(n) || classMap.count(n) || fieldMap.count(n)) continue;
                    localFields.insert(n);
                    registerRename(fieldMap, n);
                }
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: structured-binding prescan failed", CLR_WARN); }
    }

    void registerCaptureName(wstring name) {
        name = trimW(name);
        if (name.empty() || name == L"this" || name == L"*this") return;
        if (isReservedHere(name) || preserveSet.count(name)) return;
        if (!methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
            localFields.insert(name);
            registerRename(fieldMap, name);
        }
    }

    void prescanLambdas(const string& src) {
        try {
            regex lamRe(R"(\]\s*\(([^()]*)\)\s*(?:mutable\s+)?(?:->\s*[^{]+)?\{)");
            for (sregex_iterator it(src.begin(), src.end(), lamRe), end; it != end; ++it) {
                wstring argsStr = stringToWstring((*it)[1]);
                registerParamNames(argsStr);
            }
            regex lamCapRe(R"(\[([^\]]*)\]\s*\(([^()]*)\)\s*(?:mutable\s+)?(?:->\s*[^{]+)?\{)");
            for (sregex_iterator it(src.begin(), src.end(), lamCapRe), end; it != end; ++it) {
                wstring capStr = stringToWstring((*it)[1]);
                size_t start = 0;
                for (size_t i = 0; i <= capStr.size(); i++) {
                    if (i != capStr.size() && capStr[i] != L',') continue;
                    wstring part = trimW(capStr.substr(start, i - start));
                    start = i + 1;
                    if (part.empty() || part == L"this" || part == L"*this" || part == L"=" || part == L"&") continue;
                    size_t eq = part.find(L'=');
                    if (eq != wstring::npos) {
                        wstring lhs = trimW(part.substr(0, eq));
                        if (!lhs.empty() && lhs[0] == L'&') lhs = trimW(lhs.substr(1));
                        registerCaptureName(lhs);
                    }
                    else if (!part.empty() && part[0] == L'&') {
                        registerCaptureName(part.substr(1));
                    }
                    else {
                        registerCaptureName(part);
                    }
                }
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: lambda prescan regex failed", CLR_WARN); }
    }

    void prescanConcepts(const string& src) {
        try {
            regex ccRe(R"(\bconcept\s+([A-Za-z_]\w*)\s*=)");
            for (sregex_iterator it(src.begin(), src.end(), ccRe), end; it != end; ++it) {
                wstring name = stringToWstring((*it)[1]);
                preservedConcepts.insert(name);
                preserveSet.insert(name);
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: concept prescan regex failed", CLR_WARN); }
    }

    void prescanAttributes(const string& src) {
        try {
            regex atRe(R"(\[\[\s*([A-Za-z_]\w*))");
            for (sregex_iterator it(src.begin(), src.end(), atRe), end; it != end; ++it) {
                wstring name = stringToWstring((*it)[1]);
                preservedAttributes.insert(name);
                preserveSet.insert(name);
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: attribute prescan regex failed", CLR_WARN); }
    }

    // Bug-report Category 4: "a declaration's renameability can be
    // constrained by factors that are not visible in the identifier's own
    // token... any declaration carrying a name-escapes-the-program marker is
    // a candidate for silent breakage." This is deliberately a blanket rule
    // keyed on "is there an attribute here at all" -- not a list of "known
    // dangerous" attribute names -- because this tool has no way to tell
    // which specific attributes carry a name out of the program
    // (serialization, reflection, interop, data binding, caller-info,
    // configuration, resource lookup, XAML binding) and which don't, so
    // every one is treated as if it might.
    //
    // This only ever ADDS entries to preserveSet, never removes anything.
    // preserveSet is already consulted before every local*Map insertion in
    // this file (see the extractIdentifiers*/prescan* functions below), so
    // a declaration this misses just keeps whatever renamability the rest
    // of the prescan already decided for it -- a parsing miss here can
    // under-protect, never mis-protect some unrelated symbol.
    //
    // Deliberately NOT handled here (still open, needs a real name-usage
    // correlation pass, not just attribute detection): attribute arguments
    // that are themselves string literals matching an identifier name, e.g.
    // [DataMember(Name = "Foo")] where the string "Foo" would need to track
    // a renamed member named Foo elsewhere.
    wstring extractDeclaredNameHeuristic(const wstring& declLine, bool isVB) {
        wsmatch m;
        // Method / constructor / indexer / generic method: identifier
        // immediately followed by an optional "<...>" and then "(".
        static const wregex withParens(LR"(([A-Za-z_]\w*)\s*(?:<[^<>]*>)?\s*\()");
        if (regex_search(declLine, m, withParens)) {
            wstring n = m[1].str();
            if (!n.empty()) return n;
        }
        if (isVB) {
            // VB field/property: "... Name As Type".
            static const wregex vbAsRe(LR"(\b([A-Za-z_]\w*)\s+As\s+)", regex::icase);
            if (regex_search(declLine, m, vbAsRe)) return m[1].str();
            return L"";
        }
        // C# field / auto-property: cut the line at the first "{" (property
        // accessor body) or "=" (initializer), whichever comes first, strip
        // a trailing ";", and take the LAST identifier in what remains --
        // e.g. "public int Foo { get; set; }" -> "public int Foo " -> "Foo";
        // "private string TEXT;" -> "private string TEXT" -> "TEXT". Cutting
        // at "{" before scanning is what keeps this from picking up "get"/
        // "set" out of the accessor body instead of the real member name.
        wstring head = declLine;
        size_t cut = head.find(L'{');
        size_t eq = head.find(L'=');
        if (eq != wstring::npos && (cut == wstring::npos || eq < cut)) cut = eq;
        if (cut != wstring::npos) head = head.substr(0, cut);
        while (!head.empty() && (head.back() == L';' || iswspace(head.back()))) head.pop_back();
        size_t e = head.size();
        while (e > 0 && !(iswalnum(head[e - 1]) || head[e - 1] == L'_')) e--;
        size_t b = e;
        while (b > 0 && (iswalnum(head[b - 1]) || head[b - 1] == L'_')) b--;
        if (e > b) return head.substr(b, e - b);
        return L"";
    }

    void prescanAttributeDecoratedDeclarations() {
        bool isVB = (lang == Lang::VB);
        try {
            // Case 1: one or more consecutive "[Attr(...)]"-only (or VB
            // "<Attr(...)>"-only) lines immediately before a declaration on
            // a later line.
            wregex csAttrLineRe(LR"(^\s*(?:\[\s*[A-Za-z_][^\]]*\]\s*)+$)");
            wregex vbAttrLineRe(LR"(^\s*(?:<\s*[A-Za-z_][^>]*>\s*)+_?\s*$)", regex::icase);
            for (size_t i = 0; i < lines.size(); i++) {
                wstring t = trimW(lines[i]);
                if (t.empty()) continue;
                bool isAttrLine = isVB ? regex_match(t, vbAttrLineRe) : regex_match(t, csAttrLineRe);
                if (!isAttrLine) continue;
                size_t j = i + 1;
                while (j < lines.size()) {
                    wstring tj = trimW(lines[j]);
                    if (tj.empty()) { j++; continue; }
                    bool tjIsAttr = isVB ? regex_match(tj, vbAttrLineRe) : regex_match(tj, csAttrLineRe);
                    if (tjIsAttr) { j++; continue; }
                    break;
                }
                if (j >= lines.size()) continue;
                wstring declName = extractDeclaredNameHeuristic(lines[j], isVB);
                if (!declName.empty() && !isReservedHere(declName)) preserveSet.insert(declName);
            }

            // Case 2: attribute(s) and the declaration on the SAME physical
            // line, e.g. "[Foo] public int X;" / "<Foo()> Public Property X As Integer".
            wregex csInlineRe(LR"(^\s*(?:\[\s*[A-Za-z_][^\]]*\]\s*)+(\S.*)$)");
            wregex vbInlineRe(LR"(^\s*(?:<\s*[A-Za-z_][^>]*>\s*)+(\S.*)$)", regex::icase);
            for (auto& ln : lines) {
                wsmatch m;
                bool matched = isVB ? regex_match(ln, m, vbInlineRe) : regex_match(ln, m, csInlineRe);
                if (!matched) continue;
                wstring rest = m[1].str();
                if (trimW(rest).empty()) continue; // pure attribute line -- handled by Case 1 above
                wstring declName = extractDeclaredNameHeuristic(rest, isVB);
                if (!declName.empty() && !isReservedHere(declName)) preserveSet.insert(declName);
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: attribute-decoration prescan failed", CLR_WARN); }
    }

    void prescanDirectives() {
        try {
            wregex renameNsRe(LR"XN(rename_namespace\s*\(\s*"([^"]+)"\s*\))XN");
            wregex renameRe(LR"XN(rename\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\))XN");
            wregex pragmaCommentRe(LR"(^\s*#\s*pragma\s+comment\s*\(\s*linker\s*,\s*"[^"]*?:([A-Za-z_]\w*))", regex::icase);
            wregex packRe(LR"(^\s*#\s*pragma\s+pack\s*\(\s*push\s*,\s*([A-Za-z_]\w*))", regex::icase);
            wregex warningPragmaRe(LR"(^\s*#\s*pragma\s+warning\s*\(\s*(?:disable|error|default|push|pop)\s*:\s*([A-Za-z_]\w*)\s*\))", regex::icase);
            wregex ompClauseRe(LR"(^\s*#\s*pragma\s+omp\b.*\b(?:private|firstprivate|lastprivate|shared|reduction|copyin|copyprivate|linear)\s*\(([^)]*)\))", regex::icase);
            wregex importExcludeRe(LR"XN(\b(?:exclude|include)\s*\(\s*"([^"]+)"\s*(?:,\s*"([^"]+)"\s*)*\))XN");
            for (const auto& ln : lines) {
                wstring t = trimW(ln);
                if (t.empty() || t[0] != L'#') continue;
                wsmatch m;
                if (regex_search(ln, m, renameNsRe) && m[1].matched) preserveSet.insert(m[1].str());
                if (regex_search(ln, m, renameRe) && m.size() >= 3) {
                    preserveSet.insert(m[1].str());
                    preserveSet.insert(m[2].str());
                }
                if (regex_search(ln, m, pragmaCommentRe) && m[1].matched) preserveSet.insert(m[1].str());
                if (regex_search(ln, m, packRe) && m[1].matched) preserveSet.insert(m[1].str());
                if (regex_search(ln, m, warningPragmaRe) && m[1].matched) preserveSet.insert(m[1].str());
                if (regex_search(ln, m, importExcludeRe)) {
                    if (m[1].matched) preserveSet.insert(m[1].str());
                    if (m.size() >= 3 && m[2].matched) preserveSet.insert(m[2].str());
                }
                if (regex_search(ln, m, ompClauseRe) && m[1].matched) {
                    wstring listW = m[1].str();
                    size_t start = 0;
                    for (size_t i = 0; i <= listW.size(); i++) {
                        if (i != listW.size() && listW[i] != L',') continue;
                        wstring piece = trimW(listW.substr(start, i - start));
                        start = i + 1;
                        size_t colon = piece.rfind(L':');
                        wstring nameOnly = (colon == wstring::npos) ? piece : trimW(piece.substr(colon + 1));
                        while (!nameOnly.empty() && !(iswalnum(nameOnly.front()) || nameOnly.front() == L'_')) nameOnly.erase(0, 1);
                        while (!nameOnly.empty() && !(iswalnum(nameOnly.back()) || nameOnly.back() == L'_')) nameOnly.pop_back();
                        if (!nameOnly.empty()) preserveSet.insert(nameOnly);
                    }
                }
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: directive prescan failed", CLR_WARN); }
    }

    void prescanMultiDeclarators(const string& src) {
        try {
            regex declRe(R"(\b(?:int|long|short|char|bool|float|double|unsigned|signed|auto|size_t|wchar_t|char8_t|char16_t|char32_t)\s+((?:[A-Za-z_]\w*\s*,\s*)+[A-Za-z_]\w*)\s*[;=])");
            for (sregex_iterator it(src.begin(), src.end(), declRe), end; it != end; ++it) {
                wstring list = stringToWstring((*it)[1]);
                size_t start = 0;
                for (size_t i = 0; i <= list.size(); i++) {
                    if (i != list.size() && list[i] != L',') continue;
                    wstring n = trimW(list.substr(start, i - start));
                    start = i + 1;
                    while (!n.empty() && (n[0] == L'&' || n[0] == L'*' || iswspace(n[0]))) n.erase(0, 1);
                    size_t br = n.find(L'[');
                    if (br != wstring::npos) n = n.substr(0, br);
                    if (n.empty() || isReservedHere(n)) continue;
                    if (methodMap.count(n) || classMap.count(n) || fieldMap.count(n)) continue;
                    localFields.insert(n);
                    registerRename(fieldMap, n);
                }
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: multi-declarator prescan failed", CLR_WARN); }
    }

    // TODO: real DLL-import detection (extern "C" __declspec(dllimport), etc.)
    void prescanDllImports(const string& src) { (void)src; }
    // TODO: real anonymous struct/union detection
    void prescanAnonymousStructs(const string& src) { (void)src; }

    void recordVarType(const wstring& name, const wstring& type) {
        if (name.empty() || type.empty() || type == L"var") return;
        const auto key = make_pair(activeSourceLabel, name);
        if (!ambiguousSourceVarTypes.count(key)) {
            auto it = sourceVarToType.find(key);
            if (it == sourceVarToType.end()) {
                sourceVarToType.emplace(key, type);
            }
            else if (it->second != type) {
                sourceVarToType.erase(it);
                ambiguousSourceVarTypes.insert(key);
            }
        }

        // Cross-file fallback: allow a uniquely typed project receiver to resolve
        // when its declaration and member use are in different source units.
        // Conflicting same-named declarations poison this fallback, preserving the
        // existing fail-closed behavior for ambiguous identifiers.
        if (ambiguousProjectVarTypes.count(name)) return;
        auto projectIt = projectVarToType.find(name);
        if (projectIt == projectVarToType.end()) {
            projectVarToType.emplace(name, type);
        }
        else if (projectIt->second != type) {
            projectVarToType.erase(projectIt);
            ambiguousProjectVarTypes.insert(name);
        }
    }

    const wstring* findVarType(const wstring& name) const {
        const auto key = make_pair(activeSourceLabel, name);
        auto it = sourceVarToType.find(key);
        if (it != sourceVarToType.end()) return &it->second;
        if (ambiguousSourceVarTypes.count(key) || ambiguousProjectVarTypes.count(name)) return nullptr;
        auto projectIt = projectVarToType.find(name);
        return projectIt == projectVarToType.end() ? nullptr : &projectIt->second;
    }

    void prescanAutoReturn(const string& src) {
        try {
            regex arRe(R"(\bauto\s+([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\s*\()");
            for (sregex_iterator it(src.begin(), src.end(), arRe), end; it != end; ++it) {
                wstring name = stringToWstring((*it)[1]);
                wstring func = stringToWstring((*it)[2]);
                auto fit = funcReturnTypes.find(func);
                if (fit == funcReturnTypes.end() || fit->second.empty()) continue;
                recordVarType(name, fit->second);
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: auto-return prescan failed", CLR_WARN); }
    }

    // Compiler-generated VB-closure classes are emitted into C# source with
    // ordinary, source-visible identifiers (for example _Closure_0024__39_002D1).
    // Their copy constructors commonly accept another instance of the same
    // generated type. The general method-header heuristic can miss these
    // constructor-shaped declarations, leaving the parameter receiver type
    // unknown and causing `arg0._0024VB_...` references to be skipped even
    // though the corresponding field declaration was renamed. Register these
    // receiver types before line rewriting; this does not create or rename any
    // symbols itself.
    void prescanCSharpClosureConstructorParameters(const string& src) {
        if (lang != Lang::CSharp) return;
        try {
            static const regex closureCtorRe(
                R"(\b(_Closure_[A-Za-z0-9_]+)\s*\(\s*(_Closure_[A-Za-z0-9_]+)\s+([A-Za-z_]\w*)\s*\))");
            for (sregex_iterator it(src.begin(), src.end(), closureCtorRe), end; it != end; ++it) {
                wstring ownerType = stringToWstring((*it)[1]);
                wstring parameterType = stringToWstring((*it)[2]);
                wstring parameterName = stringToWstring((*it)[3]);
                // Generated VB closure copy constructors have an exact, explicit
                // self-type parameter. Do not make this depend on localTypeNames:
                // the ordinary declaration filter can omit compiler-generated
                // closure names even though the constructor is present.
                if (ownerType == parameterType && ownerType.rfind(L"_Closure_", 0) == 0) {
                    localTypeNames.insert(ownerType);
                    recordVarType(parameterName, parameterType);
                }
            }
        }
        catch (const regex_error&) {
            LogMessage(L"WARN: C# closure-constructor receiver prescan failed; generated closure member references may be preserved.", CLR_WARN);
        }
    }

    // Resolve the smallest enclosing C# class/struct/interface/record declaration.
    // Uses lexical brace matching so overloads with the same name in different
    // classes can maintain separate parameter sets.
    wstring findEnclosingTypeName(const string& src, size_t pos) {
        try {
            regex typeRe(R"(\b(?:class|struct|interface|record)\s+([A-Za-z_]\w*)[^;{}]*\{)");
            wstring best;
            size_t bestSpan = string::npos;
            for (sregex_iterator it(src.begin(), src.end(), typeRe), end; it != end; ++it) {
                size_t open = (size_t)(it->position(0) + it->length(0) - 1);
                if (open >= pos) continue;
                int depth = 0;
                size_t close = string::npos;
                for (size_t j = open; j < src.size(); ++j) {
                    if (src[j] == '{') ++depth;
                    else if (src[j] == '}' && --depth == 0) { close = j; break; }
                }
                if (close != string::npos && pos > open && pos < close && close - open < bestSpan) {
                    bestSpan = close - open;
                    best = stringToWstring((*it)[1]);
                }
            }
            return best;
        }
        catch (const regex_error&) {
            LogMessage(L"WARN: enclosing-type resolution regex failed; method parameters will use conservative name-only resolution.", CLR_WARN);
            return L"";
        }
    }

    // Build class -> declared field/property/event names from top-level class
    // members only. Method locals are skipped by consuming nested brace blocks.
    void prescanClassMembers(const string& src) {
        if (lang == Lang::CSharp) {
            try {
                regex typeRe(R"(\b(?:class|struct|interface|record)\s+([A-Za-z_]\w*)[^;{}]*\{)");
                for (sregex_iterator it(src.begin(), src.end(), typeRe), end; it != end; ++it) {
                    wstring typeName = stringToWstring((*it)[1]);
                    size_t open = (size_t)(it->position(0) + it->length(0) - 1);
                    int depth = 1;
                    size_t i = open + 1, stmt = i;
                    while (i < src.size() && depth > 0) {
                        char c = src[i];
                        if (c == '{') {
                            if (depth == 1) {
                                string header = src.substr(stmt, i - stmt);
                                // Property/event declarations terminate at their own
                                // accessor brace. Ignore method bodies and nested types.
                                if (header.find('(') == string::npos) {
                                    size_t eq = header.find('=');
                                    if (eq != string::npos) header = header.substr(0, eq);
                                    smatch mm;
                                    regex memberRe(R"((?:[A-Za-z_]\w*\s+)*(?:[A-Za-z_]\w*(?:(?:\.|::)[A-Za-z_]\w*)*(?:\s*<[^<>]*>)?[\*&\?\[\]]*\s+)([A-Za-z_]\w*)\s*$)");
                                    if (regex_search(header, mm, memberRe)) {
                                        wstring member = stringToWstring(mm[1]);
                                        if (localFields.count(member) && fieldMap.count(member)) classMembersByType[typeName].insert(member);
                                    }
                                }
                            }
                            ++depth;
                        }
                        else if (c == '}') {
                            --depth;
                            if (depth == 1) stmt = i + 1;
                        }
                        else if (c == ';' && depth == 1) {
                            string header = src.substr(stmt, i - stmt);
                            size_t eq = header.find('=');
                            if (eq != string::npos) header = header.substr(0, eq);
                            smatch mm;
                            regex memberRe(R"((?:[A-Za-z_]\w*\s+)*(?:[A-Za-z_]\w*(?:(?:\.|::)[A-Za-z_]\w*)*(?:\s*<[^<>]*>)?[\*&\?\[\]]*\s+)([A-Za-z_]\w*)\s*$)");
                            if (header.find('(') == string::npos && regex_search(header, mm, memberRe)) {
                                wstring member = stringToWstring(mm[1]);
                                if (localFields.count(member) && fieldMap.count(member)) classMembersByType[typeName].insert(member);
                            }
                            stmt = i + 1;
                        }
                        ++i;
                    }
                }
            }
            catch (const regex_error&) { LogMessage(L"WARN: class-member target prescan regex failed; type-scoped label rewrites will be skipped.", CLR_WARN); }
        }
        else if (lang == Lang::VB) {
            try {
                regex typeRe(R"(\bClass\s+([A-Za-z_]\w*)([\s\S]*?)\bEnd\s+Class\b)", regex::icase);
                regex memberRe(R"((?:^|[\r\n])[\t ]*(?:(?:Public|Private|Protected|Friend|Shared|ReadOnly|WriteOnly|WithEvents|Dim|Default|Overrides|Overridable|Shadows|Overloads)[\t ]+)*(?:Property|Event|Dim)?[\t ]*([A-Za-z_]\w*)[\t ]+As\b)", regex::icase);
                for (sregex_iterator it(src.begin(), src.end(), typeRe), end; it != end; ++it) {
                    wstring typeName = stringToWstring((*it)[1]);
                    string body = (*it)[2].str();
                    for (sregex_iterator mi(body.begin(), body.end(), memberRe), me; mi != me; ++mi) {
                        wstring member = stringToWstring((*mi)[1]);
                        if (localFields.count(member) && fieldMap.count(member)) classMembersByType[typeName].insert(member);
                    }
                }
            }
            catch (const regex_error&) { LogMessage(L"WARN: VB class-member target prescan regex failed; With-block label rewrites will be skipped.", CLR_WARN); }
        }
    }

    // Final project-wide ownership pass for C# fields/properties/events.
    // This runs after all source units have contributed renameable declarations,
    // so static members in another file and generated closure fields are indexed
    // even when declaration extraction order differs from file order.
    void prescanProjectClassMembers() {
        try {
            static const regex typeRe(R"(\b(?:class|struct|interface|record)\s+([A-Za-z_]\w*)[^;{}]*\{)");
            static const regex memberRe(
                R"(^\s*(?:(?:public|private|protected|internal|static|readonly|const|volatile|required|new|unsafe|extern|fixed|event|virtual|override|abstract|sealed)\s+)*(?:[A-Za-z_]\w*(?:(?:\.|::)[A-Za-z_]\w*)*(?:\s*<[^;{}]*>)?(?:\[\])?[*&?]*\s+)([A-Za-z_]\w*)\s*(?:=.*)?$)");
            for (const auto& unit : sources) {
                if (unit.lang != Lang::CSharp) continue;
                const string src = wstringToString(unit.text);
                for (sregex_iterator ti(src.begin(), src.end(), typeRe), te; ti != te; ++ti) {
                    const wstring typeName = stringToWstring((*ti)[1]);
                    const size_t open = static_cast<size_t>(ti->position(0) + ti->length(0) - 1);
                    int depth = 1;
                    size_t stmt = open + 1;
                    for (size_t i = open + 1; i < src.size() && depth > 0; ++i) {
                        const char c = src[i];
                        if (c == '{') {
                            if (depth == 1) {
                                string header = src.substr(stmt, i - stmt);
                                if (header.find('(') == string::npos) {
                                    smatch mm;
                                    if (regex_match(header, mm, memberRe)) {
                                        const wstring member = stringToWstring(mm[1]);
                                        if (localFields.count(member) && fieldMap.count(member))
                                            classMembersByType[typeName].insert(member);
                                    }
                                }
                            }
                            ++depth;
                        }
                        else if (c == '}') {
                            --depth;
                            if (depth == 1) stmt = i + 1;
                        }
                        else if (c == ';' && depth == 1) {
                            string header = src.substr(stmt, i - stmt);
                            if (header.find('(') == string::npos) {
                                smatch mm;
                                if (regex_match(header, mm, memberRe)) {
                                    const wstring member = stringToWstring(mm[1]);
                                    if (localFields.count(member) && fieldMap.count(member))
                                        classMembersByType[typeName].insert(member);
                                }
                            }
                            stmt = i + 1;
                        }
                    }
                }
            }
        }
        catch (const regex_error&) {
            LogMessage(L"WARN: project-wide class-member index failed; uncertain static/closure member references will be preserved.", CLR_WARN);
        }
    }

    // Register anonymous-type properties under the identity of the variable
    // receiving the anonymous object. This avoids borrowing a same-spelled
    // fieldMap entry from an unrelated class.
    void prescanAnonymousTypeMembers(const string& src) {
        if (lang != Lang::CSharp) return;
        try {
            regex anonRe(R"(\bvar\s+([A-Za-z_]\w*)\s*=\s*new\s*\{([^{}]*)\})");
            regex propRe(R"((?:^|,)\s*([A-Za-z_]\w*)\s*=)");
            for (sregex_iterator it(src.begin(), src.end(), anonRe), end; it != end; ++it) {
                wstring varName = stringToWstring((*it)[1]);
                string body = (*it)[2].str();
                for (sregex_iterator pi(body.begin(), body.end(), propRe), pe; pi != pe; ++pi) {
                    wstring member = stringToWstring((*pi)[1]);
                    if (!member.empty() && !isReservedHere(member) && !preserveSet.count(member) &&
                        !anonymousMemberRenames[varName].count(member)) {
                        anonymousMemberRenames[varName][member] = globalNG.getObfuscatedName(member);
                    }
                }
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: anonymous-type member prescan regex failed; anonymous property labels will be left unchanged.", CLR_WARN); }
    }

    // Emit explicit warnings for generated identifiers this source-level pass
    // cannot rewrite because the compiler creates them after source compilation.
    void warnCompilerGeneratedNames(const string& src) {
        try {
            regex generatedRe(R"(<[A-Za-z_][A-Za-z0-9_]*>k__BackingField|<[^>]+>[0-9]+__|<>c__DisplayClass[0-9_]*|<Main>\$)");
            for (sregex_iterator it(src.begin(), src.end(), generatedRe), end; it != end; ++it) {
                size_t pos = (size_t)it->position(0);
                size_t line = 1, col = 1;
                for (size_t i = 0; i < pos; ++i) { if (src[i] == '\n') { ++line; col = 1; } else ++col; }
                LogMessage(L"WARN: compiler-generated/post-compilation identifier at line " + to_wstring(line) +
                    L", column " + to_wstring(col) + L" (" + stringToWstring(it->str()) +
                    L") is outside source-level rename scope; no safety claim is made for this generated name.", CLR_WARN);
            }
        }
        catch (const regex_error&) { LogMessage(L"WARN: compiler-generated-name detection regex failed; generated names may remain unreported.", CLR_WARN); }
    }

    void extractIdentifiersCLike() {
        preserveDefineNames();
        if (lang == Lang::CSharp && excludePublicDeclarations) applyPublicExclusionCSharp();
        string srcUtf8 = wstringToString(source);
        prescanDllImports(srcUtf8);

        if (lang == Lang::Cpp) {
            prescanQtSignalsSlots(srcUtf8);
            prescanMacros();
            prescanAnonymousStructs(srcUtf8);
            prescanLabels(srcUtf8);
            prescanOperators(srcUtf8);
            prescanNamespaces(srcUtf8);
            prescanUsingTypedef(srcUtf8);
            prescanTemplateParams(srcUtf8);
            prescanStructuredBindings(srcUtf8);
            prescanLambdas(srcUtf8);
            prescanConcepts(srcUtf8);
            prescanAttributes(srcUtf8);
            prescanMultiDeclarators(srcUtf8);
            prescanDirectives();
        }

        if (lang == Lang::CSharp) {
            prescanCSharpExternalUsings(srcUtf8);
            prescanAttributeDecoratedDeclarations();
            // FIX (gap 5): must run before the method-name registration loop
            // below so a caller that needs protecting is already in
            // preserveSet by the time that loop decides whether to rename it.
            prescanCallerMemberName(srcUtf8);
            protectCallerMemberNameCallers(srcUtf8);
        }

        smatch m;

        try {
            if (lang == Lang::CSharp) {
                regex classRe(R"(\b(?:public\s+|private\s+|protected\s+|internal\s+|static\s+|sealed\s+|abstract\s+|partial\s+|file\s+)*\b(class|struct|interface|enum|record(?:\s+(?:class|struct))?)\s+(\w+))");
                string::const_iterator s1(srcUtf8.cbegin());
                while (regex_search(s1, srcUtf8.cend(), m, classRe)) {
                    wstring name = stringToWstring(m[2]);
                    size_t namePos = (size_t)(m[2].first - srcUtf8.cbegin());
                    bool verbatim = precededByAt(srcUtf8, namePos);
                    if (!isReservedHere(name, verbatim)) localTypeNames.insert(name);
                    if (!isReservedHere(name, verbatim) && !preserveSet.count(name)) {
                        localClasses.insert(name);
                        if (!excludeClassNames) registerRename(classMap, name);
                        registerAttributeAlias(name);
                    }
                    s1 = m.suffix().first;
                }
            }
            else {
                regex classRe(R"(\b(class|struct)\s+(\w+))");
                string::const_iterator s1(srcUtf8.cbegin());
                while (regex_search(s1, srcUtf8.cend(), m, classRe)) {
                    wstring name = stringToWstring(m[2]);
                    if (!isReservedHere(name)) localTypeNames.insert(name);
                    if (!isReservedHere(name) && !preserveSet.count(name)) {
                        localClasses.insert(name);
                        if (!excludeClassNames) registerRename(classMap, name);
                    }
                    s1 = m.suffix().first;
                }
                regex enumRe(R"(\benum\s+(?:class\s+|struct\s+)?(\w+))");
                s1 = srcUtf8.cbegin();
                while (regex_search(s1, srcUtf8.cend(), m, enumRe)) {
                    wstring name = stringToWstring(m[1]);
                    if (!isReservedHere(name)) localTypeNames.insert(name);
                    if (!isReservedHere(name) && !preserveSet.count(name)) {
                        localClasses.insert(name);
                        if (!excludeClassNames) registerRename(classMap, name);
                    }
                    s1 = m.suffix().first;
                }
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: class extraction regex failed (skipping)"), CLR_WARN); }

        if (lang == Lang::CSharp) prescanCSharpClosureConstructorParameters(srcUtf8);

        try {
            regex primCtorRe(R"(\b(?:class|record|struct)\s+(\w+)(?:\s*<[^>]*>)?\s*\(([^()]*)\))");
            for (sregex_iterator it(srcUtf8.begin(), srcUtf8.end(), primCtorRe), end; it != end; ++it) {
                wstring owner = stringToWstring((*it)[1]);
                registerParamNames(stringToWstring((*it)[2]), owner, owner);
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: primary-ctor prescan failed"), CLR_WARN); }

        try {
            regex funcHeadRe(
                R"(\b([A-Za-z_]\w*(?:\s*(?:::|\.)\s*[A-Za-z_]\w*)*(?:\s*<[^<>;{}]{0,512}>)?[\s\r\*&\[\]]{1,512})(~?[A-Za-z_]\w*)\s*\()"
            );
            regex funcTailRe(
                R"(^\s*(?:const\s*)?(override\s*)?(?:final\s*)?(?:noexcept\s*)?(?:\{|;|=>))"
            );
            string::const_iterator s1(srcUtf8.cbegin());
            while (regex_search(s1, srcUtf8.cend(), m, funcHeadRe)) {
                size_t headStart = (size_t)(m[0].first - srcUtf8.cbegin());
                size_t openParenPos = (size_t)(m[0].second - srcUtf8.cbegin()) - 1;
                size_t closeParenPos = findMatchingParen(srcUtf8, openParenPos);
                if (closeParenPos == string::npos) { s1 = srcUtf8.cbegin() + openParenPos + 1; continue; }
                string restUtf8 = srcUtf8.substr(closeParenPos + 1);
                smatch tailM;
                if (!regex_search(restUtf8, tailM, funcTailRe)) { s1 = srcUtf8.cbegin() + headStart + 1; continue; }
                wstring typeFrag = stringToWstring(m[1]);
                wstring name = stringToWstring(m[2]);
                wstring argsStr = stringToWstring(srcUtf8.substr(openParenPos + 1, closeParenPos - openParenPos - 1));
                wstring baseType = stripToBaseType(typeFrag);
                bool nameIsDtor = !name.empty() && name[0] == L'~';
                wstring bareName = nameIsDtor ? name.substr(1) : name;
                bool trailingOverride = tailM[1].matched;
                bool precedingOverride = hasPrecedingOverrideModifier(srcUtf8, headStart);
                bool isOverrideMethod = trailingOverride || precedingOverride;
                bool isGenuineDeclaration = !isStatementWord(baseType, lang);
                bool isCtorOrDtorShaped = nameIsDtor || isModifierOnlyWord(baseType, lang);

                if (isGenuineDeclaration) {
                    if (!isCtorOrDtorShaped) {
                        size_t namePos = (size_t)(m[2].first - srcUtf8.cbegin());
                        bool nameVerbatim = (lang == Lang::CSharp) && precededByAt(srcUtf8, namePos);
                        bool nameRejected = isReservedHere(bareName, nameVerbatim) || preserveSet.count(bareName) || bareName.empty()
                            || (bareName.size() > 1 && bareName[0] == L'_' && bareName[1] == L'_');

                        if (!nameRejected && lang == Lang::CSharp) {
                            static const regex eventHandlerParamRe(
                                R"(\(\s*object\s+\w+\s*,\s*[A-Za-z_]\w*EventArgs(?:\s*<[^>]*>)?\s+\w+\s*\))");
                            string paramText = srcUtf8.substr(openParenPos, closeParenPos - openParenPos + 1);
                            if (regex_match(paramText, eventHandlerParamRe)) {
                                preserveSet.insert(bareName);
                                nameRejected = true;
                                LogMessage(L"XAML pairing: preserving \"" + bareName + L"\" (event-handler signature).", CLR_TEXT_DIM);
                            }
                        }

                        if (isFrameworkMember(bareName)) nameRejected = true;

                        // If an override has no matching virtual/abstract/interface
                        // declaration in this project, it targets an external base API.
                        // Preserve it even when the framework member list is incomplete.
                        if (isOverrideMethod && !projectVirtualMethods.count(bareName))
                            nameRejected = true;

                        // FIX (issue 3): an "override" member must end up under the exact same obfuscated
                        // name as its base/interface declaration, in whichever file that lives, or the
                        // override no longer matches the base and C# reports "no suitable method found
                        // to override". The previous code pushed every override's name into preserveSet
                        // and skipped it -- so if this override happened to be scanned (source files are
                        // not guaranteed to be processed base-before-derived) before its base's own
                        // declaration, that insert poisoned preserveSet for this name project-wide: the
                        // base's declaration then hit the "preserveSet.count(bareName)" check above and
                        // was rejected too, leaving some derived classes calling the renamed base member
                        // while others still declared/called it under the original name -- exactly the
                        // "member does not exist" pattern reported across many unrelated derived classes.
                        // Overrides now simply join the one shared methodMap/localMethods entry: whichever
                        // declaration (base or override, in any file, in any processing order) is scanned
                        // first creates it, and every other one reuses it unchanged.
                        if (!nameRejected && !classMap.count(bareName)) {
                            localMethods.insert(bareName);
                            if (!methodMap.count(bareName)) {
                                registerRename(methodMap, bareName);
                            }
                            funcReturnTypes[bareName] = trimTypeFragment(typeFrag);
                        }

                        if (lang == Lang::CSharp) {
                            const wchar_t* verdict = isFrameworkMember(bareName) ? L"framework" : (nameRejected ? L"preserved" : L"will rename");
                            LogMessage(L"method candidate: " + bareName + L" [" + verdict + L"]", CLR_TEXT_DIM);
                        }
                    }
                    // FIX (a): pass the declaration's own (pre-rename) name in, so this
                    // declaration's parameters land in methodParams[bareName] and a
                    // named-argument label at a call site can be checked against THIS
                    // declaration specifically (see FIX (b) below), the same way VB's
                    // registerParamNamesVB already does via its methodName parameter.
                    // Ctor/dtor-shaped matches have no real enclosing declaration name
                    // to key on, so they keep the old flat-only registration
                    // (methodName left empty) exactly as before.
                    wstring ownerType = (lang == Lang::CSharp && !isCtorOrDtorShaped)
                        ? findEnclosingTypeName(srcUtf8, headStart) : L"";
                    registerParamNames(argsStr, (!isCtorOrDtorShaped ? bareName : L""), ownerType);
                }
                s1 = srcUtf8.cbegin() + closeParenPos + 1;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: function extraction regex failed (skipping)"), CLR_WARN); }

        if (lang == Lang::CSharp) {
            try {
                regex indexerRe(R"(\bthis\s*\[([^\]]*)\]\s*(?:\{|=>))");
                for (sregex_iterator it(srcUtf8.begin(), srcUtf8.end(), indexerRe), end; it != end; ++it) {
                    wstring inside = stringToWstring((*it)[1]);
                    registerParamNames(inside);
                }
            }
            catch (const regex_error&) { LogMessage(L"WARN: indexer-parameter prescan failed", CLR_WARN); }
        }

        // FIX: this block, and every other plain field/local-variable/parameter/
        // catch-variable/lambda-capture/structured-binding extraction path below
        // it in this function, no longer gates on isFrameworkMember(name). The
        // only reason to apply isFrameworkMember() at all is to protect a
        // declaration that is legitimately ALLOWED to collide in name with a
        // real framework member -- which is specifically an "override" of that
        // member (only methods/properties/events can be override). None of
        // these extraction shapes can ever BE an override -- this regex's own
        // modifier list does not even include "override"/"virtual"/"abstract",
        // and a for-loop variable, catch variable, lambda parameter, structured
        // binding, or local variable is never override-able syntax in the first
        // place -- so isFrameworkMember(name) here was only ever suppressing a
        // rename the user's own code needed, never protecting anything real.
        // (Property/event accessor extraction below DOES keep the check, but
        // gated on that specific declaration's own modifiers actually containing
        // "override" -- see declarationHasOverrideModifier.)
        try {
            wstring modifiers = (lang == Lang::CSharp)
                ? L"(?:public\\s+|private\\s+|protected\\s+|internal\\s+|static\\s+|readonly\\s+|const\\s+|volatile\\s+|required\\s+|new\\s+|unsafe\\s+)*"
                : L"(?:static\\s+|const\\s+|mutable\\s+|volatile\\s+|inline\\s+)*";
            string modifiersN = wstringToString(modifiers);
            string pattern = R"(\b)" + modifiersN +
                R"(([A-Za-z_]\w*(?:\s*(?:::|\.)\s*[A-Za-z_]\w*)*(?:\s*<[^<>;{}]{0,512}>)?[\*&]*)\s+(\w+)\s*(?:=[^;\r\n]{0,4096})?;)";
            regex fieldRe(pattern);
            string::const_iterator s1(srcUtf8.cbegin());
            while (regex_search(s1, srcUtf8.cend(), m, fieldRe)) {
                wstring typeFrag = stringToWstring(m[1]);
                wstring name = stringToWstring(m[2]);
                size_t namePos = (size_t)(m[2].first - srcUtf8.cbegin());
                bool verbatim = (lang == Lang::CSharp) && precededByAt(srcUtf8, namePos);
                wstring baseType = stripToBaseType(typeFrag);
                bool reject = isStatementWord(baseType, lang) || isReservedHere(name, verbatim) ||
                    preserveSet.count(name) || name.empty();
                if (!name.empty() && !isStatementWord(baseType, lang)) {
                    wstring fullType = trimTypeFragment(typeFrag);
                    if (fullType == L"var") {
                        static const regex varCastRe(R"(=\s*\(\s*([A-Za-z_]\w*(?:\s*\.\s*[A-Za-z_]\w*)*)\s*\))");
                        static const regex varNewRe(R"(=\s*new\s+([A-Za-z_]\w*(?:\s*\.\s*[A-Za-z_]\w*)*))");
                        smatch cm;
                        string wholeMatch = m[0].str();
                        if (regex_search(wholeMatch, cm, varCastRe) || regex_search(wholeMatch, cm, varNewRe))
                            fullType = stringToWstring(cm[1].str());
                    }
                    if (!fullType.empty() && fullType != L"var") recordVarType(name, fullType);
                }
                if (!reject && !methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                    localFields.insert(name);
                    registerRename(fieldMap, name);
                }
                s1 = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: field extraction regex failed (skipping)"), CLR_WARN); }

        // FIX (missing C# declaration coverage): auto-implemented properties
        // ("public string Name { get; set; }"), expression-bodied properties
        // ("public string Name => _name;"), and accessor-style events
        // ("public event Foo Bar { add {...} remove {...} }") never matched the
        // field regex above (no trailing ";" right after the name) or the
        // function-header regex (no "(" right after the name). They were simply
        // never scanned -- not protected, just invisible, which is exactly the
        // collision risk described for VB above: an unrelated field elsewhere in
        // the project with the identical spelling could still get renamed while
        // the property/event declaration itself silently kept the old name.
        if (lang == Lang::CSharp) {
            try {
                regex propAccessorRe(
                    R"(\b(?:public\s+|private\s+|protected\s+|internal\s+|static\s+|virtual\s+|override\s+|)"
                    R"(abstract\s+|sealed\s+|new\s+|required\s+|readonly\s+|unsafe\s+|extern\s+)*)"
                    R"(([A-Za-z_]\w*(?:\s*(?:::|\.)\s*[A-Za-z_]\w*)*(?:\s*<[^<>;{}]{0,512}>)?[\*&\?]*)\s+(\w+)\s*)"
                    R"((?:\{\s*(?:get|set|init)\b|=>))");
                string::const_iterator s1(srcUtf8.cbegin());
                while (regex_search(s1, srcUtf8.cend(), m, propAccessorRe)) {
                    wstring typeFrag = stringToWstring(m[1]);
                    wstring name = stringToWstring(m[2]);
                    wstring baseType = stripToBaseType(typeFrag);
                    // FIX: isFrameworkMember() only applies when THIS declaration is
                    // itself marked "override" -- see declarationHasOverrideModifier.
                    size_t matchStart = (size_t)(m[0].first - srcUtf8.cbegin());
                    bool protectedByOverride = declarationHasOverrideModifier(srcUtf8, matchStart);
                    // We do not yet build a complete property/event override graph across
                    // files and external base assemblies. Preserve every override property
                    // or accessor-style event rather than risk renaming an inherited API.
                    if (protectedByOverride) preserveSet.insert(name);
                    if (!isStatementWord(baseType, lang) && !name.empty() &&
                        !isReservedHere(name) && !preserveSet.count(name) && !protectedByOverride &&
                        !methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                        localFields.insert(name);
                        registerRename(fieldMap, name);
                    }
                    s1 = m.suffix().first;
                }
            }
            catch (const regex_error&) { LogMessage(wstring(L"WARN: property/accessor-event extraction regex failed (skipping)"), CLR_WARN); }

            try {
                regex eventAccessorRe(
                    R"(\bevent\s+([A-Za-z_]\w*(?:\s*(?:::|\.)\s*[A-Za-z_]\w*)*(?:\s*<[^<>;{}]{0,512}>)?[\*&]*)\s+(\w+)\s*\{\s*(?:add|remove)\b)");
                string::const_iterator s1(srcUtf8.cbegin());
                while (regex_search(s1, srcUtf8.cend(), m, eventAccessorRe)) {
                    wstring name = stringToWstring(m[2]);
                    // FIX: same override-modifier gating as propAccessorRe above. This
                    // regex's own pattern does not capture modifiers (it anchors on the
                    // literal word "event"), so the scan has to look at the text before
                    // the match -- declarationHasOverrideModifier handles that either way.
                    size_t matchStart = (size_t)(m[0].first - srcUtf8.cbegin());
                    bool protectedByOverride = declarationHasOverrideModifier(srcUtf8, matchStart);
                    if (protectedByOverride) preserveSet.insert(name);
                    if (!name.empty() && !isReservedHere(name) && !preserveSet.count(name) && !protectedByOverride &&
                        !methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                        localFields.insert(name);
                        registerRename(fieldMap, name);
                    }
                    s1 = m.suffix().first;
                }
            }
            catch (const regex_error&) { LogMessage(wstring(L"WARN: accessor-style event extraction regex failed (skipping)"), CLR_WARN); }
        }

        try {
            regex ctorInitRe(R"(\b([A-Za-z_]\w*)\s+(\w+)\s*\([^;(){}]{0,2048}\)\s*;)");
            string::const_iterator s1(srcUtf8.cbegin());
            while (regex_search(s1, srcUtf8.cend(), m, ctorInitRe)) {
                wstring baseType = stringToWstring(m[1]);
                wstring name = stringToWstring(m[2]);
                if (!name.empty() && (localTypeNames.count(baseType) || localClasses.count(baseType))) {
                    recordVarType(name, baseType);
                }
                s1 = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: ctor-init type extraction regex failed (skipping)"), CLR_WARN); }

        try {
            regex forRe(R"(\bfor\s*\(\s*(?:[A-Za-z_][\w:]{0,255}\s*[\*&]?\s*)+?(\w+)\s*=)");
            string::const_iterator s1(srcUtf8.cbegin());
            while (regex_search(s1, srcUtf8.cend(), m, forRe)) {
                wstring name = stringToWstring(m[1]);
                if (!isReservedHere(name) && !preserveSet.count(name) &&
                    !methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                    localFields.insert(name);
                    registerRename(fieldMap, name);
                }
                s1 = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: for-loop regex failed"), CLR_WARN); }

        try {
            if (lang == Lang::CSharp) {
                regex rangeRe(R"(\bforeach\s*\(\s*(var|[A-Za-z_][\w:<>\[\],\s]{0,512}?)\s+(\w+)\s+in\b)");
                string::const_iterator s1(srcUtf8.cbegin());
                while (regex_search(s1, srcUtf8.cend(), m, rangeRe)) {
                    wstring name = stringToWstring(m[2]);
                    wstring typeTxt = stringToWstring(m[1]);
                    if (!iequals(typeTxt, L"var")) {
                        wstring baseT = firstWordOf(typeTxt);
                        if (!baseT.empty() && !isStatementWord(baseT, lang)) {
                            wstring fullType = trimTypeFragment(typeTxt);
                            if (!fullType.empty()) recordVarType(name, fullType);
                        }
                    }
                    if (!isReservedHere(name) && !preserveSet.count(name) &&
                        !methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                        localFields.insert(name);
                        registerRename(fieldMap, name);
                    }
                    s1 = m.suffix().first;
                }
                regex decomRe(R"(\bforeach\s*\(\s*(?:var|[A-Za-z_][\w:<>\[\],\s]{0,512}?)\s*\(([^()]*)\)\s+in\b)");
                string::const_iterator s2(srcUtf8.cbegin());
                while (regex_search(s2, srcUtf8.cend(), m, decomRe)) {
                    wstring inside = stringToWstring(m[1]);
                    size_t st = 0;
                    for (size_t i = 0; i <= inside.size(); i++) {
                        if (i != inside.size() && inside[i] != L',') continue;
                        wstring n = trimW(inside.substr(st, i - st));
                        st = i + 1;
                        while (!n.empty() && (n[0] == L'&' || n[0] == L'*' || iswspace(n[0]))) n.erase(0, 1);
                        if (n.empty() || isReservedHere(n)) continue;
                        if (methodMap.count(n) || classMap.count(n) || fieldMap.count(n)) continue;
                        localFields.insert(n);
                        registerRename(fieldMap, n);
                    }
                    s2 = m.suffix().first;
                }
            }
            else {
                regex rangeRe(R"(\bfor\s*\(\s*(?:auto|[A-Za-z_][\w:<>\*&]{0,255})\s*[&\*]?\s*(\w+)\s*:)");
                string::const_iterator s1(srcUtf8.cbegin());
                while (regex_search(s1, srcUtf8.cend(), m, rangeRe)) {
                    wstring name = stringToWstring(m[1]);
                    if (!isReservedHere(name) && !preserveSet.count(name) &&
                        !methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                        localFields.insert(name);
                        registerRename(fieldMap, name);
                    }
                    s1 = m.suffix().first;
                }
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: range-for regex failed"), CLR_WARN); }

        try {
            regex catchRe(R"(\bcatch\s*\(\s*[A-Za-z_][\w:<>]{0,255}\s*[&\*]?\s*(\w+)\s*\))");
            string::const_iterator s1(srcUtf8.cbegin());
            while (regex_search(s1, srcUtf8.cend(), m, catchRe)) {
                wstring name = stringToWstring(m[1]);
                if (!isReservedHere(name) && !preserveSet.count(name) &&
                    !methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                    localFields.insert(name);
                    registerRename(fieldMap, name);
                }
                s1 = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: catch regex failed"), CLR_WARN); }

        try {
            regex localVarRe(R"(\b(?:var|int|string|bool|double|float|long|short|byte|char|decimal|object|dynamic|auto|unsigned|size_t|wstring|std::string|std::wstring)\s+(\w+)\s*(?:=[^;\r\n]{0,4096})?;)");
            string::const_iterator s1(srcUtf8.cbegin());
            while (regex_search(s1, srcUtf8.cend(), m, localVarRe)) {
                wstring name = stringToWstring(m[1]);
                if (!isReservedHere(name) && !preserveSet.count(name) &&
                    !methodMap.count(name) && !classMap.count(name) && !fieldMap.count(name)) {
                    localFields.insert(name);
                    registerRename(fieldMap, name);
                }
                s1 = m.suffix().first;
            }
        }
        catch (const regex_error&) { LogMessage(wstring(L"WARN: local-var regex failed"), CLR_WARN); }

        if (lang == Lang::Cpp) prescanAutoReturn(srcUtf8);

        if (lang == Lang::CSharp || lang == Lang::VB) prescanClassMembers(srcUtf8);
        if (lang == Lang::CSharp) {
            prescanAnonymousTypeMembers(srcUtf8);
            warnCompilerGeneratedNames(srcUtf8);
        }

        if (lang == Lang::VB) {
            try {
                regex vbTypeRe(R"(\b(?:Dim|Private|Public|Friend|Protected|WithEvents|Static)\s+(?:ReadOnly\s+|Shared\s+|)\s*([A-Za-z_]\w*)\s+As\s+([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*))", regex::icase);
                for (sregex_iterator vi(srcUtf8.begin(), srcUtf8.end(), vbTypeRe), ve; vi != ve; ++vi)
                    recordVarType(stringToWstring((*vi)[1]), stringToWstring((*vi)[2]));
            }
            catch (const regex_error&) { LogMessage(L"WARN: VB variable-type prescan regex failed; With-block label rewrites will be skipped.", CLR_WARN); }
        }

        if (lang == Lang::CSharp) {
            try {
                if (regex_search(srcUtf8, regex(R"(\bpartial\s+(?:class|struct|interface|void|[A-Za-z_]\w*)\b)", regex::icase))) {
                    LogMessage(L"WARN: Partial method detected -- if the implementation is in another file, "
                        L"obfuscate both files together or the pair will mismatch.", CLR_WARN);
                }
            }
            catch (const regex_error&) {}
        }
    }

    vector<Token> tokenizeLineVB(const wstring& line) {
        vector<Token> tokens;
        size_t i = 0;
        while (i < line.length()) {
            if (line[i] == L'\'') {
                Token t; t.text = line.substr(i); t.isComment = true;
                tokens.push_back(t); break;
            }
            if (line[i] == L'"' || (line[i] == L'$' && i + 1 < line.length() && line[i + 1] == L'"')) {
                size_t start = i;
                bool isInterpolated = (line[i] == L'$');
                if (isInterpolated) i++;
                i++;
                while (i < line.length()) {
                    if (line[i] == L'"' && i + 1 < line.length() && line[i + 1] == L'"') i += 2;
                    else if (line[i] == L'"') { i++; break; }
                    else if (isInterpolated && line[i] == L'{' && (i + 1 >= line.length() || line[i + 1] != L'{')) {
                        if (i > start) {
                            Token t; t.text = line.substr(start, i - start); t.isStringLiteral = true;
                            tokens.push_back(t);
                        }
                        size_t exprStart = i;
                        int braceDepth = 1;
                        i++;
                        while (i < line.length() && braceDepth > 0) {
                            if (line[i] == L'{') braceDepth++;
                            else if (line[i] == L'}') braceDepth--;
                            i++;
                        }
                        wstring exprContent = line.substr(exprStart + 1, i - exprStart - 2);
                        vector<Token> exprTokens = tokenizeLineVB(exprContent);
                        Token openBrace; openBrace.text = L"{"; tokens.push_back(openBrace);
                        for (auto& et : exprTokens) tokens.push_back(et);
                        Token closeBrace; closeBrace.text = L"}"; tokens.push_back(closeBrace);
                        start = i;
                    }
                    else i++;
                }
                if (start < i) {
                    Token t; t.text = line.substr(start, i - start); t.isStringLiteral = true;
                    tokens.push_back(t);
                }
                continue;
            }
            if (iswspace(line[i])) {
                size_t start = i;
                while (i < line.length() && iswspace(line[i])) i++;
                Token t; t.text = line.substr(start, i - start);
                tokens.push_back(t); continue;
            }
            if (iswalnum(line[i]) || line[i] == L'_') {
                size_t start = i;
                while (i < line.length() && (iswalnum(line[i]) || line[i] == L'_')) i++;
                Token t; t.text = line.substr(start, i - start); t.isWord = true;
                tokens.push_back(t); continue;
            }
            Token t; t.text = wstring(1, line[i]);
            tokens.push_back(t); i++;
        }
        return tokens;
    }

    vector<Token> tokenizeLineCLike(const wstring& lineIn) {
        vector<Token> tokens;
        wstring line = lineIn;
        size_t i = 0;

        if (inRawString) {
            if (line.empty()) { return tokens; }
            size_t end = line.find(rawStringEndSeq);
            if (end == wstring::npos) {
                Token t; t.text = line; t.isStringLiteral = true;
                tokens.push_back(t); return tokens;
            }
            size_t endLen = rawStringEndSeq.length();
            Token t; t.text = line.substr(0, end + endLen); t.isStringLiteral = true;
            tokens.push_back(t);
            inRawString = false;
            rawStringEndSeq.clear();
            i = end + endLen;
        }

        if (inBlockComment) {
            size_t endPos = line.find(L"*/", i);
            if (endPos == wstring::npos) {
                Token t; t.text = line; t.isComment = true;
                tokens.push_back(t); return tokens;
            }
            else {
                Token t; t.text = line.substr(0, endPos + 2); t.isComment = true;
                tokens.push_back(t);
                inBlockComment = false;
                i = endPos + 2;
            }
        }
        while (i < line.length()) {
            if (line[i] == L'/' && i + 1 < line.length() && line[i + 1] == L'/') {
                Token t; t.text = line.substr(i); t.isComment = true;
                tokens.push_back(t); break;
            }
            if (line[i] == L'/' && i + 1 < line.length() && line[i + 1] == L'*') {
                size_t endPos = line.find(L"*/", i + 2);
                if (endPos == wstring::npos) {
                    Token t; t.text = line.substr(i); t.isComment = true;
                    tokens.push_back(t); inBlockComment = true; break;
                }
                else {
                    Token t; t.text = line.substr(i, endPos + 2 - i); t.isComment = true;
                    tokens.push_back(t);
                    i = endPos + 2; continue;
                }
            }
            if (lang == Lang::CSharp && line[i] == L'@' && i + 1 < line.length() && line[i + 1] == L'"') {
                size_t start = i;
                i += 2;
                while (i < line.length()) {
                    if (line[i] == L'"' && i + 1 < line.length() && line[i + 1] == L'"') i += 2;
                    else if (line[i] == L'"') { i++; break; }
                    else i++;
                }
                Token t; t.text = line.substr(start, i - start); t.isStringLiteral = true;
                tokens.push_back(t); continue;
            }
            if (lang == Lang::CSharp && line[i] == L'$' && i + 1 < line.length() &&
                (line[i + 1] == L'"' || (line[i + 1] == L'@' && i + 2 < line.length() && line[i + 2] == L'"'))) {
                bool verbatim = (line[i + 1] == L'@');
                size_t start = i;
                i += verbatim ? 3 : 2;
                while (i < line.length()) {
                    if (verbatim && line[i] == L'"' && i + 1 < line.length() && line[i + 1] == L'"') { i += 2; continue; }
                    if (!verbatim && line[i] == L'\\' && i + 1 < line.length()) { i += 2; continue; }
                    if (line[i] == L'"') { i++; break; }
                    if (line[i] == L'{' && i + 1 < line.length() && line[i + 1] == L'{') { i += 2; continue; }
                    if (line[i] == L'}' && i + 1 < line.length() && line[i + 1] == L'}') { i += 2; continue; }
                    if (line[i] == L'{') {
                        if (i > start) {
                            Token t; t.text = line.substr(start, i - start); t.isStringLiteral = true;
                            tokens.push_back(t);
                        }
                        size_t exprStart = i;
                        int depth = 1; i++;
                        while (i < line.length() && depth > 0) {
                            if (line[i] == L'{') depth++;
                            else if (line[i] == L'}') depth--;
                            if (depth > 0) i++;
                        }
                        size_t exprEnd = (i < line.length() && line[i] == L'}') ? i : line.length();
                        if (exprEnd <= exprStart + 1) { i++; start = i; continue; }
                        wstring exprContent = line.substr(exprStart + 1, exprEnd - (exprStart + 1));

                        Token openBrace; openBrace.text = L"{"; tokens.push_back(openBrace);
                        for (auto& et : tokenizeLineCLike(exprContent)) tokens.push_back(et);
                        if (i < line.length() && line[i] == L'}') i++;
                        Token closeBrace; closeBrace.text = L"}"; tokens.push_back(closeBrace);
                        start = i; continue;
                    }
                    i++;
                }
                if (start < i) {
                    Token t; t.text = line.substr(start, i - start); t.isStringLiteral = true;
                    tokens.push_back(t);
                }
                continue;
            }
            if (iswalnum(line[i]) || line[i] == L'_') {
                if (lang == Lang::Cpp) {
                    size_t we = i;
                    while (we < line.length() && (iswalnum(line[we]) || line[we] == L'_')) we++;
                    if (we < line.length() && line[we] == L'"') {
                        wstring cand = line.substr(i, we - i);
                        if (cand == L"R" || cand == L"LR" || cand == L"uR" ||
                            cand == L"UR" || cand == L"u8R") {
                            size_t delimStart = we + 1, j = delimStart;
                            bool validDelim = true;
                            while (j < line.length() && line[j] != L'(') {
                                wchar_t c = line[j];
                                if (iswspace(c) || c == L')' || c == L'\\' || c == L'"' || (j - delimStart) >= 16) {
                                    validDelim = false; break;
                                }
                                j++;
                            }
                            if (validDelim && j < line.length() && line[j] == L'(') {
                                wstring delim = line.substr(delimStart, j - delimStart);
                                wstring endSeq = L")" + delim + L"\"";
                                size_t endMarker = line.find(endSeq, j + 1);
                                size_t start = i;
                                if (endMarker == wstring::npos) {
                                    Token t; t.text = line.substr(start); t.isStringLiteral = true;
                                    tokens.push_back(t);
                                    inRawString = true; rawStringEndSeq = endSeq;
                                    i = line.length();
                                }
                                else {
                                    size_t endPos = endMarker + endSeq.length();
                                    Token t; t.text = line.substr(start, endPos - start); t.isStringLiteral = true;
                                    tokens.push_back(t);
                                    i = endPos;
                                }
                                continue;
                            }
                        }
                    }
                }
                size_t start = i;
                while (i < line.length() && (iswalnum(line[i]) || line[i] == L'_')) i++;
                Token t; t.text = line.substr(start, i - start); t.isWord = true;
                tokens.push_back(t); continue;
            }
            if (line[i] == L'"') {
                size_t start = i;
                i++;
                while (i < line.length()) {
                    if (line[i] == L'\\' && i + 1 < line.length()) { i += 2; continue; }
                    if (line[i] == L'"') { i++; break; }
                    i++;
                }
                Token t; t.text = line.substr(start, i - start); t.isStringLiteral = true;
                tokens.push_back(t); continue;
            }
            if (line[i] == L'\'') {
                bool isDigitSep = false;
                if (i > 0 && i + 1 < line.length()) {
                    wchar_t prev = line[i - 1];
                    wchar_t next = line[i + 1];
                    if ((iswdigit(prev) ||
                        (prev >= L'a' && prev <= L'f') ||
                        (prev >= L'A' && prev <= L'F')) &&
                        (iswalnum(next) || next == L'\'')) {
                        isDigitSep = true;
                    }
                }
                if (isDigitSep) { i++; continue; }

                size_t start = i;
                i++;
                while (i < line.length()) {
                    if (line[i] == L'\\' && i + 1 < line.length()) { i += 2; continue; }
                    if (line[i] == L'\'') { i++; break; }
                    i++;
                }
                Token t; t.text = line.substr(start, i - start); t.isStringLiteral = true;
                tokens.push_back(t); continue;
            }
            if (iswspace(line[i])) {
                size_t start = i;
                while (i < line.length() && iswspace(line[i])) i++;
                Token t; t.text = line.substr(start, i - start);
                tokens.push_back(t); continue;
            }
            if (iswalnum(line[i]) || line[i] == L'_') {
                if (lang == Lang::Cpp) {
                    size_t we = i;
                    while (we < line.length() && (iswalnum(line[we]) || line[we] == L'_')) we++;
                    if (we < line.length() && line[we] == L'"') {
                        wstring cand = line.substr(i, we - i);
                        if (cand == L"R" || cand == L"LR" || cand == L"uR" ||
                            cand == L"UR" || cand == L"u8R") {
                            size_t delimStart = we + 1, j = delimStart;
                            bool validDelim = true;
                            while (j < line.length() && line[j] != L'(') {
                                wchar_t c = line[j];
                                if (iswspace(c) || c == L')' || c == L'\\' || c == L'"' || (j - delimStart) >= 16) {
                                    validDelim = false; break;
                                }
                                j++;
                            }
                            if (validDelim && j < line.length() && line[j] == L'(') {
                                wstring delim = line.substr(delimStart, j - delimStart);
                                wstring endSeq = L")" + delim + L"\"";
                                size_t endMarker = line.find(endSeq, j + 1);
                                size_t start = i;
                                if (endMarker == wstring::npos) {
                                    Token t; t.text = line.substr(start); t.isStringLiteral = true;
                                    tokens.push_back(t);
                                    inRawString = true; rawStringEndSeq = endSeq;
                                    i = line.length();
                                }
                                else {
                                    size_t endPos = endMarker + endSeq.length();
                                    Token t; t.text = line.substr(start, endPos - start); t.isStringLiteral = true;
                                    tokens.push_back(t);
                                    i = endPos;
                                }
                                continue;
                            }
                        }
                    }
                }
                size_t start = i;
                while (i < line.length() && (iswalnum(line[i]) || line[i] == L'_')) i++;
                Token t; t.text = line.substr(start, i - start); t.isWord = true;
                tokens.push_back(t); continue;
            }
            if ((line[i] == L'-' && i + 1 < line.length() && line[i + 1] == L'>') ||
                (line[i] == L':' && i + 1 < line.length() && line[i + 1] == L':')) {
                Token t; t.text = line.substr(i, 2);
                tokens.push_back(t); i += 2; continue;
            }
            Token t; t.text = wstring(1, line[i]);
            tokens.push_back(t); i++;
        }
        return tokens;
    }

    wstring processLine(const wstring& line) {
        if (lang == Lang::VB) return processLineVB(line);
        return processLineCLike(line);
    }

    static wstring findPrevMeaningful(const vector<Token>& tokens, size_t i) {
        for (int j = (int)i - 1; j >= 0; j--)
            if (!tokens[j].text.empty() && !iswspace(tokens[j].text[0])) return tokens[j].text;
        return L"";
    }

    static wstring findNextMeaningful(const vector<Token>& tokens, size_t i) {
        for (size_t j = i + 1; j < tokens.size(); j++)
            if (!tokens[j].text.empty() && !iswspace(tokens[j].text[0])) return tokens[j].text;
        return L"";
    }

    static int findPrevMeaningfulIndex(const vector<Token>& tokens, size_t i) {
        for (int j = (int)i - 1; j >= 0; j--)
            if (!tokens[j].text.empty() && !iswspace(tokens[j].text[0])) return j;
        return -1;
    }

    static int findNextMeaningfulIndex(const vector<Token>& tokens, size_t i) {
        for (size_t j = i + 1; j < tokens.size(); j++)
            if (!tokens[j].text.empty() && !iswspace(tokens[j].text[0])) return (int)j;
        return -1;
    }

    // FIX (A0.2/A0.4): a generic call site -- "Foo.Bar<T>(...)" or a bare
    // "Bar<T>(...)" -- has "<" as the next meaningful token after the method
    // name, not "(". Every call-detection check in this file keys specifically
    // on "nextToken == \"(\"", so a generic call never matched it and the method
    // name was left un-renamed even though its declaration (and every
    // non-generic call to the same method) was renamed -- the exact split
    // reported for WpfWindowTracker.TryGetWindow<T>(...). This skips a single
    // balanced "<...>" type-argument list right after tokens[i] (nesting
    // included, so "Dictionary<string, List<int>>" skips correctly as a unit)
    // and returns whatever comes after it -- "(" for a real generic call -- so
    // the existing "nextToken == \"(\"" checks can be pointed at this instead
    // and fire the same way for both shapes. If what follows "<" does not look
    // like a type-argument list (hits ';', '{', '}', or never closes on this
    // line), it returns "<" itself -- i.e. behaves exactly like a plain lookup,
    // so this is a pure superset of the old behavior with no regression risk.
    wstring findNextMeaningfulSkippingGenericArgs(const vector<Token>& tokens, size_t i) {
        int j = findNextMeaningfulIndex(tokens, i);
        if (j < 0) return L"";
        if (tokens[(size_t)j].text != L"<") return tokens[(size_t)j].text;
        int depth = 0;
        size_t k = (size_t)j;
        bool bailed = false;
        while (k < tokens.size()) {
            const wstring& tx = tokens[k].text;
            if (!tx.empty() && !iswspace(tx[0])) {
                if (tx == L"<") depth++;
                else if (tx == L">") {
                    depth--;
                    if (depth == 0) { k++; break; }
                }
                else if (tx == L";" || tx == L"{" || tx == L"}") { bailed = true; break; }
            }
            k++;
        }
        if (bailed || depth != 0) return tokens[(size_t)j].text;   // not actually a type-arg list -- plain "<"
        return findNextMeaningful(tokens, k - 1);
    }

    bool isMethodReferenceContext(const vector<Token>& tokens, size_t currentIdx) {
        for (int j = (int)currentIdx - 1; j >= 0; --j) {
            if (tokens[j].text.empty() || iswspace(tokens[j].text[0])) continue;
            const wstring& txt = tokens[j].text;
            if (iequals(txt, L"Me") || iequals(txt, L"MyBase") || iequals(txt, L"MyClass") || txt == L".") continue;
            if (iequals(txt, L"AddressOf") || iequals(txt, L"RemoveHandler") ||
                iequals(txt, L"Handles") || iequals(txt, L"Implements")) return true;
            break;
        }
        return false;
    }

    wstring processLineVB(const wstring& line) {
        vector<Token> tokens = tokenizeLineVB(line);
        wstringstream result;
        for (size_t i = 0; i < tokens.size(); i++) {
            const Token& tok = tokens[i];
            if (!tok.isWord) { result << tok.text; continue; }
            wstring token = tok.text;
            // FIX (VB case-insensitivity): normalize to the one spelling the
            // symbol's map entry actually lives under, if it has one, before
            // any of the case-sensitive map/set lookups below run. A token
            // with no renamable entry is returned unchanged, so un-renamed
            // code keeps the exact casing the user wrote.
            token = canonicalizeVBToken(token);
            wstring prevToken = findPrevMeaningful(tokens, i);
            wstring nextToken = findNextMeaningful(tokens, i);
            wstring prevPrevToken;
            {
                bool foundFirst = false;
                for (int j = (int)i - 1; j >= 0; j--) {
                    if (!tokens[j].text.empty() && !iswspace(tokens[j].text[0])) {
                        if (!foundFirst) foundFirst = true;
                        else { prevPrevToken = tokens[j].text; break; }
                    }
                }
            }
            bool isLocalClass = localClasses.count(token) != 0;
            bool isLocalMethod = localMethods.count(token) != 0;
            bool isLocalField = localFields.count(token) != 0;

            // Track With-expression receiver type across source lines. A member
            // label is rewritten only when it is declared on that resolved type.
            if (iequals(token, L"End") && iequals(nextToken, L"With")) {
                if (!vbWithTypeStack.empty()) vbWithTypeStack.pop_back();
            }
            if (iequals(token, L"With")) {
                int ni = findNextMeaningfulIndex(tokens, i);
                wstring expr = ni >= 0 ? tokens[(size_t)ni].text : L"";
                wstring typeName;
                if (iequals(expr, L"New")) {
                    int ti = ni >= 0 ? findNextMeaningfulIndex(tokens, (size_t)ni) : -1;
                    if (ti >= 0) typeName = tokens[(size_t)ti].text;
                }
                else {
                    if (const wstring* vt = findVarType(expr)) typeName = *vt;
                }
                size_t dot = typeName.find_last_of(L".");
                if (dot != wstring::npos) typeName = typeName.substr(dot + 1);
                vbWithTypeStack.push_back(typeName);
            }

            // FIX: VB named-argument label, "Foo(bar:=1)". VB tokenizes ":=" as two
            // separate tokens (":" then "="), so the shape is identifier, ":", "=" --
            // not a single combined operator. Renaming the label must agree with the
            // SPECIFIC declaration the call resolves to, not "is this name a parameter
            // of anything, anywhere" (two unrelated Subs can easily share a common
            // parameter name): find the open paren of this argument list, take the
            // identifier right before it as the call target, confirm that target is a
            // known local method/class, and only then check methodParams[callTarget]
            // -- the set of names actually declared as ITS OWN parameters -- for this
            // label, before renaming it via fieldMap.
            if (nextToken == L":" && (prevToken == L"(" || prevToken == L",")) {
                int colonIdx = findNextMeaningfulIndex(tokens, i);
                wstring afterColon = colonIdx >= 0 ? findNextMeaningful(tokens, (size_t)colonIdx) : L"";
                if (afterColon == L"=") {
                    int depth = 0;
                    int openParenIdx = -1;
                    for (int j = (int)i - 1; j >= 0; j--) {
                        const wstring& tx = tokens[j].text;
                        if (tx.empty() || iswspace(tx[0])) continue;
                        if (tx == L")") depth++;
                        else if (tx == L"(") { if (depth == 0) { openParenIdx = j; break; } depth--; }
                    }
                    wstring callTarget;
                    if (openParenIdx >= 0) {
                        int ctIdx = findPrevMeaningfulIndex(tokens, (size_t)openParenIdx);
                        if (ctIdx >= 0) callTarget = tokens[ctIdx].text;
                    }
                    bool calleeIsLocal = !callTarget.empty() && (methodMap.count(callTarget) || classMap.count(callTarget));
                    if (calleeIsLocal) {
                        auto it = methodParams.find(callTarget);
                        if (it != methodParams.end() && it->second.count(token) &&
                            fieldMap.count(token) && localFields.count(token)) {
                            result << fieldMap[token]; continue;
                        }
                    }
                    result << token; continue;
                }
            }

            if (prevToken == L".") {
                if (!vbWithTypeStack.empty() && !vbWithTypeStack.back().empty()) {
                    auto wi = classMembersByType.find(vbWithTypeStack.back());
                    if (wi != classMembersByType.end() && wi->second.count(token) &&
                        fieldMap.count(token) && localFields.count(token)) {
                        result << fieldMap[token]; continue;
                    }
                }
                // FIX (A2): short-circuit "My.X" before any of the ordinary
                // isLocalField/isLocalMethod checks below, so a local field or
                // method that happens to share a name with one of My's members
                // (a user's own "Dim Settings As String", say) can never get
                // substituted in here -- only a real "My.Settings" is matched.
                {
                    int dotIdx = findPrevMeaningfulIndex(tokens, i);
                    int objIdx = dotIdx >= 0 ? findPrevMeaningfulIndex(tokens, (size_t)dotIdx) : -1;
                    wstring objToken = objIdx >= 0 ? tokens[objIdx].text : L"";
                    if (iequals(objToken, L"My")) {
                        bool isMyMember = false;
                        for (const auto& m : VB_MY_NAMESPACE) { if (iequals(m, token)) { isMyMember = true; break; } }
                        if (isMyMember) { result << token; continue; }
                    }
                }
                if (skipExternalMembers) {
                    wstring classToken;
                    int bracketDepth = 0;
                    for (int j = (int)i - 2; j >= 0; j--) {
                        if (tokens[j].text.empty() || iswspace(tokens[j].text[0])) continue;
                        if (tokens[j].text == L")") bracketDepth++;
                        else if (tokens[j].text == L"(") { bracketDepth--; if (bracketDepth < 0) break; }
                        else if (bracketDepth == 0) { classToken = tokens[j].text; break; }
                    }
                    bool isLocalContext = iequals(classToken, L"Me") || iequals(classToken, L"MyBase") ||
                        iequals(classToken, L"MyClass") || localClasses.count(classToken) != 0;
                    if (!isLocalContext) { result << token; continue; }
                }
                if (isMethodReferenceContext(tokens, i) && methodMap.count(token) && isLocalMethod) result << methodMap[token];
                else if (fieldMap.count(token) && isLocalField) result << fieldMap[token];
                else result << token;
                continue;
            }
            if (iequals(prevToken, L"As") || iequals(prevToken, L"Of")) {
                if (!isLocalClass) { result << token; continue; }
            }
            if (iequals(prevToken, L"New")) {
                if (!isLocalClass) { result << token; continue; }
            }
            if (nextToken == L"(") {
                if (isLocalMethod && methodMap.count(token)) { result << methodMap[token]; continue; }
                if (isLocalField && fieldMap.count(token)) { result << fieldMap[token]; continue; }
                result << token; continue;
            }
            if ((iequals(prevToken, L"Dim") || iequals(prevToken, L"Private") || iequals(prevToken, L"Public") ||
                iequals(prevToken, L"Friend") || iequals(prevToken, L"Protected") || iequals(prevToken, L"Static") ||
                iequals(prevToken, L"ReadOnly") || iequals(prevToken, L"Shared") || iequals(prevToken, L"WithEvents") ||
                iequals(prevToken, L"Const")) && nextToken != L"(") {
                if (fieldMap.count(token)) { result << fieldMap[token]; continue; }
            }
            if (iequals(prevToken, L"Each") && iequals(prevPrevToken, L"For")) {
                if (fieldMap.count(token)) { result << fieldMap[token]; continue; }
            }
            if (iequals(prevToken, L"For") && iequals(nextToken, L"As")) {
                if (fieldMap.count(token)) { result << fieldMap[token]; continue; }
            }
            if (iequals(prevToken, L"Catch")) {
                if (fieldMap.count(token)) { result << fieldMap[token]; continue; }
            }
            if ((iequals(prevToken, L"Sub") || iequals(prevToken, L"Function")) && nextToken == L")") {
                if (fieldMap.count(token)) { result << fieldMap[token]; continue; }
            }
            if (prevToken == L"(" || prevToken == L",") {
                bool inLambdaDecl = false;
                for (int j = (int)i - 1; j >= 0; j--) {
                    if (iequals(tokens[j].text, L"Sub") || iequals(tokens[j].text, L"Function") || iequals(tokens[j].text, L"AddressOf")) { inLambdaDecl = true; break; }
                    if (tokens[j].text == L")" || iequals(tokens[j].text, L"End") || iequals(tokens[j].text, L"Then") || iequals(tokens[j].text, L"Else")) break;
                }
                if (inLambdaDecl && fieldMap.count(token)) { result << fieldMap[token]; continue; }
            }
            if (iequals(prevToken, L"Handles") || iequals(prevToken, L"Implements")) {
                if (nextToken == L".") {
                    if (classMap.count(token) && localClasses.count(token)) result << classMap[token];
                    else if (fieldMap.count(token) && localFields.count(token)) result << fieldMap[token];
                    else result << token;
                }
                else {
                    if (methodMap.count(token) && localMethods.count(token)) result << methodMap[token];
                    else if (fieldMap.count(token) && localFields.count(token)) result << fieldMap[token];
                    else result << token;
                }
                continue;
            }
            if (iequals(prevToken, L"AddressOf")) {
                if (methodMap.count(token) && isLocalMethod) result << methodMap[token];
                else result << token;
                continue;
            }
            if (iequals(prevToken, L"GoTo") || iequals(prevToken, L"Resume") || nextToken == L":") {
                result << token; continue;
            }
            if (iequals(prevToken, L"GetType") || iequals(prevToken, L"TypeOf")) {
                if (isLocalField && fieldMap.count(token)) { result << fieldMap[token]; continue; }
                if (!isLocalClass) { result << token; continue; }
            }
            if (classMap.count(token)) result << classMap[token];
            else if (methodMap.count(token)) result << methodMap[token];
            else if (fieldMap.count(token)) result << fieldMap[token];
            else result << token;
        }
        return result.str();
    }

    // FIX (gap 2): is a "[" token here opening a C# attribute application
    // ("[Obsolete]", "[JsonPropertyName(...)]"), or a subscript/indexer
    // ("arr[i]", "this[0]", a chained "arr[0][1]")? Attributes are only ever
    // opened where an expression could NOT precede them -- right after "(",
    // ",", ";", "{", "}", or at the very start of the line/file -- whereas an
    // indexer always follows the identifier/expression/closing bracket being
    // indexed. This is a structural test, not a name lookup, so it fires for
    // ANY attribute regardless of which one -- the general mechanism the
    // hardcoded kAttrStringArg map below never was.
    static bool isAttributeBracketOpen(const wstring& prevToken) {
        if (prevToken.empty()) return true;
        if (prevToken == L"(" || prevToken == L"," || prevToken == L";" ||
            prevToken == L"{" || prevToken == L"}") return true;
        return false;
    }

    wstring processLineCLike(const wstring& line) {
        vector<Token> tokens = tokenizeLineCLike(line);
        wstringstream result;
        // Depth of "[...]" brackets currently open that are attribute applications
        // (as opposed to indexers) -- a string literal is only ever a candidate for
        // the rewrite below while this is > 0. bracketIsAttr remembers, per currently
        // -open "[", whether THAT bracket was an attribute, so a "]" closing an
        // ordinary indexer nested inside an attribute's argument list (e.g.
        // "[Attr(arr[0])]") doesn't miscount.
        // These are engine members, not line locals, so a multiline attribute
        // application retains its bracket state across processLineCLike calls.
        for (size_t i = 0; i < tokens.size(); i++) {
            const Token& tok = tokens[i];
            if (!tok.isWord) {
                // FIX (gap 2): general mechanism replacing the old hardcoded
                // attribute/method-name -> argument-slot-index map. ANY quoted-string
                // argument inside ANY attribute application -- not just a fixed list
                // of known attribute/reflection-method names, and not just one
                // pre-decided argument position -- is a candidate: it is rewritten if
                // and only if its exact original content currently matches a name in
                // fieldMap/methodMap/classMap. This only follows a rename that
                // already happened to the declaration; it protects nothing itself,
                // so it stays separate from the declaration-protection prescan.
                if (tok.isStringLiteral && csharpAttributeBracketDepth > 0) {
                    bool quoted = tok.text.size() >= 2 && tok.text.front() == L'"' && tok.text.back() == L'"';
                    wstring inner = quoted ? tok.text.substr(1, tok.text.size() - 2) : tok.text;
                    wstring real = UnescapeCFamilyString(inner);
                    wstring obfName;
                    {
                        auto q = classMap.find(real); if (q != classMap.end()) obfName = q->second;
                        if (obfName.empty()) { auto q2 = methodMap.find(real); if (q2 != methodMap.end()) obfName = q2->second; }
                        if (obfName.empty()) { auto q3 = fieldMap.find(real); if (q3 != fieldMap.end()) obfName = q3->second; }
                    }
                    if (!obfName.empty() && obfName != real) { result << L'"' << obfName << L'"'; continue; }
                }
                if (tok.text == L"[") {
                    int pj = findPrevMeaningfulIndex(tokens, i);
                    bool isAttr = isAttributeBracketOpen(pj >= 0 ? tokens[pj].text : L"");
                    csharpBracketIsAttribute.push_back(isAttr);
                    if (isAttr) csharpAttributeBracketDepth++;
                }
                else if (tok.text == L"]") {
                    if (!csharpBracketIsAttribute.empty()) {
                        if (csharpBracketIsAttribute.back() && csharpAttributeBracketDepth > 0) csharpAttributeBracketDepth--;
                        csharpBracketIsAttribute.pop_back();
                    }
                }
                else if (tok.text == L"(") {
                    int pj = findPrevMeaningfulIndex(tokens, i);
                    wstring target = pj >= 0 ? tokens[pj].text : L"";
                    callTargetStack.push_back(target);
                    wstring resolvedType;
                    if (localTypeNames.count(target) || localClasses.count(target)) resolvedType = target;
                    int dotIdx = pj >= 0 ? findPrevMeaningfulIndex(tokens, (size_t)pj) : -1;
                    if (resolvedType.empty() && dotIdx >= 0 && tokens[dotIdx].text == L".") {
                        int recvIdx = findPrevMeaningfulIndex(tokens, (size_t)dotIdx);
                        if (recvIdx >= 0) {
                            wstring recv = tokens[recvIdx].text;
                            const wstring* vt = findVarType(recv);
                            if (vt) {
                                resolvedType = *vt;
                                size_t q = resolvedType.find_last_of(L".");
                                if (q != wstring::npos) resolvedType = resolvedType.substr(q + 1);
                                size_t g = resolvedType.find(L'<');
                                if (g != wstring::npos) resolvedType = resolvedType.substr(0, g);
                            }
                            else if (localTypeNames.count(recv) || localClasses.count(recv)) resolvedType = recv;
                        }
                    }
                    if (resolvedType.empty() && !csharpClassScopeStack.empty()) {
                        for (auto rit = csharpClassScopeStack.rbegin(); rit != csharpClassScopeStack.rend(); ++rit)
                            if (!rit->empty()) { resolvedType = *rit; break; }
                    }
                    callTargetTypeStack.push_back(resolvedType);
                }
                else if (tok.text == L")") {
                    if (!callTargetStack.empty()) callTargetStack.pop_back();
                    if (!callTargetTypeStack.empty()) callTargetTypeStack.pop_back();
                }
                else if (tok.text == L"{") {
                    // Track lexical class scope and type-qualified initializer/pattern braces.
                    wstring className, targetType;
                    int p1 = findPrevMeaningfulIndex(tokens, i);
                    int p2 = p1 >= 0 ? findPrevMeaningfulIndex(tokens, (size_t)p1) : -1;
                    int p3 = p2 >= 0 ? findPrevMeaningfulIndex(tokens, (size_t)p2) : -1;
                    if (p2 >= 0 && (iequals(tokens[p2].text, L"class") || iequals(tokens[p2].text, L"struct") ||
                        iequals(tokens[p2].text, L"interface") || iequals(tokens[p2].text, L"record")))
                        className = tokens[p1].text;
                    csharpClassScopeStack.push_back(className);
                    if (p1 >= 0 && (localTypeNames.count(tokens[p1].text) || localClasses.count(tokens[p1].text))) {
                        bool isNew = p2 >= 0 && iequals(tokens[p2].text, L"new");
                        bool isPattern = p2 >= 0 && (iequals(tokens[p2].text, L"is") || iequals(tokens[p2].text, L"case"));
                        if (isNew || isPattern) targetType = tokens[p1].text;
                    }
                    if (targetType.empty() && !pendingCSharpBraceTargetType.empty()) targetType = pendingCSharpBraceTargetType;
                    pendingCSharpBraceTargetType.clear();
                    if (targetType.empty() && p1 >= 0 && tokens[p1].text == L")") {
                        int d = 0, openIdx = -1;
                        for (int z = p1; z >= 0; --z) {
                            if (tokens[z].text == L")") ++d;
                            else if (tokens[z].text == L"(" && --d == 0) { openIdx = z; break; }
                        }
                        if (openIdx > 0) {
                            int ty = findPrevMeaningfulIndex(tokens, (size_t)openIdx);
                            int nw = ty >= 0 ? findPrevMeaningfulIndex(tokens, (size_t)ty) : -1;
                            if (ty >= 0 && nw >= 0 && iequals(tokens[nw].text, L"new") && (localTypeNames.count(tokens[ty].text) || localClasses.count(tokens[ty].text)))
                                targetType = tokens[ty].text;
                        }
                    }
                    csharpBraceTargetTypes.push_back(targetType);
                }
                else if (tok.text == L"}") {
                    if (!csharpBraceTargetTypes.empty()) csharpBraceTargetTypes.pop_back();
                    if (!csharpClassScopeStack.empty()) csharpClassScopeStack.pop_back();
                }
                result << tok.text;
                continue;
            }
            wstring token = tok.text;
            wstring prevToken = findPrevMeaningful(tokens, i);
            wstring nextToken = findNextMeaningful(tokens, i);

            if (lang == Lang::CSharp && (localTypeNames.count(token) || localClasses.count(token)) &&
                (iequals(prevToken, L"new") || iequals(prevToken, L"is") || iequals(prevToken, L"case")))
                pendingCSharpBraceTargetType = token;

            {
                size_t macroEnd = i;
                if (tryRewriteMacroArgs(tokens, i, result, macroEnd)) { i = macroEnd; continue; }
            }

            if (token == L"__declspec") {
                size_t dj = i + 1;
                while (dj < tokens.size() && !tokens[dj].isWord &&
                    !tokens[dj].text.empty() && iswspace(tokens[dj].text[0])) dj++;
                if (dj < tokens.size() && tokens[dj].text == L"(") {
                    size_t dp1 = dj + 1;
                    while (dp1 < tokens.size() && !tokens[dp1].isWord &&
                        !tokens[dp1].text.empty() && iswspace(tokens[dp1].text[0])) dp1++;
                    if (dp1 < tokens.size() && tokens[dp1].isWord && tokens[dp1].text == L"property") {
                        size_t dp2 = dp1 + 1;
                        while (dp2 < tokens.size() && !tokens[dp2].isWord &&
                            !tokens[dp2].text.empty() && iswspace(tokens[dp2].text[0])) dp2++;
                        if (dp2 < tokens.size() && tokens[dp2].text == L"(") {
                            for (size_t m = i; m <= dp2; m++) result << tokens[m].text;
                            int depth = 1;
                            size_t dk = dp2 + 1;
                            for (; dk < tokens.size(); dk++) {
                                const Token& t2 = tokens[dk];
                                if (t2.isWord) {
                                    if (t2.text == L"get" || t2.text == L"put") {
                                        result << t2.text;
                                    }
                                    else {
                                        if (methodMap.count(t2.text) && localMethods.count(t2.text)) result << methodMap[t2.text];
                                        else if (fieldMap.count(t2.text) && localFields.count(t2.text)) result << fieldMap[t2.text];
                                        else if (classMap.count(t2.text) && localClasses.count(t2.text)) result << classMap[t2.text];
                                        else result << t2.text;
                                    }
                                    continue;
                                }
                                if (!t2.isStringLiteral && !t2.isComment) {
                                    for (wchar_t c : t2.text) {
                                        if (c == L'(') depth++;
                                        else if (c == L')') depth--;
                                    }
                                }
                                result << t2.text;
                                if (depth == 0) break;
                            }
                            i = (dk < tokens.size()) ? dk : tokens.size() - 1;
                            continue;
                        }
                    }
                }
            }

            if (CPP_PRAGMA_INTRODUCER_TOKENS.count(token)) {
                size_t j = i + 1;
                while (j < tokens.size() && !tokens[j].isWord &&
                    !tokens[j].text.empty() && iswspace(tokens[j].text[0])) {
                    j++;
                }
                if (j < tokens.size() && tokens[j].text == L"(") {
                    int depth = 0;
                    size_t k = j;
                    result << token;
                    for (; k < tokens.size(); k++) {
                        const Token& t2 = tokens[k];
                        if (t2.isWord && depth >= 1) {
                            if (CPP_ATTRIBUTE_LITERAL_WORDS.count(t2.text)) result << t2.text;
                            else result << renameIdentIfLocal(t2.text);
                            continue;
                        }
                        if (!t2.isStringLiteral && !t2.isComment) {
                            for (wchar_t c : t2.text) {
                                if (c == L'(') depth++;
                                else if (c == L')') depth--;
                            }
                        }
                        result << t2.text;
                        if (depth == 0 && k > j) break;
                    }
                    i = (k < tokens.size()) ? k : tokens.size() - 1;
                    continue;
                }
                result << token; continue;
            }

            if (token.size() > 1 && token[0] == L'_') {
                bool compilerReserved = (token[1] == L'_');
                if (!compilerReserved) {
                    for (const auto& p : CPP_UNDERSCORE_PREFIXES) {
                        if (token.compare(0, p.size(), p) == 0) { compilerReserved = true; break; }
                    }
                }
                if (compilerReserved) { result << token; continue; }
            }

            if (iequals(prevToken, L"operator")) {
                // FIX: symbolic operators (operator+, operator==, ...) are
                // punctuation, not word tokens, so this branch is ONLY ever
                // reached for a WORD right after "operator" -- which means a
                // user-defined conversion operator's target type, e.g.
                // "public static explicit operator Foo(Bar b)". Unconditionally
                // keeping the original text here (as before) left that "Foo"
                // un-renamed even after the class Foo was renamed everywhere
                // else, producing a dangling reference. Built-in type keywords
                // (bool, int, string, ...) simply aren't in classMap, so this
                // still falls through to the plain "result << token" below for
                // those, same as before.
                if (classMap.count(token) && localClasses.count(token)) { result << classMap[token]; continue; }
                result << token; continue;
            }
            if (iequals(prevToken, L"goto")) { result << token; continue; }
            if (preservedMacroParams.count(token) ||
                preservedLabels.count(token) ||
                preservedOperators.count(token) ||
                preservedNamespaces.count(token) ||
                preservedTypedefs.count(token) ||
                preservedTemplateParams.count(token) ||
                preservedAttributes.count(token) ||
                preservedConcepts.count(token) ||
                preservedQtSignalsSlots.count(token)) {
                result << token; continue;
            }
            if (nextToken == L":" && preservedLabels.count(token)) { result << token; continue; }
            if (iequals(prevToken, L"namespace") || iequals(prevToken, L"concept") ||
                iequals(prevToken, L"typename")) {
                result << token; continue;
            }

            // Attribute named arguments use '=' rather than ':' and refer to a
            // writable member on the locally-declared attribute type. The open
            // attribute invocation is represented by callTargetStack.
            if (lang == Lang::CSharp && csharpAttributeBracketDepth > 0 && nextToken == L"=" &&
                (prevToken == L"(" || prevToken == L",")) {
                wstring attrType = callTargetStack.empty() ? L"" : callTargetStack.back();
                auto hasAttrMember = [&](const wstring& t) -> bool {
                    auto ci = classMembersByType.find(t);
                    return ci != classMembersByType.end() && ci->second.count(token) != 0;
                    };
                bool resolved = hasAttrMember(attrType) ||
                    (attrType.size() > 9 && attrType.substr(attrType.size() - 9) == L"Attribute" && hasAttrMember(attrType.substr(0, attrType.size() - 9))) ||
                    hasAttrMember(attrType + L"Attribute");
                if (resolved && fieldMap.count(token) && localFields.count(token)) { result << fieldMap[token]; continue; }
                result << token; continue;
            }

            // Object initializers and property patterns are resolved against the
            // explicit local type attached to the currently-open brace context.
            if (lang == Lang::CSharp && !csharpBraceTargetTypes.empty() &&
                !csharpBraceTargetTypes.back().empty() && (nextToken == L"=" || nextToken == L":")) {
                const wstring& targetType = csharpBraceTargetTypes.back();
                auto ci = classMembersByType.find(targetType);
                if (ci != classMembersByType.end() && ci->second.count(token) &&
                    fieldMap.count(token) && localFields.count(token)) {
                    result << fieldMap[token]; continue;
                }
            }

            if (lang == Lang::CSharp && nextToken == L":" &&
                (prevToken == L"(" || prevToken == L"," || prevToken == L"[")) {
                wstring callTarget = callTargetStack.empty() ? L"" : callTargetStack.back();
                wstring targetType = callTargetTypeStack.empty() ? L"" : callTargetTypeStack.back();
                bool calleeIsLocal = methodMap.count(callTarget) || classMap.count(callTarget);
                if (!calleeIsLocal) { result << token; continue; }
                // FIX (b): require the label to be a member of THIS SPECIFIC target's
                // own methodParams entry, the same way VB's "bar:=1" check (above) uses
                // methodParams[callTarget] -- not merely "is this name a parameter of
                // ANY method/attribute in the project", which is what the old
                // fieldMap/localFields-only check amounted to (two unrelated methods,
                // or a method and an unrelated attribute, can easily share a common
                // parameter name). fieldMap/localFields are still checked too (FIX
                // (issue 1) above, kept as-is) so a stale fieldMap entry excluded from
                // renaming elsewhere still can't be used here.
                bool isOwnParam = false;
                if (!targetType.empty()) {
                    auto ownerIt = methodParamsByType.find(targetType);
                    if (ownerIt != methodParamsByType.end()) {
                        auto methodIt = ownerIt->second.find(callTarget);
                        isOwnParam = methodIt != ownerIt->second.end() && methodIt->second.count(token);
                    }
                }
                else {
                    // A bare call can use the name-only registry only when the
                    // declaration name is unambiguous across the scanned types.
                    auto owners = methodOwnerTypes.find(callTarget);
                    if (owners == methodOwnerTypes.end() || owners->second.size() <= 1) {
                        auto pit = methodParams.find(callTarget);
                        isOwnParam = pit != methodParams.end() && pit->second.count(token);
                    }
                }
                if (isOwnParam && fieldMap.count(token) && localFields.count(token)) { result << fieldMap[token]; continue; }
                result << token; continue;
            }

            if (prevToken == L"." || prevToken == L"->" || prevToken == L"::") {
                // A framework-name collision must not suppress a declaration that
                // is positively known to be a project member (for example a local
                // property named Text). Unknown/external receivers are still blocked
                // below, so this exception cannot turn a BCL access into a rename.
                bool tokenIsDeclaredLocal =
                    (fieldMap.count(token) && localFields.count(token)) ||
                    (methodMap.count(token) && localMethods.count(token)) ||
                    (classMap.count(token) && localClasses.count(token));
                if (isFrameworkMember(token) && !tokenIsDeclaredLocal) {
                    result << token;
                    continue;
                }

                if (prevToken == L"::") {
                    if (methodMap.count(token) && localMethods.count(token)) { result << methodMap[token]; continue; }
                    if (classMap.count(token) && localClasses.count(token)) { result << classMap[token]; continue; }
                    if (fieldMap.count(token) && localFields.count(token)) { result << fieldMap[token]; continue; }
                    result << token; continue;
                }

                // FIX (A0.2): use the generic-argument-aware lookahead here instead of
                // the raw nextToken, so "Obj.Method<T>(...)" is recognized as a call the
                // same way "Obj.Method(...)" already is.
                if (findNextMeaningfulSkippingGenericArgs(tokens, i) == L"(") {
                    if (methodMap.count(token) && localMethods.count(token)) { result << methodMap[token]; continue; }
                    if (classMap.count(token) && localClasses.count(token)) { result << classMap[token]; continue; }
                    result << token; continue;
                }

                // The earlier collision guard already preserves framework members
                // unless this exact spelling is a positively identified project
                // declaration. Do not re-block a local field/property here: doing so
                // left declarations renamed while accesses such as obj.Total or
                // obj.Snapshot retained the old name.
                int dotIdx = findPrevMeaningfulIndex(tokens, i);
                int objIdx = dotIdx >= 0 ? findPrevMeaningfulIndex(tokens, (size_t)dotIdx) : -1;
                wstring objToken = objIdx >= 0 ? tokens[objIdx].text : L"";

                // C#'s "base" always names a member of the inherited framework base
                // class here -- it can never resolve to a locally-renamed field or
                // method the way "this"/"Me"/"MyBase"/"MyClass" can. Short-circuit it
                // unconditionally rather than relying on isFrameworkMember()/fieldMap
                // to have classified the member correctly first: if Layer 1 missed it
                // (e.g. InvokeRequired isn't in COMMON_DOTNET_FRAMEWORK_MEMBERS and
                // isn't in net_assemblies.txt either), the fallback fieldMap branch
                // below would otherwise still rename it. Note this must NOT be folded
                // into receiverIsThis -- that flag is a rename *trigger*, so adding
                // "base" to it would make base.X a rename target instead of blocking it.
                if (iequals(objToken, L"base")) { result << token; continue; }

                bool receiverIsThis =
                    iequals(objToken, L"this") ||
                    iequals(objToken, L"Me") ||
                    iequals(objToken, L"MyBase") ||
                    iequals(objToken, L"MyClass");

                // FIX (A0.5): a static-class-qualified reference with no call paren at
                // all -- nameof(WpfWindowTracker.TryGetWindow), AddressOf
                // WpfWindowTracker.TryGetWindow, or a plain static field read like
                // WpfWindowTracker.SomeField -- has a receiver that is neither
                // this/Me/MyBase/MyClass nor a variable resolvable via varToType (the
                // qualifier IS the class name itself, not an instance of it), so
                // neither receiverIsThis nor receiverIsLocalClassTyped below ever
                // caught it and the member stayed un-renamed even though its
                // declaration was. If the qualifier is literally one of our own
                // renamed classes, that is just as reliable a signal that the member
                // after the dot is ours too.
                bool receiverIsLocalClassName = localTypeNames.count(objToken) != 0 ||
                    localClasses.count(objToken) != 0;
                // A project class may be declared in another source unit and its
                // static member access may be the only use in this file. The
                // type-scoped declaration registry is stronger evidence than a
                // flat same-spelled fieldMap entry: Data.Member is eligible only
                // when Member was actually recorded as a member of Data.
                bool receiverOwnsDeclaredMember = false;
                {
                    auto ownerIt = classMembersByType.find(objToken);
                    receiverOwnsDeclaredMember = ownerIt != classMembersByType.end() &&
                        ownerIt->second.count(token) != 0;
                }

                bool receiverIsLocalClassTyped = false;
                bool receiverHasExternalType = false;
                {
                    const wstring* vt = findVarType(objToken);
                    if (vt) {
                        // Reduce a declared type down to the single identifier that actually
                        // names the class/struct, regardless of how it was spelled at the
                        // declaration site: strip generic args ("List<Client>" -> "List",
                        // and for the element type see bareElementType below), array/pointer/
                        // nullable markers ("Client[]", "Client*", "Client?" -> "Client"), and
                        // take the LAST segment after a qualifier -- "::" for C++, but also "."
                        // for C# ("MyApp.Models.Client" -> "Client"), which the old code never
                        // checked at all (C# doesn't use "::" for namespaces).
                        auto bareOf = [](wstring t) -> wstring {
                            size_t lt = t.find(L'<');
                            if (lt != wstring::npos) t = t.substr(0, lt);
                            while (!t.empty() && (t.back() == L'*' || t.back() == L'&' ||
                                t.back() == L'?' || t.back() == L']' || t.back() == L'[' ||
                                iswspace(t.back()))) t.pop_back();
                            size_t lastSep = t.find_last_of(L".");
                            size_t lastScope = t.rfind(L"::");
                            size_t cut = wstring::npos;
                            if (lastScope != wstring::npos) cut = lastScope + 2;
                            else if (lastSep != wstring::npos) cut = lastSep + 1;
                            if (cut != wstring::npos && cut <= t.size()) t = t.substr(cut);
                            return t;
                            };
                        wstring bare = bareOf(*vt);

                        // A receiver's declared project type can be absent from this file's
                        // localTypeNames set (for example, when the type declaration is in a
                        // companion source file or was intentionally preserved). The
                        // type-scoped member index is independent evidence: if this exact
                        // member was declared on the resolved receiver type, bind the access
                        // to that declaration rather than treating it as an unknown receiver.
                        // This fixes cross-class accesses such as editor.Title / editor.Client
                        // when those declarations have already been renamed.
                        {
                            auto ownerIt = classMembersByType.find(bare);
                            if (ownerIt != classMembersByType.end() && ownerIt->second.count(token)) {
                                receiverOwnsDeclaredMember = true;
                                receiverIsLocalClassTyped = true;
                            }
                        }

                        // For a generic container ("List<Client>") the receiver itself isn't a
                        // Client, so bare above resolved to "List" -- that's deliberately not
                        // treated as local-class-typed (a method call on the list itself, e.g.
                        // list.Add, is a different matter), but it also shouldn't be flagged as
                        // a definitely-external receiver purely because "List" is a known BCL
                        // name: that classification is about THIS token's receiver object, and
                        // for a generic container accessed as obj.Field directly (not obj[i].Field)
                        // the field genuinely belongs to the container type itself, so fall
                        // through to the same known-external check below using "bare".
                        // A project type may be intentionally preserved by an attribute
                        // (for example [StructLayout] on an interop struct). It remains
                        // a local type for member binding even though it has no classMap entry.
                        if (localTypeNames.count(bare) || localClasses.count(bare)) {
                            receiverIsLocalClassTyped = true;
                        }
                        else if (DOTNET_TYPES.count(bare) || CPP_STD_TYPES.count(bare) ||
                            g_generatedNetSymbols.count(bare)) {
                            // Only block the rename when the receiver's type is *affirmatively*
                            // recognized as a real external/BCL/library type. An unrecognized
                            // type name (e.g. a local class the prescan missed for some reason,
                            // a type from an unscanned third-party DLL, or a qualifier we still
                            // didn't normalize away) is treated as unknown rather than external
                            // -- Layer 1 (isFrameworkMember, checked above before we ever reach
                            // this point) already filters out real framework/BCL member names by
                            // the token's own name, so defaulting "unknown receiver type" to
                            // "don't block" is the safer choice now that that list is populated
                            // from GetMembers() across the whole scanned .NET surface, not just
                            // a short hand-maintained set.
                            receiverHasExternalType = true;
                        }
                    }
                }

                // Unknown receiver types are NOT evidence that a member belongs
                // to this project. The previous fall-through allowed an unrelated
                // local field with the same spelling to rewrite external calls such
                // as unknownControl.InvokeRequired or service.Name. Only rewrite a
                // dotted member when the receiver is positively identified as this,
                // a local class instance, or a local class name (static access).
                // Known external types are included in the same preserve path.
                bool receiverIsKnownLocal =
                    receiverIsThis || receiverIsLocalClassTyped || receiverIsLocalClassName ||
                    receiverOwnsDeclaredMember;
                if (!receiverIsKnownLocal || receiverHasExternalType) {
                    // Record a project symbol whose declaration was renamed but
                    // whose dotted use could not be proven to belong to that type.
                    // This is the important case for ignored.txt: Snapshot may be
                    // in fieldMap while obj.Snapshot was conservatively left alone.
                    if ((fieldMap.count(token) && localFields.count(token)) ||
                        (methodMap.count(token) && localMethods.count(token)) ||
                        (classMap.count(token) && localClasses.count(token)))
                        ignoredSymbols.insert(token);
                    result << token;
                    continue;
                }

                bool tokenIsLocalClass = localClasses.count(token) != 0;

                if (fieldMap.count(token) && localFields.count(token)) { result << fieldMap[token]; continue; }
                if (methodMap.count(token) && localMethods.count(token)) { result << methodMap[token]; continue; }

                if (tokenIsLocalClass && classMap.count(token)) { result << classMap[token]; continue; }

                result << token; continue;
            }

            // FIX (A0.2): same generic-argument-aware lookahead as the dot-qualified
            // branch above, for a bare call to a method in the same class/file,
            // "Method<T>(...)".
            if (findNextMeaningfulSkippingGenericArgs(tokens, i) == L"(") {
                if (methodMap.count(token) && localMethods.count(token)) { result << methodMap[token]; continue; }
                if (classMap.count(token) && localClasses.count(token)) { result << classMap[token]; continue; }
                if (fieldMap.count(token) && localFields.count(token)) { result << fieldMap[token]; continue; }
                result << token; continue;
            }

            if (classMap.count(token) && localClasses.count(token)) result << classMap[token];
            else if (methodMap.count(token) && localMethods.count(token)) result << methodMap[token];
            else if (fieldMap.count(token) && localFields.count(token)) result << fieldMap[token];
            else result << token;
        }
        return result.str();
    }
};

// ============================================================================
// v3.3: free helper -- inject an include line near the top of a C++ file
// ============================================================================
static wstring injectStandaloneInclude(const wstring& code, const wstring& includeLine) {
    if (includeLine.empty()) return code;
    wstringstream ss(code);
    wstringstream out;
    wstring ln;
    bool inserted = false;
    bool sawFirstNonWs = false;
    while (getline(ss, ln)) {
        if (!inserted) {
            wstring t = trimW(ln);
            if (!t.empty() && t[0] == L'\uFEFF') t.erase(0, 1);
            bool isIncludeLike =
                t.rfind(L"#include", 0) == 0 ||
                t.rfind(L"#pragma once", 0) == 0 ||
                t.rfind(L"#pragma comment", 0) == 0;
            bool isBlankOrComment =
                t.empty() ||
                (t.size() >= 2 && t[0] == L'/' && (t[1] == L'/' || t[1] == L'*'));
            if (!isIncludeLike && !isBlankOrComment) {
                out << includeLine << L"\n";
                inserted = true;
            }
            (void)sawFirstNonWs;
        }
        out << ln << L"\n";
    }
    if (!inserted) out << includeLine << L"\n";
    return out.str();
}


// ============================================================================
// FILESYSTEM HELPERS
// ============================================================================
static wstring getExeDir() {
    wchar_t buf[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return L"";
    wstring p(buf, len);
    size_t slash = p.find_last_of(L"\\/");
    return (slash == wstring::npos) ? L"" : p.substr(0, slash + 1);
}

static bool fileExistsW(const wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// Turns a Win32 error code into readable text, e.g. "The system cannot find the path specified".
static wstring Win32ErrorText(DWORD err) {
    LPWSTR buf = nullptr;
    DWORD len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&buf, 0, nullptr);
    wstring msg;
    if (len && buf) {
        msg.assign(buf, len);
        while (!msg.empty() && (msg.back() == L'\r' || msg.back() == L'\n' ||
            msg.back() == L' ' || msg.back() == L'.'))
            msg.pop_back();
    }
    if (buf) LocalFree(buf);
    if (msg.empty()) msg = L"error code " + to_wstring(err);
    return msg;
}

// Creates `path`, including any missing intermediate folders (CreateDirectoryW
// only ever creates ONE level at a time, so a nested path like "bak\sub\sub2"
// fails with ERROR_PATH_NOT_FOUND if "bak\sub" doesn't already exist).
// On failure, if outErr is given, it receives the Win32 error code so the
// caller can report WHY it failed, not just that it failed.
static bool ensureDir(const wstring& path, DWORD* outErr = nullptr) {
    DWORD a = GetFileAttributesW(path.c_str());
    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) return true;

    wstring norm = path;
    for (auto& c : norm) if (c == L'/') c = L'\\';
    while (norm.size() > 1 && norm.back() == L'\\') norm.pop_back();

    size_t pos = 0;
    if (norm.size() >= 2 && norm[1] == L':') {
        pos = 2; // "C:" drive prefix -- don't try to "create" the drive root
    }
    else if (norm.size() >= 2 && norm[0] == L'\\' && norm[1] == L'\\') {
        // UNC path "\\server\share\..." -- skip past server and share names
        size_t p1 = norm.find(L'\\', 2);
        size_t p2 = (p1 == wstring::npos) ? wstring::npos : norm.find(L'\\', p1 + 1);
        pos = (p2 == wstring::npos) ? norm.size() : p2;
    }

    while (pos < norm.size()) {
        size_t next = norm.find(L'\\', pos + 1);
        wstring sub = (next == wstring::npos) ? norm : norm.substr(0, next);
        DWORD sa = GetFileAttributesW(sub.c_str());
        if (sa == INVALID_FILE_ATTRIBUTES) {
            if (!CreateDirectoryW(sub.c_str(), nullptr)) {
                DWORD err = GetLastError();
                if (err != ERROR_ALREADY_EXISTS) {
                    if (outErr) *outErr = err;
                    return false;
                }
            }
        }
        else if (!(sa & FILE_ATTRIBUTE_DIRECTORY)) {
            if (outErr) *outErr = ERROR_DIRECTORY; // a file already sits where a folder is needed
            return false;
        }
        if (next == wstring::npos) break;
        pos = next;
    }

    DWORD finalAttr = GetFileAttributesW(norm.c_str());
    bool ok = finalAttr != INVALID_FILE_ATTRIBUTES && (finalAttr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (!ok && outErr) *outErr = GetLastError();
    return ok;
}

static bool dirExistsW(const wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static wstring parentDirOf(const wstring& filePath) {
    size_t s = filePath.find_last_of(L"\\/");
    return (s == wstring::npos) ? L".\\" : filePath.substr(0, s + 1);
}

static wstring baseNameOf(const wstring& filePath) {
    size_t s = filePath.find_last_of(L"\\/");
    return (s == wstring::npos) ? filePath : filePath.substr(s + 1);
}

static bool endsWithW(const wstring& s, const wstring& suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

// v3.4: build an #include line that references the decryptor host file from a TU.
static wstring MakeIncludeLineForFile(const wstring& tuPath, const wstring& headerPath) {
    wstring tuDir = parentDirOf(tuPath);
    wstring hDir = parentDirOf(headerPath);
    wstring ref;
    if (iequals(tuDir, hDir)) ref = baseNameOf(headerPath);
    else {
        ref = headerPath;
        for (auto& c : ref) if (c == L'\\') c = L'/';
    }
    return L"#include \"" + ref + L"\"";
}

// ============================================================================
// §2: LoadedProject + three loaders
// ============================================================================
struct LoadedProject {
    vector<wstring> files;
    Lang            lang;
    wstring         rootDir;
    wstring         displayName;
    // Part B: the actual .csproj/.vbproj file(s) backing this LoadedProject,
    // if any -- empty for Folder mode (nothing to read PackageReferences
    // from), one entry for Project mode, one per sub-project for Solution
    // mode. `files` above holds source files to obfuscate; this holds the
    // project file(s) to read dependency metadata from -- a different thing.
    vector<wstring> projectFiles;
};

static vector<wstring> ExpandGlob(const wstring& dir, const wstring& pattern) {
    vector<wstring> out;
    vector<wstring> parts;
    wstring cur;
    for (wchar_t c : pattern) {
        if (c == L'\\' || c == L'/') { if (!cur.empty()) parts.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    if (parts.empty()) return out;
    function<void(size_t, wstring)> rec = [&](size_t idx, wstring currentDir) {
        if (idx >= parts.size()) return;
        wstring part = parts[idx];
        bool last = (idx == parts.size() - 1);
        bool hasGlob = part.find(L'*') != wstring::npos || part.find(L'?') != wstring::npos;
        if (last) {
            if (hasGlob) {
                WIN32_FIND_DATAW fd;
                HANDLE h = FindFirstFileW((currentDir + L"\\" + part).c_str(), &fd);
                if (h != INVALID_HANDLE_VALUE) {
                    do {
                        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                            out.push_back(currentDir + L"\\" + fd.cFileName);
                    } while (FindNextFileW(h, &fd));
                    FindClose(h);
                }
            }
            else {
                wstring p = currentDir + L"\\" + part;
                if (fileExistsW(p)) out.push_back(p);
            }
        }
        else if (part == L"**") {
            rec(idx + 1, currentDir);
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW((currentDir + L"\\*").c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    if (fd.cFileName[0] == L'.') continue;
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        rec(idx, currentDir + L"\\" + fd.cFileName);
                    }
                } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
        }
        else if (hasGlob) {
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW((currentDir + L"\\" + part).c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    if (fd.cFileName[0] == L'.') continue;
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        rec(idx + 1, currentDir + L"\\" + fd.cFileName);
                    }
                } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
        }
        else {
            wstring next = currentDir + L"\\" + part;
            if (dirExistsW(next)) rec(idx + 1, next);
        }
        };
    rec(0, dir);
    return out;
}

static LoadedProject LoadFromFolder(const wstring& root, Lang lang) {
    LoadedProject result;
    result.lang = lang;
    result.rootDir = root;
    while (!result.rootDir.empty() && (result.rootDir.back() == L'\\' || result.rootDir.back() == L'/'))
        result.rootDir.pop_back();
    result.displayName = baseNameOf(result.rootDir);

    static const set<wstring> skipDirs = {
        L"bin", L"obj", L"packages", L"node_modules", L".git", L".vs",
        L"testresults", L"build", L"out", L"cmakefiles"
    };

    auto shouldSkipDir = [&](const wstring& name) -> bool {
        wstring lower = toLowerW(name);
        if (skipDirs.count(lower)) return true;
        if (lower.rfind(L"build-", 0) == 0) return true;
        if (lower.rfind(L"cmake-build-", 0) == 0) return true;
        return false;
        };

    auto shouldSkipFile = [&](const wstring& name) -> bool {
        wstring lower = toLowerW(name);
        if (lower.rfind(L"moc_", 0) == 0 && endsWithW(lower, L".cpp")) return true;
        if (lower.rfind(L"ui_", 0) == 0 && endsWithW(lower, L".h")) return true;
        if (lower.rfind(L"qrc_", 0) == 0 && endsWithW(lower, L".cpp")) return true;
        if (endsWithW(lower, L"_moc.cpp")) return true;
        return false;
        };

    auto matchesExt = [&](const wstring& p) -> bool {
        wstring lower = toLowerW(p);
        if (lang == Lang::CSharp) return endsWithW(lower, L".cs");
        if (lang == Lang::VB) return endsWithW(lower, L".vb");
        return endsWithW(lower, L".cpp") || endsWithW(lower, L".cc") || endsWithW(lower, L".cxx") ||
            endsWithW(lower, L".h") || endsWithW(lower, L".hpp");
        };

    function<void(const wstring&)> walk = [&](const wstring& dir) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            wstring full = dir + L"\\" + fd.cFileName;
            wstring lowerFull = toLowerW(full);
            if (lowerFull.find(L"_obfuscated") != wstring::npos) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!shouldSkipDir(fd.cFileName)) walk(full);
            }
            else {
                if (shouldSkipFile(fd.cFileName)) continue;
                if (!matchesExt(fd.cFileName)) continue;
                result.files.push_back(full);
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        };
    walk(result.rootDir);

    set<wstring> seen;
    vector<wstring> deduped;
    for (auto& f : result.files) {
        wstring lower = toLowerW(f);
        if (seen.insert(lower).second) deduped.push_back(f);
    }
    sort(deduped.begin(), deduped.end());
    result.files = std::move(deduped);
    return result;
}

static LoadedProject LoadFromProject(const wstring& projPath, Lang lang) {
    LoadedProject result;
    result.lang = lang;
    result.rootDir = parentDirOf(projPath);
    while (!result.rootDir.empty() && (result.rootDir.back() == L'\\' || result.rootDir.back() == L'/'))
        result.rootDir.pop_back();
    result.displayName = baseNameOf(projPath);
    result.projectFiles.push_back(projPath);

    ifstream ifs(projPath.c_str(), ios::binary);
    if (!ifs) return LoadedProject();
    string u((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    ifs.close();
    if (u.size() >= 3 && (unsigned char)u[0] == 0xEF && (unsigned char)u[1] == 0xBB && (unsigned char)u[2] == 0xBF)
        u.erase(0, 3);

    {
        string cleaned;
        size_t i = 0;
        while (i < u.size()) {
            if (i + 3 < u.size() && u[i] == '<' && u[i + 1] == '!' && u[i + 2] == '-' && u[i + 3] == '-') {
                size_t end = u.find("-->", i + 4);
                if (end == string::npos) { cleaned += u.substr(i); break; }
                i = end + 3;
            }
            else {
                cleaned += u[i];
                i++;
            }
        }
        u = cleaned;
    }

    wstring wu = stringToWstring(u);

    if (wu.find(L"$(") != wstring::npos) {
        LogMessage(L"WARN: project uses MSBuild properties ($(...)); file list may be incomplete — use Folder mode if the result looks wrong.", CLR_WARN);
    }

    bool sdkStyle = false;
    try {
        wregex sdkRe(LR"(<Project\b[^>]*\bSdk\s*=)");
        sdkStyle = regex_search(wu, sdkRe);
    }
    catch (...) {}

    bool disableDefault = false;
    try {
        wregex edciRe(LR"(<EnableDefaultCompileItems>\s*false\s*</EnableDefaultCompileItems>)", regex::icase);
        disableDefault = regex_search(wu, edciRe);
    }
    catch (...) {}

    set<wstring> fileSetLower;
    auto addFile = [&](const wstring& p) {
        wstring lower = toLowerW(p);
        if (fileSetLower.insert(lower).second) result.files.push_back(p);
        };

    auto resolveOne = [&](const wstring& piece) {
        wstring trimmed = trimW(piece);
        if (trimmed.empty()) return;
        if (trimmed.find(L"$(") != wstring::npos) return;
        bool hasGlob = trimmed.find(L'*') != wstring::npos || trimmed.find(L'?') != wstring::npos;
        if (hasGlob) {
            for (auto& f : ExpandGlob(result.rootDir, trimmed)) addFile(f);
        }
        else {
            wstring full = trimmed;
            for (auto& c : full) if (c == L'/') c = L'\\';
            if (!(full.size() > 1 && full[1] == L':')) full = result.rootDir + L"\\" + full;
            if (fileExistsW(full)) addFile(full);
        }
        };

    if (sdkStyle && !disableDefault) {
        LoadedProject folder = LoadFromFolder(result.rootDir, lang);

        set<wstring> removeLower;
        try {
            wregex removeRe(LR"re(<(?:Compile|ClCompile|ClInclude)\s+Remove\s*=\s*"([^"]+)")re");
            for (wsregex_iterator it(wu.begin(), wu.end(), removeRe), end; it != end; ++it) {
                wstring piece = (*it)[1].str();
                size_t st = 0;
                for (size_t i = 0; i <= piece.size(); i++) {
                    if (i != piece.size() && piece[i] != L';') continue;
                    wstring item = trimW(piece.substr(st, i - st));
                    st = i + 1;
                    if (item.empty()) continue;
                    bool hasGlob = item.find(L'*') != wstring::npos || item.find(L'?') != wstring::npos;
                    if (hasGlob) {
                        for (auto& f : ExpandGlob(result.rootDir, item)) removeLower.insert(toLowerW(f));
                    }
                    else {
                        wstring full = item;
                        for (auto& c : full) if (c == L'/') c = L'\\';
                        if (!(full.size() > 1 && full[1] == L':')) full = result.rootDir + L"\\" + full;
                        removeLower.insert(toLowerW(full));
                    }
                }
            }
            wregex defaultExclRe(LR"(<DefaultItemExcludes[^>]*>([^<]+)</DefaultItemExcludes>)");
            for (wsregex_iterator it(wu.begin(), wu.end(), defaultExclRe), end; it != end; ++it) {
                wstring ex = (*it)[1].str();
                size_t st = 0;
                for (size_t i = 0; i <= ex.size(); i++) {
                    if (i != ex.size() && ex[i] != L';') continue;
                    wstring item = trimW(ex.substr(st, i - st));
                    st = i + 1;
                    if (item.empty()) continue;
                    bool hasGlob = item.find(L'*') != wstring::npos || item.find(L'?') != wstring::npos;
                    if (hasGlob) {
                        for (auto& f : ExpandGlob(result.rootDir, item)) removeLower.insert(toLowerW(f));
                    }
                    else {
                        wstring full = item;
                        for (auto& c : full) if (c == L'/') c = L'\\';
                        if (!(full.size() > 1 && full[1] == L':')) full = result.rootDir + L"\\" + full;
                        removeLower.insert(toLowerW(full));
                    }
                }
            }
        }
        catch (...) {}

        for (auto& f : folder.files) {
            if (removeLower.count(toLowerW(f))) continue;
            addFile(f);
        }
    }
    else {
        try {
            wregex includeRe(LR"re(<(?:Compile|ClCompile|ClInclude)\s+Include\s*=\s*"([^"]+)")re");
            for (wsregex_iterator it(wu.begin(), wu.end(), includeRe), end; it != end; ++it) {
                wstring piece = (*it)[1].str();
                size_t st = 0;
                for (size_t i = 0; i <= piece.size(); i++) {
                    if (i != piece.size() && piece[i] != L';') continue;
                    resolveOne(piece.substr(st, i - st));
                    st = i + 1;
                }
            }
        }
        catch (...) {}
    }

    set<wstring> seen;
    vector<wstring> deduped;
    for (auto& f : result.files) {
        wstring lower = toLowerW(f);
        if (seen.insert(lower).second) deduped.push_back(f);
    }
    sort(deduped.begin(), deduped.end());
    result.files = std::move(deduped);
    return result;
}

static LoadedProject LoadFromSolution(const wstring& slnPath, Lang lang, bool* languageMismatch = nullptr, Lang* detectedLanguage = nullptr) {
    if (languageMismatch) *languageMismatch = false;
    if (detectedLanguage) *detectedLanguage = lang;
    LoadedProject result;
    result.lang = lang;
    result.rootDir = parentDirOf(slnPath);
    while (!result.rootDir.empty() && (result.rootDir.back() == L'\\' || result.rootDir.back() == L'/'))
        result.rootDir.pop_back();
    result.displayName = baseNameOf(slnPath);

    ifstream ifs(slnPath.c_str(), ios::binary);
    if (!ifs) return LoadedProject();
    string u((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    ifs.close();
    if (u.size() >= 3 && (unsigned char)u[0] == 0xEF && (unsigned char)u[1] == 0xBB && (unsigned char)u[2] == 0xBF)
        u.erase(0, 3);

    wstring wu = stringToWstring(u);
    vector<wstring> projPaths;
    try {
        wregex projRe(LR"re(\bProject\s*\(\s*"[^"]*"\s*\)\s*=\s*"[^"]*"\s*,\s*"([^"]+\.(?:csproj|vbproj|vcxproj))")re", regex::icase);
        for (wsregex_iterator it(wu.begin(), wu.end(), projRe), end; it != end; ++it) {
            wstring rel = (*it)[1].str();
            for (auto& c : rel) if (c == L'/') c = L'\\';
            wstring abs = result.rootDir + L"\\" + rel;
            if (fileExistsW(abs)) projPaths.push_back(abs);
        }
    }
    catch (...) {}

    bool sawCs = false, sawVb = false, sawVcx = false;
    for (auto& p : projPaths) {
        wstring lower = toLowerW(p);
        if (endsWithW(lower, L".csproj")) sawCs = true;
        else if (endsWithW(lower, L".vbproj")) sawVb = true;
        else if (endsWithW(lower, L".vcxproj")) sawVcx = true;
    }
    int kinds = (int)sawCs + (int)sawVb + (int)sawVcx;
    if (kinds > 1) {
        LogMessage(L"This solution mixes project languages (found multiple project types). "
            L"Mixed-language solutions are not supported — pick the individual project "
            L"(.csproj / .vbproj / .vcxproj), or run once per language.", CLR_ERROR);
        return LoadedProject();
    }

    // A solution's project extensions are authoritative. Do not let the
    // currently selected target language silently load a different project type.
    if (kinds == 1) {
        Lang detected = sawCs ? Lang::CSharp : (sawVb ? Lang::VB : Lang::Cpp);
        if (detected != lang) {
            if (languageMismatch) *languageMismatch = true;
            if (detectedLanguage) *detectedLanguage = detected;
            wstring message = wstring(L"Language mismatch: the selected target is ") +
                langName(lang) + L", but this solution contains " + langName(detected) +
                L" project(s). Select " + langName(detected) +
                L" as the target language, then choose the solution again.";
            LogMessage(message, CLR_ERROR);
            return LoadedProject();
        }
    }

    result.projectFiles = projPaths;

    set<wstring> allLower;
    for (auto& p : projPaths) {
        LoadedProject sub = LoadFromProject(p, lang);
        for (auto& f : sub.files) {
            wstring lower = toLowerW(f);
            if (allLower.insert(lower).second) result.files.push_back(f);
        }
    }
    sort(result.files.begin(), result.files.end());
    return result;
}

// ============================================================================
// §6b: PrepareBackup (project mode)
// ============================================================================
static wstring StripPrefixRel(const wstring& path, const wstring& root) {
    wstring p = path, r = root;
    for (auto& c : p) if (c == L'/') c = L'\\';
    for (auto& c : r) if (c == L'/') c = L'\\';
    if (p.size() > r.size() && _wcsnicmp(p.c_str(), r.c_str(), r.size()) == 0) {
        wstring rel = p.substr(r.size());
        while (!rel.empty() && (rel[0] == L'\\' || rel[0] == L'/')) rel.erase(0, 1);
        return rel;
    }
    return L"_external\\" + baseNameOf(path);
}

static wstring PrepareBackup(const LoadedProject& proj) {
    wstring bakDir = proj.rootDir + L"\\bak";
    if (dirExistsW(bakDir)) {
        LogMessage(L"ERROR: bak\\ already exists at \"" + bakDir + L"\" — move or delete it first.", CLR_ERROR);
        return L"";
    }
    {
        DWORD err = 0;
        if (!ensureDir(bakDir, &err)) {
            LogMessage(L"ERROR: could not create \"" + bakDir + L"\" (" + Win32ErrorText(err) + L").", CLR_ERROR);
            return L"";
        }
    }

    set<wstring> siblingsLower;
    vector<wstring> siblings;
    auto queueSibling = [&](const wstring& s) {
        wstring lower = toLowerW(s);
        if (siblingsLower.insert(lower).second) siblings.push_back(s);
        };

    for (auto& f : proj.files) {
        wstring lower = toLowerW(f);
        size_t ld = f.find_last_of(L'.');
        size_t ls = f.find_last_of(L"\\/");
        if (ld == wstring::npos || (ls != wstring::npos && ld < ls)) continue;
        wstring stem = f.substr(0, ld);
        wstring ext = lower.substr(ld);

        if (lower.find(L".designer.") != wstring::npos) {
            size_t dp = stem.find_last_of(L'.');
            if (dp != wstring::npos) {
                wstring stemNoDesigner = stem.substr(0, dp);
                wstring resxPath = stemNoDesigner + L".resx";
                if (fileExistsW(resxPath)) queueSibling(resxPath);
            }
        }
        if (ext == L".cs" && lower.find(L".designer.") == wstring::npos) {
            wstring designerPath = stem + L".Designer.cs";
            if (fileExistsW(designerPath)) {
                wstring resxPath = stem + L".resx";
                if (fileExistsW(resxPath)) queueSibling(resxPath);
            }
        }
        if (ext == L".cs") {
            wstring xaml;
            if (endsWithW(lower, L".xaml.cs")) {
                xaml = f.substr(0, f.size() - 3);
            }
            else {
                xaml = stem + L".xaml";
            }
            if (fileExistsW(xaml)) queueSibling(xaml);
        }
        if (ext == L".cpp" || ext == L".h" || ext == L".hpp" || ext == L".cc" || ext == L".cxx") {
            wstring ui = stem + L".ui";
            if (fileExistsW(ui)) queueSibling(ui);
        }
    }

    int copied = 0;
    auto copyFile = [&](const wstring& src) -> bool {
        wstring rel = StripPrefixRel(src, proj.rootDir);
        wstring dest = bakDir + L"\\" + rel;
        wstring destDir = parentDirOf(dest);
        DWORD err = 0;
        if (!destDir.empty() && !ensureDir(destDir, &err)) {
            LogMessage(L"ERROR: backup could not create \"" + destDir + L"\" (" + Win32ErrorText(err) + L").", CLR_ERROR);
            return false;
        }
        if (!CopyFileW(src.c_str(), dest.c_str(), FALSE)) {
            LogMessage(L"ERROR: backup failed: \"" + src + L"\" -> \"" + dest + L"\".", CLR_ERROR);
            return false;
        }
        copied++;
        return true;
        };

    for (auto& f : proj.files) if (!copyFile(f)) return L"";
    for (auto& s : siblings) if (!copyFile(s)) return L"";

    LogMessage(L"Backup: wrote " + to_wstring(copied) + L" file(s) to \"" + bakDir + L"\".", CLR_SUCCESS);
    return bakDir;
}

// ============================================================================
// §6b (single-file half already existed; unchanged)
// ============================================================================
static wstring PrepareBackupSingleFile(const wstring& targetFile) {
    wstring bakDir = parentDirOf(targetFile) + L"bak";
    if (dirExistsW(bakDir)) {
        LogMessage(L"ERROR: bak\\ already exists at \"" + bakDir + L"\" — move or delete it first.", CLR_ERROR);
        return L"";
    }
    DWORD bakErr = 0;
    if (!ensureDir(bakDir, &bakErr)) {
        LogMessage(L"ERROR: could not create \"" + bakDir + L"\" (" + Win32ErrorText(bakErr) + L").", CLR_ERROR);
        return L"";
    }
    int copied = 0;
    auto copyIn = [&](const wstring& src) -> bool {
        if (!fileExistsW(src)) return true;
        wstring dest = bakDir + L"\\" + baseNameOf(src);
        if (!CopyFileW(src.c_str(), dest.c_str(), FALSE)) {
            LogMessage(L"ERROR: backup failed copying \"" + src + L"\" to \"" + dest + L"\".", CLR_ERROR);
            return false;
        }
        copied++;
        return true;
        };
    if (!copyIn(targetFile)) return L"";
    {
        wstring lower = toLowerW(targetFile);
        wstring xamlCandidate;
        static const wstring xamlCsSuffix = L".xaml.cs";
        if (lower.size() > xamlCsSuffix.size() && lower.compare(lower.size() - xamlCsSuffix.size(), xamlCsSuffix.size(), xamlCsSuffix) == 0)
            xamlCandidate = targetFile.substr(0, targetFile.size() - 3);
        else {
            size_t ld = targetFile.find_last_of(L'.');
            size_t ls = targetFile.find_last_of(L"\\/");
            if (ld != wstring::npos && (ls == wstring::npos || ld > ls))
                xamlCandidate = targetFile.substr(0, ld) + L".xaml";
        }
        if (!xamlCandidate.empty() && !copyIn(xamlCandidate)) return L"";

        size_t ld = targetFile.find_last_of(L'.');
        size_t ls = targetFile.find_last_of(L"\\/");
        if (ld != wstring::npos && (ls == wstring::npos || ld > ls)) {
            wstring stem = targetFile.substr(0, ld);
            wstring ext = targetFile.substr(ld);
            if (!copyIn(stem + L".Designer" + ext)) return L"";
            if (!copyIn(stem + L".resx")) return L"";
        }
    }
    LogMessage(L"Backup: wrote " + to_wstring(copied) + L" file(s) to \"" + bakDir + L"\".", CLR_SUCCESS);
    return bakDir;
}

static wstring getElementsDir() {
    wstring dir = getExeDir();
    if (dir.empty()) return L"";
    wstring elem = dir + L"elements\\";
    ensureDir(elem);
    return elem;
}

// ============================================================================
// RUN LOGGING
// ============================================================================
static wstring g_lastRunLogFolder;

static wstring getLogsDir() {
    wstring dir = getExeDir();
    if (dir.empty()) return L"";
    wstring logs = dir + L"logs\\";
    ensureDir(logs);
    return logs;
}

static wstring timestampFolderName() {
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t buf[64];
    swprintf_s(buf, L"%04d-%02d-%02d_%02d-%02d-%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return wstring(buf);
}

static void writeTextFileUtf8(const wstring& path, const wstring& content) {
    ofstream f(path.c_str(), ios::binary);
    if (!f) return;
    f << wstringToString(content);
}

struct RunLogInfo {
    wstring sourcePath;
    wstring outputPath;
    Lang lang = Lang::VB;
    bool success = false;
    wstring errorMsg;
    vector<pair<wstring, bool>> checkboxStates;
    wstring deadCodeCountSel;
    bool stringEncryptionDone = false;
    vector<pair<wstring, wstring>> stringPairs;

    bool xamlPairAttempted = false;
    bool xamlPairSucceeded = false;
    wstring xamlPairError;
    wstring xamlOutputPath;
    wstring xamlVerificationReport;

    wstring backupPath;
    vector<wstring> projectFiles;
    bool projectMode = false;

    int classRenames = 0;
    int methodRenames = 0;
    int fieldRenames = 0;
};

static void WriteRunLogs(const RunLogInfo& info) {
    wstring logsDir = getLogsDir();
    if (logsDir.empty()) return;
    wstring folder = logsDir + timestampFolderName() + L"\\";
    if (!ensureDir(folder)) return;
    g_lastRunLogFolder = folder;

    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t ts[64]; swprintf_s(ts, L"%04d-%02d-%02d_%02d-%02d-%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    if (info.projectMode) {
        wstringstream s;
        s << L"=== Project Obfuscation Log ===\r\n";
        s << L"Timestamp: " << ts << L"\r\n";
        s << L"Source: " << info.sourcePath << L"\r\n";
        s << L"Backup: " << info.backupPath << L"\r\n";
        s << L"Files: " << info.projectFiles.size() << L"\r\n";
        s << L"Language: " << langName(info.lang) << L"\r\n";
        s << L"Status: " << (info.success ? L"success" : L"FAILED") << L"\r\n";
        if (!info.errorMsg.empty()) s << L"Error: " << info.errorMsg << L"\r\n";
        s << L"\r\n--- Files ---\r\n";
        for (auto& f : info.projectFiles) s << f << L"\r\n";
        s << L"\r\n--- Options ---\r\n";
        bool deadCodeOn = false;
        for (auto& c : info.checkboxStates)
            if (c.second) {
                s << c.first << L": yes\r\n";
                if (c.first == L"Dead code injection") deadCodeOn = true;
            }
        if (deadCodeOn && !info.deadCodeCountSel.empty())
            s << L"Dead code amount: " << info.deadCodeCountSel << L"\r\n";
        s << L"\r\n--- Rename Summary ---\r\n";
        s << L"Classes renamed: " << info.classRenames << L"\r\n";
        s << L"Methods renamed: " << info.methodRenames << L"\r\n";
        s << L"Fields renamed: " << info.fieldRenames << L"\r\n";
        if (info.stringEncryptionDone)
            s << L"Strings encrypted: " << info.stringPairs.size() << L"\r\n";

        if (info.xamlPairAttempted) {
            s << L"\r\n--- XAML pairing ---\r\n";
            s << L"Result: " << (info.xamlPairSucceeded ? L"success" : L"FAILED") << L"\r\n";
            if (!info.xamlPairError.empty()) s << L"Error: " << info.xamlPairError << L"\r\n";
            if (!info.xamlVerificationReport.empty()) s << L"Handlers:\r\n" << info.xamlVerificationReport;
        }
        writeTextFileUtf8(folder + L"project_logs.txt", s.str());

        wstring perFileDir = folder + L"per_file\\";
        ensureDir(perFileDir);
        for (auto& f : info.projectFiles) {
            wstringstream pf;
            pf << L"=== Per-File Obfuscation Log ===\r\n";
            pf << L"Timestamp: " << ts << L"\r\n";
            pf << L"Source: " << f << L"\r\n";
            pf << L"Output: " << f << L"\r\n";
            pf << L"Backup: " << info.backupPath << L"\r\n";
            writeTextFileUtf8(perFileDir + baseNameOf(f) + L".txt", pf.str());
        }
    }
    else {
        wstringstream s;
        s << L"=== General Obfuscation Log ===\r\n";
        s << L"Timestamp: " << ts << L"\r\n";
        s << L"Source file: " << info.sourcePath << L"\r\n";
        if (info.success) s << L"Output file: " << info.outputPath << L"\r\n";
        if (!info.backupPath.empty()) s << L"Backup: " << info.backupPath << L"\r\n";
        s << L"Language: " << langName(info.lang) << L"\r\n";
        s << L"\r\n--- Options ---\r\n";

        bool deadCodeOn = false;
        for (auto& c : info.checkboxStates)
            if (c.second) {
                s << c.first << L": yes\r\n";
                if (c.first == L"Dead code injection") deadCodeOn = true;
            }
        if (deadCodeOn && !info.deadCodeCountSel.empty())
            s << L"Dead code amount: " << info.deadCodeCountSel << L"\r\n";
        if (info.stringEncryptionDone)
            s << L"Strings encrypted: " << to_wstring(info.stringPairs.size()) << L"\r\n";

        if (info.xamlPairAttempted) {
            s << L"\r\n--- XAML pairing ---\r\n";
            s << L"Result: " << (info.xamlPairSucceeded ? L"success" : L"FAILED") << L"\r\n";
            if (!info.xamlPairError.empty()) s << L"Error: " << info.xamlPairError << L"\r\n";
            if (!info.xamlOutputPath.empty()) s << L"Rewritten XAML: " << info.xamlOutputPath << L"\r\n";
            if (!info.xamlVerificationReport.empty()) s << L"Handlers:\r\n" << info.xamlVerificationReport;
        }

        writeTextFileUtf8(folder + L"general_logs.txt", s.str());
    }

    if (info.stringEncryptionDone && !info.stringPairs.empty()) {
        wstringstream enc, orig, dec;
        for (size_t i = 0; i < info.stringPairs.size(); i++) {
            enc << L"[" << (i + 1) << L"] " << info.stringPairs[i].second << L"\r\n";
            orig << L"[" << (i + 1) << L"] " << info.stringPairs[i].first << L"\r\n";
            dec << L"[" << (i + 1) << L"] " << info.stringPairs[i].first << L"\r\n";
        }
        writeTextFileUtf8(folder + L"strings_encrypted.txt", enc.str());
        writeTextFileUtf8(folder + L"strings_original.txt", orig.str());
        writeTextFileUtf8(folder + L"decrypted_strings.txt", dec.str());
    }
}

static void loadFlatSymbolFile(const wstring& fullPath, set<wstring>& out) {
    std::ifstream f(fullPath, std::ios::binary);
    if (!f.is_open()) return;
    std::string lineUtf8;
    bool first = true;
    while (std::getline(f, lineUtf8)) {
        if (first) {
            first = false;
            if (lineUtf8.size() >= 3 &&
                (unsigned char)lineUtf8[0] == 0xEF &&
                (unsigned char)lineUtf8[1] == 0xBB &&
                (unsigned char)lineUtf8[2] == 0xBF) {
                lineUtf8.erase(0, 3);
            }
        }
        while (!lineUtf8.empty() && (lineUtf8.back() == '\r' || lineUtf8.back() == '\n'))
            lineUtf8.pop_back();
        size_t s = lineUtf8.find_first_not_of(" \t");
        if (s == string::npos) continue;
        lineUtf8 = lineUtf8.substr(s);
        if (lineUtf8.empty() || lineUtf8[0] == '#') continue;
        out.insert(stringToWstring(lineUtf8));
    }
}

static bool extractResourceById(int id, const wstring& dest) {
    if (fileExistsW(dest)) {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExW(dest.c_str(), GetFileExInfoStandard, &fad)) {
            LARGE_INTEGER sz; sz.HighPart = fad.nFileSizeHigh; sz.LowPart = fad.nFileSizeLow;
            if (sz.QuadPart > 0) return true;
        }
    }
    HMODULE m = GetModuleHandleW(nullptr);
    HRSRC hr = FindResourceW(m, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!hr) return false;
    DWORD sz = SizeofResource(m, hr);
    if (sz == 0) return false;
    HGLOBAL hg = LoadResource(m, hr);
    if (!hg) return false;
    const void* pd = LockResource(hg);
    if (!pd) return false;
    std::ofstream f(wstringToString(dest), std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(pd), sz);
    return f.good();
}

static int extractAllEmbeddedElements() {
    wstring dir = getElementsDir();
    if (dir.empty()) return 0;
    int n = 0;
    if (extractResourceById(RES_WIN32_EXPORTS, dir + kElemWin32Exports))  n++;
    if (extractResourceById(RES_WIN32_LIBS, dir + kElemWin32Libs))     n++;
    if (extractResourceById(RES_STL_HEADERS, dir + kElemStlHeaders))    n++;
    if (extractResourceById(RES_NET_ASSEMBLIES, dir + kElemNetAssemblies)) n++;
    if (extractResourceById(RES_NET_MEMBERS, dir + kElemNetMembers))   n++;
    if (extractResourceById(RES_VB_RUNTIME, dir + kElemVbRuntime))    n++;
    if (extractResourceById(RES_MANIFEST, dir + kElemManifest))      n++;
    return n;
}

static void loadSymbolsFromElements() {
    g_generatedWin32Symbols.clear();
    g_generatedThirdPartySymbols.clear();
    g_generatedHeaderOnlySymbols.clear();
    g_generatedNetSymbols.clear();
    g_generatedNetMembers.clear();
    g_generatedVbRuntime.clear();

    wstring dir = getElementsDir();
    if (dir.empty()) return;

    loadFlatSymbolFile(dir + kElemWin32Exports, g_generatedWin32Symbols);
    loadFlatSymbolFile(dir + kElemWin32Libs, g_generatedWin32Symbols);
    loadFlatSymbolFile(dir + kElemStlHeaders, g_generatedHeaderOnlySymbols);
    loadFlatSymbolFile(dir + kElemNetAssemblies, g_generatedNetSymbols);   // legacy fallback
    loadFlatSymbolFile(dir + kElemNetMembers, g_generatedNetMembers);     // authoritative (spec A1)
    loadFlatSymbolFile(dir + kElemVbRuntime, g_generatedVbRuntime);       // spec A2
}

static bool symbolsFolderHasFiles() {
    wstring dir = getElementsDir();
    if (dir.empty()) return false;
    wstring pattern = dir + L"*";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) { found = true; break; }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return found;
}

// ============================================================================
// SYMBOL-SOURCE ALERT TOAST
// ============================================================================
namespace SymbolToast {
    using namespace Gdiplus;

    const int CardW = 400, CardH = 96, M = 22, Radius = 18, ScreenPad = 18;
    const double InMs = 1100, OutMs = 480, LifeMs = 5000, TypeDelay = 220, CharMs = 20;

    const wchar_t* kClassName = L"ObfuscatorSymbolToast";

    struct State {
        HWND hwnd = nullptr;
        UINT_PTR timerId = 0;
        wstring title = L"OBFUSCATOR // SYMBOLS";
        wstring message;
        ULONGLONG startTick = 0;
        bool closing = false;
    };
    static State g_state;

    inline double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
    inline Color withAlpha(const Color& c, int a) {
        if (a < 0) a = 0; if (a > 255) a = 255;
        return Color((BYTE)a, c.GetR(), c.GetG(), c.GetB());
    }
    inline double spring(double p) {
        p = clamp01(p);
        double s = 1.0 - pow(1.0 - p, 3.0);
        return s + 0.06 * sin(p * 6.2831853) * (1.0 - p);
    }
    inline double inBack(double p, double s) {
        p = clamp01(p);
        return p * p * ((s + 1.0) * p - s);
    }

    static GraphicsPath* roundRect(const RectF& r, float rad) {
        GraphicsPath* p = new GraphicsPath();
        float d = rad * 2.0f;
        if (d > r.Width) d = r.Width;
        if (d > r.Height) d = r.Height;
        p->AddArc(r.X, r.Y, d, d, 180, 90);
        p->AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
        p->AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
        p->AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
        p->CloseFigure();
        return p;
    }

    static const Color kAccent(255, 120, 255, 140);
    static const Color kAccentDark(255, 0, 160, 60);
    static const Color kTitle(255, 60, 170, 90);
    static const Color kBodyTop(244, 8, 52, 26);
    static const Color kBodyBottom(244, 3, 26, 13);

    static void drawIcon(Graphics& g, float cx, float cy, double pulse) {
        PointF tri[3] = { PointF(cx, cy - 15), PointF(cx - 16, cy + 13), PointF(cx + 16, cy + 13) };
        for (int i = 4; i >= 1; i--) {
            Pen pen(withAlpha(kAccent, (int)(9 + 6 * pulse)), (REAL)(i * 3));
            pen.SetLineJoin(LineJoinRound);
            g.DrawPolygon(&pen, tri, 3);
        }
        SolidBrush fillB(withAlpha(kAccent, 38));
        g.FillPolygon(&fillB, tri, 3);
        Pen outline(kAccent, 2.2f);
        outline.SetLineJoin(LineJoinRound);
        g.DrawPolygon(&outline, tri, 3);
    }

    static void drawFrame(Graphics& g, double t, const wstring& title, const wstring& message, double lifeMs) {
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);

        RectF card((REAL)M, (REAL)M, (REAL)CardW, (REAL)CardH);
        double pulse = 0.5 + 0.5 * sin(t / 420.0);

        for (int i = M; i >= 1; i--) {
            int a = (int)(46.0 * pow(1.0 - i / (double)(M + 1), 1.7));
            RectF r(card.X - i, card.Y - i + 4, card.Width + 2 * i, card.Height + 2 * i);
            GraphicsPath* p = roundRect(r, (float)(Radius + i));
            SolidBrush b(Color(a, 0, 18, 6));
            g.FillPath(&b, p);
            delete p;
        }

        GraphicsPath* cardPath = roundRect(card, (float)Radius);

        for (int i = 3; i >= 1; i--) {
            Pen pen(withAlpha(kAccent, (int)(7 + 5 * pulse)), (REAL)(i * 3));
            g.DrawPath(&pen, cardPath);
        }

        LinearGradientBrush body(card, kBodyTop, kBodyBottom, LinearGradientModeVertical);
        g.FillPath(&body, cardPath);

        RectF sheen(card.X + 1, card.Y + 1, card.Width - 2, card.Height * 0.5f);
        GraphicsPath* sp = roundRect(sheen, (float)Radius - 1);
        LinearGradientBrush sheenBrush(sheen, Color(30, 255, 255, 255), Color(0, 255, 255, 255), LinearGradientModeVertical);
        g.FillPath(&sheenBrush, sp);
        delete sp;

        drawIcon(g, card.X + 40, card.Y + card.Height / 2.0f - 3, pulse);

        float tx = card.X + 74;
        FontFamily ff(L"Consolas");
        Font titleFont(&ff, 8.0f, FontStyleBold, UnitPoint);
        SolidBrush titleB(withAlpha(kTitle, 230));
        g.DrawString(title.c_str(), -1, &titleFont, PointF(tx, card.Y + 12), &titleB);

        int n = (int)std::max(0.0, std::min((double)message.size(), (t - TypeDelay) / CharMs));
        bool typing = n < (int)message.size();
        wstring shown = message.substr(0, n) + ((typing || ((int)(t / 420) % 2 == 0)) ? L"_" : L" ");
        RectF textRect(tx, card.Y + 30, card.Width - 74 - 20, card.Height - 30 - 18);
        StringFormat sf;
        sf.SetTrimming(StringTrimmingEllipsisWord);
        Font msgFont(&ff, 10.5f, FontStyleBold, UnitPoint);
        SolidBrush msgB(kAccent);
        g.DrawString(shown.c_str(), -1, &msgFont, textRect, &sf, &msgB);

        float remain = (float)clamp01(1.0 - t / lifeMs);
        RectF track(tx, card.Y + card.Height - 11, card.Width - 74 - 20, 3);
        GraphicsPath* tp = roundRect(track, 1.5f);
        SolidBrush trackB(withAlpha(kAccent, 28));
        g.FillPath(&trackB, tp);
        delete tp;
        if (remain > 0.01f) {
            RectF fill(track.X, track.Y, std::max(3.0f, track.Width * remain), track.Height);
            GraphicsPath* fp = roundRect(fill, 1.5f);
            LinearGradientBrush fillBrush(track, kAccentDark, kAccent, LinearGradientModeHorizontal);
            g.FillPath(&fillBrush, fp);
            delete fp;
        }

        Pen rim(withAlpha(kAccent, (int)(70 + 50 * pulse)), 1.4f);
        g.DrawPath(&rim, cardPath);

        RectF innerR(card.X + 1.8f, card.Y + 1.8f, card.Width - 3.6f, card.Height - 3.6f);
        GraphicsPath* inner = roundRect(innerR, (float)Radius - 2);
        Pen innerPen(Color(22, 255, 255, 255), 1.0f);
        g.DrawPath(&innerPen, inner);
        delete inner;

        delete cardPath;
    }

    static void push(HWND hwnd, int bmpW, int bmpH, double t, const wstring& title,
        const wstring& message, double lifeMs, int x, int y, BYTE alpha) {
        Bitmap bmp(bmpW, bmpH, PixelFormat32bppPARGB);
        {
            Graphics g(&bmp);
            g.Clear(Color(0, 0, 0, 0));
            drawFrame(g, t, title, message, lifeMs);
        }

        HDC screenDc = GetDC(nullptr);
        HDC memDc = CreateCompatibleDC(screenDc);

        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = bmpW;
        bi.bmiHeader.biHeight = -bmpH;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP hBmp = CreateDIBSection(memDc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (hBmp && bits) {
            BitmapData bd;
            Rect lockRect(0, 0, bmpW, bmpH);
            if (bmp.LockBits(&lockRect, ImageLockModeRead, PixelFormat32bppPARGB, &bd) == Ok) {
                for (int row = 0; row < bmpH; row++)
                    memcpy((BYTE*)bits + row * bmpW * 4, (BYTE*)bd.Scan0 + row * bd.Stride, bmpW * 4);
                bmp.UnlockBits(&bd);
            }

            HGDIOBJ old = SelectObject(memDc, hBmp);
            POINT pos = { x, y };
            SIZE size = { bmpW, bmpH };
            POINT src = { 0, 0 };
            BLENDFUNCTION blend = { AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
            UpdateLayeredWindow(hwnd, screenDc, &pos, &size, memDc, &src, 0, &blend, ULW_ALPHA);
            SelectObject(memDc, old);
            DeleteObject(hBmp);
        }
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
    }

    static void onTick(HWND hwnd) {
        ULONGLONG t = GetTickCount64() - g_state.startTick;
        double td = (double)t;
        double life = g_state.closing ? std::min(td, LifeMs) : LifeMs;

        int bmpW = CardW + 2 * M, bmpH = CardH + 2 * M;
        RECT wa; SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
        int targetX = wa.right - ScreenPad - CardW - M;
        int targetY = wa.bottom - ScreenPad - CardH - M;
        int offX = wa.right + 8;
        double travel = offX - targetX;

        double x; double alpha = 1.0;
        if (td < InMs) {
            x = targetX + travel * (1.0 - spring(td / InMs));
            alpha = clamp01(td / 180.0);
        }
        else if (td < life) {
            x = targetX;
        }
        else if (td < life + OutMs) {
            double p = (td - life) / OutMs;
            x = targetX + travel * inBack(p, 1.3);
            alpha = 1.0 - clamp01((p - 0.5) / 0.5);
        }
        else {
            KillTimer(hwnd, g_state.timerId);
            DestroyWindow(hwnd);
            g_state.hwnd = nullptr;
            return;
        }

        push(hwnd, bmpW, bmpH, td, g_state.title, g_state.message, life, (int)llround(x), targetY, (BYTE)(255 * alpha));
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_TIMER:
            onTick(hwnd);
            return 0;
        case WM_LBUTTONDOWN:
            g_state.closing = true;
            return 0;
        case WM_NCHITTEST:
            return HTCLIENT;
        case WM_DESTROY:
            return 0;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    static void registerClassOnce(HINSTANCE hInst) {
        static bool done = false;
        if (done) return;
        WNDCLASS wc = {};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = kClassName;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClass(&wc);
        done = true;
    }

    static void show(const wstring& message, const wstring& title = L"OBFUSCATOR // SYMBOLS") {
        HINSTANCE hInst = GetModuleHandle(nullptr);
        registerClassOnce(hInst);

        if (g_state.hwnd) {
            KillTimer(g_state.hwnd, g_state.timerId);
            DestroyWindow(g_state.hwnd);
            g_state.hwnd = nullptr;
        }

        g_state.title = title;
        g_state.message = message;
        g_state.closing = false;

        int bmpW = CardW + 2 * M, bmpH = CardH + 2 * M;
        HWND hwnd = CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            kClassName, L"", WS_POPUP,
            0, 0, bmpW, bmpH, nullptr, nullptr, hInst, nullptr);
        if (!hwnd) return;

        g_state.hwnd = hwnd;
        g_state.startTick = GetTickCount64();
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        g_state.timerId = SetTimer(hwnd, 1, 15, nullptr);
        onTick(hwnd);
    }
}

// ============================================================================
// GRABBER HELPERS
// ============================================================================
static wstring findDumpbin() {
    const wchar_t* roots[] = {
        L"C:\\Program Files\\Microsoft Visual Studio\\2022\\",
        L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\",
        L"C:\\Program Files\\Microsoft Visual Studio\\2019\\",
        L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\",
    };
    const wchar_t* editions[] = {
        L"Community", L"Professional", L"Enterprise", L"BuildTools", L"Preview", L"WDExpress"
    };
    for (auto root : roots) for (auto edition : editions) {
        wstring vsRoot = wstring(root) + edition + L"\\VC\\Tools\\MSVC\\";
        WIN32_FIND_DATAW fdVer;
        HANDLE hVer = FindFirstFileW((vsRoot + L"*").c_str(), &fdVer);
        if (hVer == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(fdVer.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (fdVer.cFileName[0] == L'.') continue;
            wstring candidate = vsRoot + fdVer.cFileName + L"\\bin\\Hostx64\\x64\\dumpbin.exe";
            if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
                FindClose(hVer);
                return candidate;
            }
        } while (FindNextFileW(hVer, &fdVer));
        FindClose(hVer);
    }
    return L"";
}

static wstring findLatestSdkIncludeDir() {
    wstring root = L"C:\\Program Files (x86)\\Windows Kits\\10\\Include\\";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((root + L"10.0.*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return L"";
    wstring best;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        wstring candidate = root + fd.cFileName + L"\\";
        if (candidate > best) best = candidate;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return best;
}

static wstring findLatestSdkLibRoot() {
    wstring inc = findLatestSdkIncludeDir();
    if (inc.empty()) return L"";
    size_t p = inc.rfind(L"\\Include\\");
    if (p == wstring::npos) return L"";
    wstring sdkRoot = inc.substr(0, p + 1);
    wstring ver = inc.substr(p + 9);
    while (!ver.empty() && ver.back() == L'\\') ver.pop_back();
    return sdkRoot + L"Lib\\" + ver + L"\\";
}

static wstring findMsvcIncludeDir() {
    wstring d = findDumpbin();
    if (d.empty()) return L"";
    size_t vp = d.find(L"\\VC\\Tools\\MSVC\\");
    if (vp == wstring::npos) return L"";
    wstring mr = d.substr(0, vp + 15);
    size_t vs = mr.size();
    size_t ve = d.find(L"\\bin\\", vs);
    if (ve == wstring::npos) return L"";
    return mr + d.substr(vs, ve - vs) + L"\\include\\";
}

typedef std::function<void(const wstring&)> PSOutputCallback;

static DWORD runPowerShell(const wstring& args, PSOutputCallback onLine) {
    wstring cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass " + args;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return (DWORD)-1;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite; si.hStdError = hWrite; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');
    BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);
    if (!ok) { CloseHandle(hRead); return (DWORD)-1; }
    char buf[4096]; DWORD read = 0; std::string acc;
    while (ReadFile(hRead, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
        buf[read] = '\0'; acc += buf;
        size_t pos;
        while ((pos = acc.find('\n')) != std::string::npos) {
            std::string line = acc.substr(0, pos); acc.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (onLine) onLine(stringToWstring(line));
        }
    }
    if (!acc.empty() && onLine) onLine(stringToWstring(acc));
    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD rc = (DWORD)-1; GetExitCodeProcess(pi.hProcess, &rc);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return rc;
}

static wstring writeTempListFile(const vector<wstring>& lines) {
    wchar_t tmpDir[MAX_PATH], tmpPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpDir);
    GetTempFileNameW(tmpDir, L"ofv", 0, tmpPath);
    std::ofstream f(tmpPath, std::ios::binary);
    for (auto& l : lines) f << wstringToString(l) << "\n";
    f.close();
    return tmpPath;
}

static void runDumpbinBatch(const wstring& dumpbin, const wstring& flag, const wstring& matchExpr,
    const vector<wstring>& files, PSOutputCallback onLine,
    std::function<void(size_t)> onProgress) {
    if (files.empty()) return;
    wstring listPath = writeTempListFile(files);
    wstringstream s;
    s << L"$files = Get-Content -LiteralPath '" << listPath << L"'; $i = 0; foreach ($p in $files) { $i++;"
        << L"$o = & '" << dumpbin << L"' " << flag << L" $p 2>$null; foreach ($l in $o) { if ($l -match '"
        << matchExpr << L"') { Write-Host $matches[1] } }"
        << L"Write-Host ('##' + $i + '##') }";
    wstring args = L"-NoProfile -ExecutionPolicy Bypass -Command \"" + s.str() + L"\"";
    runPowerShell(args, [&](const wstring& l) {
        if (l.size() > 4 && l.front() == L'#' && l.back() == L'#') {
            size_t n = wcstoul(l.substr(2, l.size() - 4).c_str(), nullptr, 10);
            if (onProgress) onProgress(n);
            return;
        }
        if (onLine) onLine(l);
        });
    DeleteFileW(listPath.c_str());
}

struct DevEnvCheck { bool hasDumpbin; wstring dumpbinPath; bool hasSdk; wstring sdkDir; };

static DevEnvCheck CheckDevEnvironment() {
    DevEnvCheck c;
    c.dumpbinPath = findDumpbin();
    c.hasDumpbin = !c.dumpbinPath.empty();
    c.sdkDir = findLatestSdkLibRoot();
    c.hasSdk = !c.sdkDir.empty();
    return c;
}

static bool writeSymbolSet(const wstring& path, const set<wstring>& syms) {
    std::ofstream f(wstringToString(path), std::ios::binary);
    if (!f) return false;
    f << "\xEF\xBB\xBF";
    for (const auto& s : syms) { if (s.empty()) continue; f << wstringToString(s) << "\n"; }
    return f.good();
}

struct GrabberHints {
    vector<wstring> dllNames;
    vector<wstring> libNames;
    vector<wstring> headerDirs;
};

static GrabberHints collectGrabberHints() {
    GrabberHints h;
    wchar_t sys[MAX_PATH] = {}; GetSystemDirectoryW(sys, MAX_PATH);
    wstring sysDir = wstring(sys) + L"\\";
    WIN32_FIND_DATAW fd;
    HANDLE hh = FindFirstFileW((sysDir + L"*.dll").c_str(), &fd);
    if (hh != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                h.dllNames.push_back(sysDir + fd.cFileName);
        } while (FindNextFileW(hh, &fd));
        FindClose(hh);
    }
    wstring libRoot = findLatestSdkLibRoot();
    if (!libRoot.empty()) {
        const wchar_t* arches[] = { L"x64", L"x86", L"arm64" };
        const wchar_t* kinds[] = { L"um", L"ucrt" };
        for (auto a : arches) for (auto k : kinds) {
            wstring sub = libRoot + k + L"\\" + a + L"\\";
            WIN32_FIND_DATAW fdl;
            HANDLE hl = FindFirstFileW((sub + L"*.lib").c_str(), &fdl);
            if (hl == INVALID_HANDLE_VALUE) continue;
            do {
                if (!(fdl.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    h.libNames.push_back(sub + fdl.cFileName);
            } while (FindNextFileW(hl, &fdl));
            FindClose(hl);
        }
    }
    wstring incDir = findMsvcIncludeDir();
    if (!incDir.empty()) h.headerDirs.push_back(incDir);
    return h;
}

// ============================================================================
// APP STATE
// ============================================================================
enum class SymbolMode { Embedded, Generate };
static SymbolMode g_symbolMode = SymbolMode::Embedded;
static std::atomic<bool> g_grabberDone{ false };
// Guards the heavy obfuscation pass (regex-heavy identifier extraction/rename,
// XAML/resx/designer pairing, string encryption, dead-code injection, file I/O)
// which now runs on a background thread so the UI stays responsive on large
// files/projects. Set true right before the worker thread is spawned, cleared
// by the worker itself just before it returns.
static std::atomic<bool> g_obfuscationBusy{ false };

static void ApplyMode() {
    if (g_symbolMode == SymbolMode::Embedded) {
        extractAllEmbeddedElements();
    }
    loadSymbolsFromElements();
}

static void LogSymbolCounts(const wchar_t* prefix) {
    LogMessage(wstring(prefix) + L": win32=" + to_wstring(g_generatedWin32Symbols.size()) +
        L" thirdparty=" + to_wstring(g_generatedThirdPartySymbols.size()) +
        L" headeronly=" + to_wstring(g_generatedHeaderOnlySymbols.size()) +
        L" net=" + to_wstring(g_generatedNetSymbols.size()) +
        L" netmembers=" + to_wstring(g_generatedNetMembers.size()) +
        L" vbruntime=" + to_wstring(g_generatedVbRuntime.size()), CLR_TEXT_DIM);
}

static DWORD WINAPI GrabberWorker(LPVOID) {
    wstring elements = getElementsDir();
    if (elements.empty()) { g_grabberDone.store(true); return 0; }
    LogMessage(L"=== GRABBER: writing to " + elements + L" ===", CLR_ACCENT);

    wstring dumpbin = findDumpbin();
    if (dumpbin.empty())
        LogMessage(L"  WARN: dumpbin not found — Win32/STL scan will produce nothing.", CLR_WARN);
    else
        LogMessage(L"  dumpbin: " + dumpbin, CLR_TEXT_DIM);

    GrabberHints hints = collectGrabberHints();
    LogMessage(L"  hints: " + to_wstring(hints.dllNames.size()) + L" dlls, " +
        to_wstring(hints.libNames.size()) + L" libs, " +
        to_wstring(hints.headerDirs.size()) + L" header dirs", CLR_TEXT_DIM);

    set<wstring> exports, libs, headers, net;

    if (!dumpbin.empty()) {
        LogMessage(L"  exports: scanning " + to_wstring(hints.dllNames.size()) + L" dlls (1 batched process)...", CLR_INFO);
        runDumpbinBatch(dumpbin, L"/exports",
            L"^\\s*\\d+\\s+[0-9A-Fa-f]+\\s+[0-9A-Fa-f]+\\s+([A-Za-z_]\\w*)\\s*(=.*)?$",
            hints.dllNames,
            [&](const wstring& l) { if (!l.empty()) exports.insert(l); },
            [&](size_t n) { if (n % 25 == 0 || n == hints.dllNames.size())
            LogMessage(L"  exports [" + to_wstring(n) + L"/" + to_wstring(hints.dllNames.size()) + L"]", CLR_TEXT_DIM); });
    }

    if (!dumpbin.empty()) {
        LogMessage(L"  libs: scanning " + to_wstring(hints.libNames.size()) + L" libs (1 batched process)...", CLR_INFO);
        runDumpbinBatch(dumpbin, L"/symbols",
            L"External\\s+\\|\\s+(\\S+)\\s*$",
            hints.libNames,
            [&](const wstring& l) {
                wstring sym = l;
                if (sym.rfind(L"__imp_", 0) == 0) sym = sym.substr(6);
                size_t at = sym.find(L'@');
                if (!sym.empty() && sym[0] == L'_' && at != wstring::npos) sym = sym.substr(1, at - 1);
                if (sym.empty() || sym[0] == L'?' || sym[0] == L'.') return;
                libs.insert(sym);
            },
            [&](size_t n) { if (n % 50 == 0 || n == hints.libNames.size())
            LogMessage(L"  libs    [" + to_wstring(n) + L"/" + to_wstring(hints.libNames.size()) + L"]", CLR_TEXT_DIM); });
    }

    for (const auto& dir : hints.headerDirs) {
        wstringstream s;
        s << L"Get-ChildItem -Path '" << dir << L"' -Recurse -Include *.h,*.hpp -File -ErrorAction SilentlyContinue | ForEach-Object { Select-String -Path $_.FullName -Pattern '^\\s*(?:inline\\s+|constexpr\\s+|static\\s+)*[A-Za-z_][\\w:<>,\\s\\*&]*\\b([A-Za-z_]\\w*)\\s*\\(' -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_.Matches[0].Groups[1].Value } }";
        wstring args = L"-NoProfile -ExecutionPolicy Bypass -Command \"" + s.str() + L"\"";
        runPowerShell(args, [&](const wstring& l) { if (!l.empty()) headers.insert(l); });
    }

    {
        wstringstream s;
        // Two gaps closed here, both seen as real compile failures:
        //  - The old $roots list only covered .NET Core/5+ shared runtimes plus
        //    the .NET Framework *reference* assemblies (metadata-only stubs that
        //    LoadFrom can fail/throw on, or simply don't carry every member).
        //    A classic .NET Framework WinForms app's real implementation DLLs
        //    (where things like System.Windows.Forms.MouseButtons actually live)
        //    are under the GAC and the Framework runtime directories, which
        //    weren't scanned at all -- hence enum members like MouseButtons.Left
        //    getting renamed (CS1061).
        //  - Third-party NuGet control libraries (Guna.UI2, DrakeUI, etc.) were
        //    never scanned at all -- only system/BCL locations were in $roots.
        //    Their restored DLLs live under the per-user NuGet global package
        //    cache regardless of which specific project uses them, so scanning
        //    that cache (same GetExportedTypes+GetMembers approach) catches
        //    these without needing to know the current project's path. This is
        //    the fix for DrakeUIListBox-style CS1061 errors.
        s << L"$roots = @("
            L"'C:\\Program Files\\dotnet\\shared\\Microsoft.NETCore.App',"
            L"'C:\\Program Files\\dotnet\\shared\\Microsoft.AspNetCore.App',"
            L"'C:\\Program Files\\dotnet\\shared\\Microsoft.WindowsDesktop.App',"
            L"'C:\\Program Files (x86)\\Reference Assemblies\\Microsoft\\Framework\\.NETFramework',"
            L"'C:\\Windows\\Microsoft.NET\\Framework\\v4.0.30319',"
            L"'C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319',"
            L"'C:\\Windows\\Microsoft.NET\\assembly\\GAC_MSIL',"
            L"'C:\\Windows\\Microsoft.NET\\assembly\\GAC_32',"
            L"'C:\\Windows\\Microsoft.NET\\assembly\\GAC_64',"
            L"(Join-Path $env:USERPROFILE '.nuget\\packages')"
            L");"
            L"foreach ($root in $roots) { if (-not $root -or -not (Test-Path $root)) { continue };"
            L"Get-ChildItem -Path $root -Recurse -Filter '*.dll' -ErrorAction SilentlyContinue | ForEach-Object {"
            L"  try { $asm = [System.Reflection.Assembly]::LoadFrom($_.FullName); foreach ($t in $asm.GetExportedTypes()) { if ($t.Name) { Write-Host $t.Name }; try {"
            L" $flags = [System.Reflection.BindingFlags]::Public -bor [System.Reflection.BindingFlags]::NonPublic -bor [System.Reflection.BindingFlags]::Instance -bor [System.Reflection.BindingFlags]::Static;"
            // NonPublic alone also matches private members; a protected virtual
            // method like UIElement.OnMouseLeftButtonDown (the case this fixes --
            // GetMembers()'s public-only default overload above never saw it) is
            // IsFamily or IsFamilyOrAssembly, never IsPrivate/IsAssembly, so this
            // filter keeps exactly "public, or overridable from user code" and
            // still excludes a base type's private implementation details.
            L" foreach ($m in $t.GetMembers($flags)) { if ($m.Name -and ($m.IsPublic -or $m.IsFamily -or $m.IsFamilyOrAssembly)) { Write-Host $m.Name } } } catch {} } } catch {} } }";
        wstring args = L"-NoProfile -ExecutionPolicy Bypass -Command \"" + s.str() + L"\"";
        runPowerShell(args, [&](const wstring& l) { if (!l.empty()) net.insert(l); });
    }

    // Spec A2: separate reflection pass over the VB runtime / My-namespace
    // backing modules. These live in Microsoft.VisualBasic(.Core) and are not
    // discoverable by the general $roots scan above (that scan only walks
    // assemblies physically present under the GAC/.NET dirs/NuGet cache --
    // Microsoft.VisualBasic is normally already loaded into the PowerShell
    // host process itself, so it's reflected directly by type name instead).
    set<wstring> vbRuntime;
    {
        wstringstream s;
        s << L"Add-Type -AssemblyName Microsoft.VisualBasic -ErrorAction SilentlyContinue;"
            L"$names = @("
            L"'Microsoft.VisualBasic.Interaction','Microsoft.VisualBasic.Strings',"
            L"'Microsoft.VisualBasic.Conversion','Microsoft.VisualBasic.DateAndTime',"
            L"'Microsoft.VisualBasic.FileSystem','Microsoft.VisualBasic.Information',"
            L"'Microsoft.VisualBasic.Constants','Microsoft.VisualBasic.ControlChars',"
            L"'Microsoft.VisualBasic.VBMath','Microsoft.VisualBasic.Financial',"
            L"'Microsoft.VisualBasic.CompilerServices.NewLateBinding',"
            L"'Microsoft.VisualBasic.CompilerServices.Conversions',"
            L"'Microsoft.VisualBasic.CompilerServices.ProjectData',"
            L"'Microsoft.VisualBasic.CompilerServices.Utils',"
            L"'Microsoft.VisualBasic.CompilerServices.LikeOperator',"
            L"'Microsoft.VisualBasic.CompilerServices.Operators',"
            L"'Microsoft.VisualBasic.CompilerServices.ObjectFlowControl',"
            L"'Microsoft.VisualBasic.CompilerServices.FlowControl',"
            L"'Microsoft.VisualBasic.CompilerServices.StandardModuleAttribute',"
            L"'Microsoft.VisualBasic.CompilerServices.DesignerGeneratedAttribute'"
            L");"
            L"$asm = [Microsoft.VisualBasic.Strings].Assembly;"
            L"foreach ($tn in $names) {"
            L"  try { $t = $asm.GetType($tn); if (-not $t) { continue }; if ($t.Name) { Write-Host $t.Name };"
            L"    $flags = [System.Reflection.BindingFlags]::Public -bor [System.Reflection.BindingFlags]::NonPublic -bor [System.Reflection.BindingFlags]::Instance -bor [System.Reflection.BindingFlags]::Static;"
            L"    foreach ($m in $t.GetMembers($flags)) {"
            L"      if ($m.Name -and ($m.IsPublic -or $m.IsFamily -or $m.IsFamilyOrAssembly)) { Write-Host $m.Name } } } catch {} }";
        wstring args = L"-NoProfile -ExecutionPolicy Bypass -Command \"" + s.str() + L"\"";
        runPowerShell(args, [&](const wstring& l) { if (!l.empty()) vbRuntime.insert(l); });
    }

    LogMessage(L"  collected: exports=" + to_wstring(exports.size()) +
        L" libs=" + to_wstring(libs.size()) +
        L" headers=" + to_wstring(headers.size()) +
        L" net=" + to_wstring(net.size()) +
        L" vbruntime=" + to_wstring(vbRuntime.size()), CLR_TEXT_DIM);

    writeSymbolSet(elements + kElemWin32Exports, exports);
    writeSymbolSet(elements + kElemWin32Libs, libs);
    writeSymbolSet(elements + kElemStlHeaders, headers);
    writeSymbolSet(elements + kElemNetAssemblies, net);   // legacy, kept for backward compat (spec A1)
    writeSymbolSet(elements + kElemNetMembers, net);      // authoritative (spec A1) -- same reflected
    // type+member data, same BindingFlags
    // (Public|Instance|Static is GetMembers()'s
    // default overload, already in use above)
    writeSymbolSet(elements + kElemVbRuntime, vbRuntime); // spec A2

    {
        wstring path = elements + kElemManifest;
        ofstream f(wstringToString(path), ios::binary);
        if (f) {
            SYSTEMTIME st; GetLocalTime(&st);
            f << "{\n  \"tool\": \"obfusvb\",\n  \"generatedAt\": \""
                << (int)st.wYear << "-" << (int)st.wMonth << "-" << (int)st.wDay << "T"
                << (int)st.wHour << ":" << (int)st.wMinute << ":" << (int)st.wSecond << "\",\n";
            f << "  \"counts\": {\n";
            f << "    \"win32_exports\": " << exports.size() << ",\n";
            f << "    \"win32_libs\": " << libs.size() << ",\n";
            f << "    \"stl_headers\": " << headers.size() << ",\n";
            f << "    \"net_assemblies\": " << net.size() << "\n";
            f << "  }\n}\n";
        }
    }

    LogMessage(L"=== GRABBER: done ===", CLR_SUCCESS);
    g_grabberDone.store(true);
    return 0;
}

// ============================================================================
// GUI
// ============================================================================
#define IDC_FILEPATH       100
#define IDC_BROWSE         101
#define IDC_CHK_CLASSES    102
#define IDC_CHK_CONTROLS   103
#define IDC_CHK_PUBLIC     104
#define IDC_CHK_CUSTOM     105
#define IDC_CUSTOM_NAMES   106
#define IDC_OBFUSCATE      107
#define IDC_LOG            108
#define IDC_PROGRESS       109
#define IDC_STATUS            110
#define IDC_LANG_COMBO     111
#define IDC_CHK_MACROS     112
#define IDC_CHK_LABELS     113
#define IDC_CHK_OPERATORS  114
#define IDC_CHK_NAMESPACES 115
#define IDC_CHK_TYPEDEFS   116
#define IDC_CHK_TEMPLATES  117
#define IDC_CHK_PRECOMP    118
#define IDC_BTN_EMBEDDED   120
#define IDC_BTN_GENERATE   121
#define IDC_BTN_REFRESH    122
#define IDC_BTN_RESCAN_REFS 123   // Part B4: "Re-scan project references"
#define IDC_CHK_O0O         130
#define IDC_CHK_VWV         131
#define IDC_CHK_I1L         132
#define IDC_CHK_DEADCODE    133
#define IDC_CHK_MIX         134
#define IDC_CMB_COUNT       135
#define IDC_CHK_STRENC      136
#define IDC_CHK_INJECT_DECRYPT 137
#define IDC_CHK_SKIP_ENCRYPTED 138
#define IDC_CHK_SKIP_EXTERNAL  139
#define IDC_CHK_STREXCL         140
#define IDC_EDIT_STREXCL        141
#define IDC_BTN_SHOWLOG         142

#define IDC_MODE_LANG       200
#define IDC_MODE_SINGLE     201
#define IDC_MODE_FOLDER     202
#define IDC_MODE_PROJECT    203
#define IDC_MODE_SOLUTION   204

HWND hWndMain, hEditPath, hBtnBrowse, hComboLang, hVersion, hTitleStatic;
HWND hChkO0o, hChkVWV, hChkI1L;
HWND hChkDeadCode, hChkMix, hCmbCount;
HWND hChkStrEnc, hChkInjectDecrypt, hChkSkipEncrypted;
HWND hChkSkipExternal;
HWND hLblAdvanced2;
HWND hChkStrExcl, hEditStrExcl;
HWND hChkClasses, hChkControls, hChkPublic, hChkCustom, hEditCustom;
HWND hBtnObfuscate, hLog, hProgress, hStatus;
HWND hBtnShowLog;
HWND hChkMacros, hChkLabels, hChkOperators, hChkNamespaces, hChkTypedefs, hChkTemplates, hChkPrecomp;
HWND hSegEmbedded, hSegGenerate, hBtnRefresh, hBtnRescanRefs;
HFONT hFontUI, hFontMono, hFontBold, hFontTitle, hFontSmall, hFontMonoSmall, hFontSection, hFontTiny;

static int g_clientW = 1280;
static int g_clientH = 800;
static int g_stackHCache = 360;
static int g_cardLogHCache = 360;
static int g_actionCardTop = 0;
static int g_optCardTop = 0;
static int g_optCardH = 400;
static int g_logCardTop = 0;
static int g_logCardBottom = 0;

static bool g_logCollapsed = false;
static double g_logAnimT = 0.0;
static const int kShowLogBtnH = 34;

static int g_animTick = 0;

static bool g_glitchEnabled = true;
static ULONGLONG g_animStart = 0;
static ULONGLONG g_lastEff = 0;
static ULONGLONG g_tearNext = 40;
static ULONGLONG g_tearUntil = 0;
static int g_tearCount = 0;
static struct TearBand { int y; int h; int dx; } g_tears[3];
static ULONGLONG g_noiseNext = 0;
static int g_noiseCount = 0;
static struct NoiseSeg { int x; int len; int y; double k; } g_noiseSegs[60];
static ULONGLONG g_rollNext = 180;
static ULONGLONG g_rollUntil = 0;
static int g_ghostDx = 3;
static std::mt19937 g_glitchRng((unsigned int)std::time(nullptr) ^ (unsigned int)GetTickCount());

static RECT g_btnCloseRect = { 0,0,0,0 };
static RECT g_btnMinRect = { 0,0,0,0 };
static const int HEADER_H = 52;

static LoadedProject g_currentProject;
static bool g_projectLoaded = false;
static wstring g_singleFilePath;
static bool g_pendingSingleFileBrowse = false;

// ============================================================================
// PART B (v3.4+): project-reference-aware symbol protection
// ============================================================================
// Everything in this section is a general mechanism over whatever a project
// file actually declares -- no package id, assembly name, type name, or
// member name is ever hardcoded anywhere below. Correctness comes from
// reading the project's own metadata (B1), resolving the packages it
// actually references, and reflecting their actual public surface (B2); a
// package this has never seen before is handled exactly like one it has.
// ============================================================================

// ----------------------------------------------------------------------------
// B1 -- generic project reference reader (no MSBuild). Parsed the same way
// LoadFromProject above parses <Compile Include=".."> -- strip BOM + XML
// comments, then regex over the raw text. Nothing here is specific to any
// package: PackageReference/Reference/ProjectReference are matched purely by
// their XML shape.
// ----------------------------------------------------------------------------
struct PackageRef {
    wstring id;
    wstring version;
};

struct ProjectReferenceInfo {
    vector<PackageRef> packages;       // <PackageReference Include=".." Version=".."/>
    vector<wstring>    hintPathDlls;   // <Reference><HintPath>..</HintPath></Reference>
    vector<wstring>    bareReferenceNames; // <Reference Include="X"/> with no HintPath --
    // framework/GAC assemblies referenced by simple name (classic WPF/WinForms
    // projects commonly reference PresentationFramework, WindowsBase, System.Windows.Forms
    // etc. exactly this way, with no path at all for the resolver to follow).
    vector<wstring>    targetFrameworks; // <TargetFramework>/<TargetFrameworks>
    // <ProjectReference Include=".."/> is deliberately NOT collected anywhere
    // here -- per B1 it points at the user's own code (another project in the
    // same solution), not a third-party dependency, regardless of what that
    // project happens to be named.
};

static ProjectReferenceInfo ParseProjectReferences(const wstring& projPath) {
    ProjectReferenceInfo info;
    ifstream ifs(projPath.c_str(), ios::binary);
    if (!ifs) return info;
    string u((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    ifs.close();
    if (u.size() >= 3 && (unsigned char)u[0] == 0xEF && (unsigned char)u[1] == 0xBB && (unsigned char)u[2] == 0xBF)
        u.erase(0, 3);
    {
        // Strip XML comments first, same as LoadFromProject, so a
        // commented-out <PackageReference> is never mistaken for a real one.
        string cleaned; size_t i = 0;
        while (i < u.size()) {
            if (i + 3 < u.size() && u[i] == '<' && u[i + 1] == '!' && u[i + 2] == '-' && u[i + 3] == '-') {
                size_t end = u.find("-->", i + 4);
                if (end == string::npos) { cleaned += u.substr(i); break; }
                i = end + 3;
            }
            else { cleaned += u[i]; i++; }
        }
        u = cleaned;
    }
    wstring wu = stringToWstring(u);

    try {
        // Attribute order on <PackageReference> is not fixed (Version before
        // or after Include, extra attributes like PrivateAssets in between),
        // so Include/Version are each matched independently within the
        // element's own attribute text rather than assuming one fixed order.
        // attrInclude/attrVersion each need a custom raw-string delimiter: their
        // content ends "...)\"" (capture group close immediately followed by a
        // literal quote), which is exactly the empty-delimiter raw-string
        // terminator "")\"\"", so an empty-delimiter LR"(...)" closes early right
        // there. Same bug/fix as removeRe/includeRe/projRe fixed earlier.
        wregex pkgRe(LR"(<PackageReference\b([^>]*?)/?>)", regex::icase);
        wregex attrInclude(LR"ATR(\bInclude\s*=\s*"([^"]+)")ATR", regex::icase);
        wregex attrVersion(LR"ATR(\bVersion\s*=\s*"([^"]+)")ATR", regex::icase);
        for (wsregex_iterator it(wu.begin(), wu.end(), pkgRe), end; it != end; ++it) {
            wstring attrs = (*it)[1].str();
            wsmatch m1, m2;
            if (!regex_search(attrs, m1, attrInclude)) continue;
            PackageRef pr;
            pr.id = trimW(m1[1].str());
            if (regex_search(attrs, m2, attrVersion)) pr.version = trimW(m2[1].str());
            if (!pr.id.empty()) info.packages.push_back(pr);
        }
    }
    catch (...) {}

    try {
        // <Reference Include="Some.Dll"><HintPath>..\x\Some.dll</HintPath></Reference>
        // and the self-closing/body form with no HintPath at all --
        // <Reference Include="PresentationFramework" /> -- which is how a
        // classic (non-SDK-style) WPF/WinForms project references its UI
        // framework assemblies: by simple name, resolved from the GAC/
        // framework at build time, never from a path this file could read.
        wregex refOpenRe(LR"(<Reference\b([^>]*?)(/?)>([\s\S]*?(?=<Reference\b|$)))", regex::icase);
        wregex attrInclude2(LR"ATR(\bInclude\s*=\s*"([^",]+))ATR", regex::icase); // name only, drop any ", Version=.." suffix
        wregex hintRe(LR"(<HintPath>\s*([^<]+?)\s*</HintPath>)", regex::icase);
        wregex closeRe(LR"(</Reference>)", regex::icase);
        for (wsregex_iterator it(wu.begin(), wu.end(), refOpenRe), end; it != end; ++it) {
            wstring attrs = (*it)[1].str();
            bool selfClosed = (*it)[2].str() == L"/";
            wstring rest = (*it)[3].str();
            // For the non-self-closing form, only look inside this element's
            // own body (up to its matching </Reference>), not into whatever
            // the next <Reference> in the file happens to contain.
            if (!selfClosed) {
                wsmatch cm;
                if (regex_search(rest, cm, closeRe)) rest = rest.substr(0, cm.position(0));
                else rest.clear(); // unterminated -- nothing reliable to read
            }
            else {
                rest.clear();
            }
            wsmatch m;
            if (regex_search(rest, m, hintRe)) { info.hintPathDlls.push_back(trimW(m[1].str())); continue; }
            wsmatch im;
            if (regex_search(attrs, im, attrInclude2)) {
                wstring name = trimW(im[1].str());
                if (!name.empty()) info.bareReferenceNames.push_back(name);
            }
        }
    }
    catch (...) {}

    try {
        wregex tfmRe(LR"(<TargetFrameworks?>\s*([^<]+?)\s*</TargetFrameworks?>)", regex::icase);
        for (wsregex_iterator it(wu.begin(), wu.end(), tfmRe), end; it != end; ++it) {
            wstring piece = (*it)[1].str();
            size_t st = 0;
            for (size_t i = 0; i <= piece.size(); i++) {
                if (i != piece.size() && piece[i] != L';') continue;
                wstring item = trimW(piece.substr(st, i - st));
                st = i + 1;
                if (!item.empty()) info.targetFrameworks.push_back(item);
            }
        }
    }
    catch (...) {}

    return info;
}

// Resolves a PackageReference to the .dll(s) it restores to in the per-user
// NuGet cache: %USERPROFILE%\.nuget\packages\<id-lowercase>\<version>\lib\<tfm>\*.dll
// When the project's TargetFramework(s) are known, only lib\<tfm> folders that
// match one of them are scanned; otherwise (or if none match -- e.g. the
// package only ships a differently-named moniker) every lib\* folder for that
// package/version is unioned instead of guessing a single one. Under-scanning
// a package is what causes a rename to break the build, so this errs toward
// scanning more of the package's own declared surface, never less.
static vector<wstring> ResolveNuGetPackageDlls(const wstring& id, const wstring& version,
    const vector<wstring>& tfms) {
    vector<wstring> dlls;
    wchar_t home[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableW(L"USERPROFILE", home, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return dlls;
    wstring idLower = toLowerW(id);
    wstring pkgDir = wstring(home) + L"\\.nuget\\packages\\" + idLower + L"\\" + version + L"\\lib\\";
    if (!dirExistsW(pkgDir)) return dlls;

    auto collectDir = [&](const wstring& dir) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((dir + L"*.dll").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                dlls.push_back(dir + fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        };

    bool matchedAny = false;
    if (!tfms.empty()) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((pkgDir + L"*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                for (auto& t : tfms) {
                    if (iequals(fd.cFileName, t)) { collectDir(pkgDir + wstring(fd.cFileName) + L"\\"); matchedAny = true; break; }
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    if (!matchedAny) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((pkgDir + L"*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                collectDir(pkgDir + wstring(fd.cFileName) + L"\\");
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    sort(dlls.begin(), dlls.end());
    dlls.erase(unique(dlls.begin(), dlls.end()), dlls.end());
    return dlls;
}

// ----------------------------------------------------------------------------
// B2 -- generic symbol extractor. Reflects over whatever DLLs B1/the resolver
// handed it, using the exact same BindingFlags combination and
// GetExportedTypes+GetMembers approach already used for the system/NuGet-wide
// scan in GrabberWorker further down (Public|Instance|Static). It is
// deliberately "dumb": every public type name and every public member name
// found is emitted, with no per-name filtering of any kind. Correctness comes
// from *which DLLs* were handed to it, never from any judgment made in here
// about what a given package or type "should" contain.
// ----------------------------------------------------------------------------
struct ProjectSymbolExtractor {
    // PowerShell emits diagnostic records with a private prefix. The native
    // callback routes those to the application's log while keeping ordinary
    // output lines as symbol names.
    static void HandleOutputLine(const wstring& line, set<wstring>& out, const wchar_t* stage) {
        static const wstring kLogPrefix = L"##OBF_LOG##";
        if (line.compare(0, kLogPrefix.size(), kLogPrefix) == 0) {
            LogMessage(wstring(L"  PowerShell [") + stage + L"] " + line.substr(kLogPrefix.size()), CLR_TEXT_DIM);
        }
        else if (!line.empty()) {
            // All symbol records are explicitly tagged. Any untagged output
            // is PowerShell diagnostic text (including stderr/error records).
            LogMessage(wstring(L"  PowerShell [") + stage + L"] output: " + line, CLR_WARN);
        }
    }

    static set<wstring> Extract(const vector<wstring>& dlls) {
        set<wstring> out;
        if (dlls.empty()) return out;
        LogMessage(L"  PowerShell: preparing reflection list for " + to_wstring(dlls.size()) + L" DLL(s).", CLR_INFO);
        wstring listPath = writeTempListFile(dlls);
        wstringstream s;
        s << L"$files = Get-Content -LiteralPath '" << listPath << L"'; $i = 0; $total = $files.Count;"
            L"foreach ($p in $files) { $i++; if (-not $p) { continue };"
            L"  Write-Output ('##OBF_LOG##[' + $i + '/' + $total + '] Loading ' + [IO.Path]::GetFileName($p));"
            L"  try { $asm = [System.Reflection.Assembly]::LoadFrom($p);"
            L"    Write-Output ('##OBF_LOG##Loaded assembly ' + $asm.GetName().Name);"
            L"    $types = $asm.GetExportedTypes();"
            L"    Write-Output ('##OBF_LOG##Enumerating ' + $types.Count + ' exported type(s)');"
            L"    foreach ($t in $types) {"
            L"      if ($t.Name) { Write-Output ('##OBF_SYM##' + $t.Name) };"
            L"      try {"
            L"        $flags = [System.Reflection.BindingFlags]::Public -bor [System.Reflection.BindingFlags]::NonPublic -bor [System.Reflection.BindingFlags]::Instance -bor [System.Reflection.BindingFlags]::Static;"
            L"        foreach ($m in $t.GetMembers($flags)) {"
            L"          if ($m.Name -and ($m.IsPublic -or $m.IsFamily -or $m.IsFamilyOrAssembly)) { Write-Output ('##OBF_SYM##' + $m.Name) }"
            L"        }"
            L"      } catch { Write-Output ('##OBF_LOG##Member enumeration warning for ' + $t.FullName + ': ' + $_.Exception.Message) }"
            L"    }"
            L"    Write-Output ('##OBF_LOG##Finished ' + [IO.Path]::GetFileName($p));"
            L"  } catch { Write-Output ('##OBF_LOG##ERROR loading ' + [IO.Path]::GetFileName($p) + ': ' + $_.Exception.Message) }"
            L"}";
        wstring args = L"-NoProfile -ExecutionPolicy Bypass -Command \"" + s.str() + L"\"";
        runPowerShell(args, [&](const wstring& l) {
            static const wstring symPrefix = L"##OBF_SYM##";
            if (l.compare(0, symPrefix.size(), symPrefix) == 0) out.insert(l.substr(symPrefix.size()));
            else HandleOutputLine(l, out, L"DLL reflection");
            });
        DeleteFileW(listPath.c_str());
        LogMessage(L"  PowerShell: DLL reflection emitted " + to_wstring(out.size()) + L" unique symbol(s).", CLR_INFO);
        return out;
    }

    // For <Reference><Include="X"/> with no HintPath: resolve by assembly name.
    static set<wstring> ExtractByName(const vector<wstring>& names) {
        set<wstring> out;
        if (names.empty()) return out;
        LogMessage(L"  PowerShell: preparing reflection for " + to_wstring(names.size()) + L" framework/name reference(s).", CLR_INFO);
        wstring listPath = writeTempListFile(names);
        wstringstream s;
        s << L"$names = Get-Content -LiteralPath '" << listPath << L"'; $i = 0; $total = $names.Count;"
            L"foreach ($n in $names) { $i++; if (-not $n) { continue };"
            L"  Write-Output ('##OBF_LOG##[' + $i + '/' + $total + '] Resolving ' + $n);"
            L"  $asm = $null;"
            L"  try { $asm = [System.Reflection.Assembly]::Load($n) } catch {"
            L"    try { $asm = [System.Reflection.Assembly]::LoadWithPartialName($n) } catch {} }"
            L"  if (-not $asm) { Write-Output ('##OBF_LOG##WARNING could not resolve ' + $n); continue };"
            L"  Write-Output ('##OBF_LOG##Loaded assembly ' + $asm.GetName().Name);"
            L"  try { $types = $asm.GetExportedTypes();"
            L"    Write-Output ('##OBF_LOG##Enumerating ' + $types.Count + ' exported type(s)');"
            L"    foreach ($t in $types) {"
            L"      if ($t.Name) { Write-Output ('##OBF_SYM##' + $t.Name) };"
            L"      try {"
            L"        $flags = [System.Reflection.BindingFlags]::Public -bor [System.Reflection.BindingFlags]::NonPublic -bor [System.Reflection.BindingFlags]::Instance -bor [System.Reflection.BindingFlags]::Static;"
            L"        foreach ($m in $t.GetMembers($flags)) {"
            L"          if ($m.Name -and ($m.IsPublic -or $m.IsFamily -or $m.IsFamilyOrAssembly)) { Write-Output ('##OBF_SYM##' + $m.Name) }"
            L"        }"
            L"      } catch { Write-Output ('##OBF_LOG##Member enumeration warning for ' + $t.FullName + ': ' + $_.Exception.Message) }"
            L"    }"
            L"  } catch { Write-Output ('##OBF_LOG##ERROR enumerating ' + $n + ': ' + $_.Exception.Message) }"
            L"}";
        wstring args = L"-NoProfile -ExecutionPolicy Bypass -Command \"" + s.str() + L"\"";
        runPowerShell(args, [&](const wstring& l) {
            static const wstring symPrefix = L"##OBF_SYM##";
            if (l.compare(0, symPrefix.size(), symPrefix) == 0) out.insert(l.substr(symPrefix.size()));
            else HandleOutputLine(l, out, L"framework reflection");
            });
        DeleteFileW(listPath.c_str());
        LogMessage(L"  PowerShell: framework reflection emitted " + to_wstring(out.size()) + L" unique symbol(s).", CLR_INFO);
        return out;
    }
};
// ----------------------------------------------------------------------------
// B5 -- cache layout: elements/projects/<project-hash>/{external_symbols.txt,
// known_packages.json}, keyed only by a hash of the project file's own path.
// Nothing here is specific to any project, language, or package.
// ----------------------------------------------------------------------------
static wstring HashProjectPath(const wstring& projPath) {
    // FNV-1a over the lowercased path -- not cryptographic, just needs to be
    // stable and collision-unlikely enough to use as a folder name.
    wstring lower = toLowerW(projPath);
    uint64_t h = 1469598103934665603ULL;
    for (wchar_t c : lower) { h ^= (uint64_t)c; h *= 1099511628211ULL; }
    wchar_t buf[32];
    swprintf_s(buf, L"%016llx", (unsigned long long)h);
    return buf;
}

static wstring ProjectCacheDir(const wstring& projPath) {
    wstring dir = getElementsDir();
    if (dir.empty()) return L"";
    wstring sub = dir + L"projects\\" + HashProjectPath(projPath) + L"\\";
    ensureDir(sub);
    return sub;
}

struct KnownPackageEntry { wstring id, version; bool scanned; };

// known_packages.json has one fixed shape
// ({"packages":[{"id":...,"version":...,"scanned":...}, ...]}), so it is
// read/written with a couple of targeted regexes rather than a general JSON
// parser -- consistent with how the rest of this file reads project XML
// (regex over text, not a full parser).
static wstring JsonEscape(const wstring& s) {
    wstring out;
    for (wchar_t c : s) {
        if (c == L'"' || c == L'\\') out += L'\\';
        out += c;
    }
    return out;
}

static vector<KnownPackageEntry> LoadKnownPackages(const wstring& path) {
    vector<KnownPackageEntry> out;
    if (!fileExistsW(path)) return out;
    ifstream ifs(path.c_str(), ios::binary);
    if (!ifs) return out;
    string u((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    wstring wu = stringToWstring(u);
    try {
        // Same early-termination bug as attrInclude/attrVersion above: the first
        // capture group's content ends "...)\"" which an empty-delimiter raw
        // string reads as its own terminator, truncating everything after it --
        // the leftover tail then gets re-parsed as an ordinary (escape-processing)
        // string, which is where the "\s"/"\{"/"\}" illegal-escape-sequence errors
        // came from. Also cleans up "\s * " / " | " -> "\s*" / "|" -- stray spaces
        // inside what should be plain "\s*"/alternation tokens that don't change
        // what the pattern matches literally but aren't what was intended either.
        wregex objRe(LR"JSN(\{\s*"id"\s*:\s*"((?:[^"\\]|\\.)*)"\s*,\s*"version"\s*:\s*"((?:[^"\\]|\\.)*)"\s*,\s*"scanned"\s*:\s*(true|false)\s*\})JSN");
        for (wsregex_iterator it(wu.begin(), wu.end(), objRe), end; it != end; ++it) {
            KnownPackageEntry e;
            e.id = (*it)[1].str();
            e.version = (*it)[2].str();
            e.scanned = (*it)[3].str() == L"true";
            out.push_back(e);
        }
    }
    catch (...) {}
    return out;
}

static void SaveKnownPackages(const wstring& path, const vector<KnownPackageEntry>& entries) {
    wstringstream s;
    s << L"{\n  \"packages\": [\n";
    for (size_t i = 0; i < entries.size(); i++) {
        auto& e = entries[i];
        s << L"    { \"id\": \"" << JsonEscape(e.id) << L"\", \"version\": \"" << JsonEscape(e.version)
            << L"\", \"scanned\": " << (e.scanned ? L"true" : L"false") << L" }";
        if (i + 1 < entries.size()) s << L",";
        s << L"\n";
    }
    s << L"  ]\n}\n";
    writeTextFileUtf8(path, s.str());
}

// ----------------------------------------------------------------------------
// B3 -- one-time themed prompt per unseen (id, version) pair. Modeled after
// the DecryptorHostPick picker further down (owner-draw-free version, since
// this one needs plain checkboxes rather than a single-select list): a small
// popup window this thread creates and pumps messages for itself, so it works
// whether it's shown from the UI thread or (per spec) the analyze worker
// thread.
// ----------------------------------------------------------------------------
namespace PackagePromptDialog {
    static const wchar_t* kClass = L"VBObfuscatorPkgPrompt";
    static vector<HWND> g_checks;
    static bool g_done = false;
    static bool g_extract = false; // true => "Extract Selected" pressed
    static HBRUSH g_bg = nullptr;

    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, CLR_TEXT);
            SetBkColor(hdc, CLR_BG);
            SetBkMode(hdc, OPAQUE);
            return (LRESULT)g_bg;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == 9001) { g_extract = true;  g_done = true; DestroyWindow(hwnd); return 0; }
            if (id == 9002) { g_extract = false; g_done = true; DestroyWindow(hwnd); return 0; }
            return 0;
        }
        case WM_CLOSE:
            g_extract = false; g_done = true;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            return 0;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    // Shows the modal listing `pkgs` generically (name + version as read from
    // the project file -- never anything hardcoded). On return, checkedOut[i]
    // says whether pkgs[i]'s checkbox was left checked (defaulted checked);
    // the return value says whether "Extract Selected" (vs "Skip All" or
    // closing the window) was the button pressed.
    static bool Show(const vector<PackageRef>& pkgs, vector<bool>& checkedOut) {
        checkedOut.assign(pkgs.size(), true); // "per-package checkboxes defaulted checked"
        if (pkgs.empty()) return false;

        static bool clsDone = false;
        if (!clsDone) {
            WNDCLASS wc = {};
            wc.lpfnWndProc = Proc;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = kClass;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            RegisterClass(&wc);
            clsDone = true;
        }
        if (!g_bg) g_bg = CreateSolidBrush(CLR_BG);

        int rowH = 26;
        int W = 520, headerH = 70, footerH = 56;
        int listH = (int)pkgs.size() * rowH;
        int H = headerH + listH + footerH;

        RECT orc = { 0,0,0,0 };
        if (hWndMain) GetWindowRect(hWndMain, &orc);
        int x = orc.left + ((orc.right - orc.left) - W) / 2;
        int y = orc.top + ((orc.bottom - orc.top) - H) / 2;
        if (x < 0) x = 0; if (y < 0) y = 0;

        g_done = false; g_extract = false; g_checks.clear();
        HWND hwnd = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, kClass,
            L"New package references found", WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_CLIPCHILDREN,
            x, y, W, H, hWndMain, nullptr, GetModuleHandle(nullptr), nullptr);
        if (!hwnd) return false;

        HWND lbl = CreateWindow(L"STATIC",
            L"These packages haven't been scanned yet. Extracting their public\n"
            L"members protects them from renaming; skipping may break the build.",
            WS_CHILD | WS_VISIBLE, 16, 16, W - 32, 40, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        SendMessage(lbl, WM_SETFONT, (WPARAM)hFontUI, TRUE);

        for (size_t i = 0; i < pkgs.size(); i++) {
            wstring label = pkgs[i].id + (pkgs[i].version.empty() ? L"" : (L"  " + pkgs[i].version));
            HWND chk = CreateWindow(L"BUTTON", label.c_str(),
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                16, headerH + (int)i * rowH, W - 32, rowH - 2, hwnd,
                (HMENU)(INT_PTR)(9100 + i), GetModuleHandle(nullptr), nullptr);
            SendMessage(chk, WM_SETFONT, (WPARAM)hFontUI, TRUE);
            SendMessage(chk, BM_SETCHECK, BST_CHECKED, 0);
            g_checks.push_back(chk);
        }

        HWND btnExtract = CreateWindow(L"BUTTON", L"Extract Selected",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, W - 320, H - footerH + 10, 150, 30,
            hwnd, (HMENU)(INT_PTR)9001, GetModuleHandle(nullptr), nullptr);
        HWND btnSkip = CreateWindow(L"BUTTON", L"Skip All",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, W - 160, H - footerH + 10, 140, 30,
            hwnd, (HMENU)(INT_PTR)9002, GetModuleHandle(nullptr), nullptr);
        SendMessage(btnExtract, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        SendMessage(btnSkip, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        SetForegroundWindow(hwnd);
        if (hWndMain) EnableWindow(hWndMain, FALSE);
        MSG m;
        while (!g_done && GetMessage(&m, nullptr, 0, 0) > 0) {
            TranslateMessage(&m);
            DispatchMessage(&m);
        }
        if (hWndMain) { EnableWindow(hWndMain, TRUE); SetForegroundWindow(hWndMain); }

        for (size_t i = 0; i < g_checks.size(); i++)
            checkedOut[i] = (SendMessage(g_checks[i], BM_GETCHECK, 0, 0) == BST_CHECKED);
        g_checks.clear();
        return g_extract;
    }
}

// ----------------------------------------------------------------------------
// B3/B4 orchestrator. Diffs the project's current PackageReference list
// against known_packages.json by (id, version) only -- never against any
// hardcoded allow/deny list -- prompts once per unseen pair, extracts
// whichever the user leaves checked, and (re)loads the full protected set
// into g_generatedProjectSymbols. Called automatically on every project-mode
// analyze run, and again, unchanged, by the "Re-scan project references"
// button (B4) -- a re-scan is just another run of this same function, so any
// reference added or changed since the last run is picked up the same way.
// Returns the exact-count summary line B4 reports, or "" if `proj` has no
// project file to read (Folder mode).
// ----------------------------------------------------------------------------
static wstring RunProjectReferenceProtection(const LoadedProject& proj) {
    if (proj.projectFiles.empty()) { g_generatedProjectSymbols.clear(); return L""; }

    const ULONGLONG protectionStart = GetTickCount64();
    LogMessage(L"Reference protection: starting scan of " + to_wstring(proj.projectFiles.size()) + L" project file(s).", CLR_INFO);

    int scannedCount = 0, newSymbolCount = 0, unprotectedCount = 0;
    set<wstring> combined;

    for (const wstring& projPath : proj.projectFiles) {
        const ULONGLONG projectStart = GetTickCount64();
        LogMessage(L"  References: reading " + baseNameOf(projPath) + L"...", CLR_TEXT_DIM);
        ProjectReferenceInfo refInfo = ParseProjectReferences(projPath);
        LogMessage(L"  References: found " + to_wstring(refInfo.packages.size()) + L" NuGet package(s), " +
            to_wstring(refInfo.hintPathDlls.size()) + L" HintPath DLL(s), and " +
            to_wstring(refInfo.bareReferenceNames.size()) + L" framework/name reference(s).", CLR_TEXT_DIM);
        wstring cacheDir = ProjectCacheDir(projPath);
        if (cacheDir.empty()) {
            LogMessage(L"  References: cache directory unavailable; skipping " + baseNameOf(projPath) + L".", CLR_WARN);
            continue;
        }
        wstring knownPath = cacheDir + L"known_packages.json";
        wstring symbolsPath = cacheDir + L"external_symbols.txt";

        vector<KnownPackageEntry> known = LoadKnownPackages(knownPath);
        set<wstring> cachedSymbols; // whatever B2 already extracted on a prior run
        loadFlatSymbolFile(symbolsPath, cachedSymbols);
        LogMessage(L"  References: cache contains " + to_wstring(cachedSymbols.size()) + L" symbol(s); " +
            to_wstring(known.size()) + L" package decision(s) loaded.", CLR_TEXT_DIM);

        auto findKnown = [&](const wstring& id, const wstring& version) -> KnownPackageEntry* {
            for (auto& k : known) if (iequals(k.id, id) && k.version == version) return &k;
            return nullptr;
            };

        vector<PackageRef> unseen;
        for (auto& pr : refInfo.packages) if (!findKnown(pr.id, pr.version)) unseen.push_back(pr);

        set<wstring> freshlyExtracted;
        if (!unseen.empty()) {
            LogMessage(L"  References: " + to_wstring(unseen.size()) + L" new package(s) need a protection decision.", CLR_INFO);
            vector<bool> checked;
            PackagePromptDialog::Show(unseen, checked);
            vector<wstring> toExtractDlls;
            for (size_t i = 0; i < unseen.size(); i++) {
                KnownPackageEntry e;
                e.id = unseen[i].id;
                e.version = unseen[i].version;
                e.scanned = checked[i];
                known.push_back(e);
                if (checked[i]) {
                    for (auto& d : ResolveNuGetPackageDlls(e.id, e.version, refInfo.targetFrameworks))
                        toExtractDlls.push_back(d);
                }
                else {
                    LogMessage(L"WARNING: members of " + e.id + L" " + e.version +
                        L" are not protected; renaming may break the build or runtime.", CLR_WARN);
                }
            }
            if (!toExtractDlls.empty()) {
                LogMessage(L"  References: reflecting " + to_wstring(toExtractDlls.size()) + L" NuGet DLL(s) with PowerShell; this may take a while...", CLR_INFO);
                const ULONGLONG extractStart = GetTickCount64();
                freshlyExtracted = ProjectSymbolExtractor::Extract(toExtractDlls);
                LogMessage(L"  References: NuGet reflection finished in " + to_wstring((GetTickCount64() - extractStart) / 1000) +
                    L"s; collected " + to_wstring(freshlyExtracted.size()) + L" symbol(s).", CLR_TEXT_DIM);
            }
            else if (!unseen.empty()) {
                LogMessage(L"  References: no NuGet DLLs selected for reflection.", CLR_TEXT_DIM);
            }
            SaveKnownPackages(knownPath, known);
        }

        for (auto& k : known) { if (k.scanned) scannedCount++; else unprotectedCount++; }

        set<wstring> fileTotal = cachedSymbols;
        size_t beforeFresh = fileTotal.size();
        fileTotal.insert(freshlyExtracted.begin(), freshlyExtracted.end());
        newSymbolCount += (int)(fileTotal.size() - beforeFresh);

        // <Reference><HintPath>..</HintPath></Reference> entries are plain
        // local DLLs, not NuGet packages -- never prompted or cached in
        // known_packages.json, just reflected and folded in every run, same
        // as any other dependency this project declares.
        vector<wstring> hintDlls;
        for (auto& rel : refInfo.hintPathDlls) {
            wstring full = rel;
            for (auto& c : full) if (c == L'/') c = L'\\';
            if (!(full.size() > 1 && full[1] == L':')) full = parentDirOf(projPath) + full;
            if (fileExistsW(full)) hintDlls.push_back(full);
        }
        if (!hintDlls.empty()) {
            LogMessage(L"  References: reflecting " + to_wstring(hintDlls.size()) + L" HintPath DLL(s) with PowerShell...", CLR_INFO);
            const ULONGLONG hintStart = GetTickCount64();
            set<wstring> hintSyms = ProjectSymbolExtractor::Extract(hintDlls);
            LogMessage(L"  References: HintPath reflection finished in " + to_wstring((GetTickCount64() - hintStart) / 1000) +
                L"s; collected " + to_wstring(hintSyms.size()) + L" symbol(s).", CLR_TEXT_DIM);
            size_t beforeHint = fileTotal.size();
            fileTotal.insert(hintSyms.begin(), hintSyms.end());
            newSymbolCount += (int)(fileTotal.size() - beforeHint);
        }

        // Bare <Reference Include="X"/> entries (no HintPath) -- classic WPF/
        // WinForms framework assemblies (PresentationFramework, WindowsBase,
        // System.Windows.Forms, ...) referenced by simple name. Same
        // every-run treatment as hintDlls: never prompted or cached in
        // known_packages.json, since these aren't third-party package
        // decisions, just this project's own declared framework surface.
        if (!refInfo.bareReferenceNames.empty()) {
            LogMessage(L"  References: resolving/reflection of " + to_wstring(refInfo.bareReferenceNames.size()) + L" framework/name reference(s) with PowerShell...", CLR_INFO);
            const ULONGLONG bareStart = GetTickCount64();
            set<wstring> bareSyms = ProjectSymbolExtractor::ExtractByName(refInfo.bareReferenceNames);
            LogMessage(L"  References: framework/name reflection finished in " + to_wstring((GetTickCount64() - bareStart) / 1000) +
                L"s; collected " + to_wstring(bareSyms.size()) + L" symbol(s).", CLR_TEXT_DIM);
            size_t beforeBare = fileTotal.size();
            fileTotal.insert(bareSyms.begin(), bareSyms.end());
            newSymbolCount += (int)(fileTotal.size() - beforeBare);
        }

        writeSymbolSet(symbolsPath, fileTotal);
        combined.insert(fileTotal.begin(), fileTotal.end());
        LogMessage(L"  References: " + baseNameOf(projPath) + L" complete (" + to_wstring(fileTotal.size()) +
            L" cached/protected symbols; " + to_wstring((GetTickCount64() - projectStart) / 1000) + L"s).", CLR_TEXT_DIM);
    }

    g_generatedProjectSymbols = std::move(combined);
    LogMessage(L"Reference protection: finished in " + to_wstring((GetTickCount64() - protectionStart) / 1000) +
        L"s; " + to_wstring(g_generatedProjectSymbols.size()) + L" combined symbol(s).", CLR_INFO);

    return L"Scanned " + to_wstring(scannedCount) + L" packages, " +
        to_wstring(newSymbolCount) + L" new symbols protected, " +
        to_wstring(unprotectedCount) + L" packages unprotected.";
}

static int CardX() { return 24; }
static int CardW() { return 620; }
static int ColAX() { return 24; }
static int ColAW() { return (g_clientW - 64) * 55 / 100; }
static int ColBX() { return ColAX() + ColAW() + 16; }
static int ColBW() { return g_clientW - ColBX() - 24; }
static int RightX() { return ColBX(); }
static int RightW() { return ColBW(); }
static int CardInner() { return 20; }

Lang GetSelectedLang() {
    int sel = (int)SendMessage(hComboLang, CB_GETCURSEL, 0, 0);
    switch (sel) { case 1: return Lang::CSharp; case 2: return Lang::Cpp; default: return Lang::VB; }
}

void InitFonts() {
    int dpi = GetDeviceCaps(GetDC(nullptr), LOGPIXELSY);
    auto sz = [&](int pt) { return -MulDiv(pt, dpi, 72); };
    hFontUI = CreateFont(sz(9), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
    hFontSmall = CreateFont(sz(8), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
    hFontBold = CreateFont(sz(9), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
    hFontTitle = CreateFont(sz(15), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
    hFontSection = CreateFont(sz(8), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
    hFontMono = CreateFont(sz(9), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
    hFontMonoSmall = CreateFont(sz(8), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
    hFontTiny = CreateFont(sz(7), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
}

void LogMessage(const wchar_t* msg, COLORREF color) { LogMessage(wstring(msg ? msg : L""), color); }

void LogMessage(const wstring& msg, COLORREF color) {
    if (!hLog) return;
    wchar_t ts[32]; SYSTEMTIME st; GetLocalTime(&st);
    swprintf_s(ts, L"%02d:%02d:%02d  ", st.wHour, st.wMinute, st.wSecond);

    int len = GetWindowTextLength(hLog);
    SendMessage(hLog, EM_SETSEL, len, len);

    CHARFORMAT2 cf = {}; cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
    cf.crTextColor = CLR_TEXT_DIM;
    cf.yHeight = 160;
    wcscpy_s(cf.szFaceName, L"Consolas");
    SendMessage(hLog, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessage(hLog, EM_REPLACESEL, 0, (LPARAM)ts);

    len = GetWindowTextLength(hLog);
    SendMessage(hLog, EM_SETSEL, len, len);
    cf.crTextColor = color;
    SendMessage(hLog, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessage(hLog, EM_REPLACESEL, 0, (LPARAM)msg.c_str());

    len = GetWindowTextLength(hLog);
    SendMessage(hLog, EM_SETSEL, len, len);
    SendMessage(hLog, EM_REPLACESEL, 0, (LPARAM)L"\r\n");
    SendMessage(hLog, EM_SCROLLCARET, 0, 0);
}

void UpdateCustomTextboxState() {
    BOOL c = (SendMessage(hChkCustom, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(hEditCustom, c);
    if (!c) SetWindowText(hEditCustom, L"");
}

void UpdateStringEncryptionDependents() {
    BOOL strEncOn = (SendMessage(hChkStrEnc, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(hChkInjectDecrypt, strEncOn);
    EnableWindow(hChkSkipEncrypted, strEncOn);
    EnableWindow(hChkStrExcl, strEncOn);
    BOOL exclOn = strEncOn && (SendMessage(hChkStrExcl, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(hEditStrExcl, exclOn);
    if (!exclOn) SetWindowText(hEditStrExcl, L"");
}

void PopulateFileList(const LoadedProject& proj) {
    if (!hEditPath) return;
    SendMessage(hEditPath, LB_RESETCONTENT, 0, 0);
    for (const auto& f : proj.files) {
        SendMessage(hEditPath, LB_ADDSTRING, 0, (LPARAM)f.c_str());
    }
}

void PopulateSingleFileList(const wstring& path) {
    if (!hEditPath) return;
    SendMessage(hEditPath, LB_RESETCONTENT, 0, 0);
    SendMessage(hEditPath, LB_ADDSTRING, 0, (LPARAM)path.c_str());
}

void OnBrowse() {
    wchar_t fn[MAX_PATH] = { 0 };
    Lang lang = GetSelectedLang();
    const wchar_t* filter =
        (lang == Lang::CSharp) ? L"C# (*.cs)\0*.cs\0All\0*.*\0" :
        (lang == Lang::Cpp) ? L"C/C++ (*.cpp;*.h)\0*.cpp;*.cc;*.cxx;*.h;*.hpp\0All\0*.*\0" :
        L"VB (*.vb)\0*.vb\0All\0*.*\0";
    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hWndMain;
    ofn.lpstrFilter = filter; ofn.lpstrFile = fn; ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileName(&ofn)) {
        g_projectLoaded = false;
        g_currentProject = LoadedProject();
        g_singleFilePath = fn;
        PopulateSingleFileList(g_singleFilePath);
        EnableWindow(hBtnObfuscate, TRUE);
        EnableWindow(hBtnRescanRefs, FALSE);
        LogMessage(wstring(L"Selected: ") + fn, CLR_INFO);
    }
}

vector<wstring> ParseCustomNames(HWND hEdit = hEditCustom) {
    vector<wstring> r;
    int len = GetWindowTextLength(hEdit);
    if (len == 0) return r;
    vector<wchar_t> buf(len + 1);
    GetWindowText(hEdit, buf.data(), len + 1);
    wstring text(buf.data()); size_t st = 0;
    while (st < text.length()) {
        size_t c = text.find(L',', st);
        wstring p;
        if (c == wstring::npos) { p = text.substr(st); st = text.length(); }
        else { p = text.substr(st, c - st); st = c + 1; }
        p.erase(0, p.find_first_not_of(L" \t\r\n"));
        p.erase(p.find_last_not_of(L" \t\r\n") + 1);
        if (!p.empty()) r.push_back(p);
    }
    return r;
}

void OnRefreshSymbols() {
    LogMessage(L"=== reloading symbols from .\\elements\\ ===", CLR_ACCENT);
    loadSymbolsFromElements();
    LogSymbolCounts(L"  loaded");
}

void OnGenerateFromSystem() {
    DevEnvCheck env = CheckDevEnvironment();
    if (!env.hasDumpbin) {
        LogMessage(L"=== GENERATE (dev): aborted — dumpbin.exe not found ===", CLR_ERROR);
        SymbolToast::show(L"Can't generate: dumpbin.exe not found.\nInstall Visual Studio / Build Tools with the "
            L"\"Desktop development with C++\" workload, then try again.", L"DEV TOOLS REQUIRED");
        return;
    }
    LogMessage(L"=== GENERATE (dev) — writing to .\\elements\\ ===", CLR_ACCENT);
    LogMessage(L"  dumpbin: " + env.dumpbinPath, CLR_TEXT_DIM);
    if (env.hasSdk) LogMessage(L"  Windows SDK libs: " + env.sdkDir, CLR_TEXT_DIM);
    else {
        LogMessage(L"  WARN: Windows SDK lib folder not found — Win32 .lib symbol scan will be skipped.", CLR_WARN);
        SymbolToast::show(L"Windows SDK libs not found.\nWin32 export/DLL scan will still run, but .lib symbol "
            L"scanning will be skipped.", L"PARTIAL DEV ENVIRONMENT");
    }
    ShowWindow(hProgress, SW_SHOW);
    SendMessage(hProgress, PBM_SETPOS, 0, 0);
    SetWindowText(hStatus, L"generating from system...");
    g_grabberDone.store(false);
    HANDLE th = CreateThread(nullptr, 0, GrabberWorker, nullptr, 0, nullptr);
    int anim = 0;
    while (!g_grabberDone.load()) {
        MSG m;
        while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessage(&m); }
        SendMessage(hProgress, PBM_SETPOS, anim++ % 100, 0);
        DWORD wr = MsgWaitForMultipleObjects(1, &th, FALSE, 100, QS_ALLINPUT);
        if (wr == WAIT_OBJECT_0) break;
    }
    CloseHandle(th);
    SendMessage(hProgress, PBM_SETPOS, 100, 0);
    ShowWindow(hProgress, SW_HIDE);
    loadSymbolsFromElements();
    LogSymbolCounts(L"  reloaded");
    SetWindowText(hStatus, L"generation complete");
}

static vector<pair<wstring, bool>> CollectCheckboxStates() {
    auto chk = [](HWND h) -> bool { return h && SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED; };
    vector<pair<wstring, bool>> v = {
        { L"Exclude class names",          chk(hChkClasses) },
        { L"Exclude control names",        chk(hChkControls) },
        { L"Skip public declarations",     chk(hChkPublic) },
        { L"Exclude custom names",         chk(hChkCustom) },
        { L"Preserve macros",              chk(hChkMacros) },
        { L"Preserve labels",         chk(hChkLabels) },
        { L"Preserve operators",           chk(hChkOperators) },
        { L"Preserve namespaces",          chk(hChkNamespaces) },
        { L"Preserve typedefs",            chk(hChkTypedefs) },
        { L"Preserve templates",           chk(hChkTemplates) },
        { L"Skip preprocessor #if 0 blocks",           chk(hChkPrecomp) },
        { L"Name style: O0o",              chk(hChkO0o) },
        { L"Name style: vWw",              chk(hChkVWV) },
        { L"Name style: I1lL",             chk(hChkI1L) },
        { L"Dead code injection",             chk(hChkDeadCode) },
        { L"Interleave code",              chk(hChkMix) },
        { L"String encryption",              chk(hChkStrEnc) },
        { L"Inject decrypt module",             chk(hChkInjectDecrypt) },
        { L"Skip already-encrypted strings",           chk(hChkSkipEncrypted) },
        { L"Custom string exclusion list",     chk(hChkStrExcl) },
        { L"Skip external members",        chk(hChkSkipExternal) },
    };
    return v;
}

static void LayoutControls(HWND hwnd);

// ============================================================================
// XAML PAIRING (C# only)
// ============================================================================
static const wregex& XamlEventAttrRegex() {
    static const wregex re(LR"XN(\b(?:Click|MouseDown|MouseUp|MouseMove|MouseEnter|MouseLeave|MouseWheel|MouseLeftButtonDown|MouseLeftButtonUp|MouseRightButtonDown|MouseRightButtonUp|PreviewMouseDown|PreviewMouseUp|PreviewMouseMove|PreviewMouseLeftButtonDown|PreviewMouseLeftButtonUp|PreviewMouseRightButtonDown|PreviewMouseRightButtonUp|KeyDown|KeyUp|PreviewKeyDown|PreviewKeyUp|TextInput|PreviewTextInput|GotFocus|LostFocus|GotKeyboardFocus|LostKeyboardFocus|Loaded|Unloaded|Closing|Closed|SizeChanged|TextChanged|SelectionChanged|Checked|Unchecked|DragEnter|DragOver|DragLeave|Drop|ContentRendered|StateChanged|Activated|Deactivated|DataContextChanged|ValueChanged|Initialized|Tapped|DoubleTapped|PointerPressed|PointerMoved|PointerReleased)\s*=\s*"([A-Za-z_]\w*)")XN");
    return re;
}

// Several of the attribute names matched above (Checked, Unchecked, StateChanged,
// ValueChanged, SelectionChanged, ...) are ALSO legitimate XAML routed events, but
// some hand-written XAML sets them directly to a literal boolean instead of wiring
// a handler (e.g. a stray `Checked="True"` where `IsChecked="True"` was meant). The
// regex above can't tell "method name" from "boolean literal" apart -- both match
// `[A-Za-z_]\w*` -- so without this guard a bare True/False gets treated as an
// unresolved/renameable event-handler reference and aborts the whole pairing step.
// XAML's boolean type converter only ever accepts these two spellings, so excluding
// them here is safe and does not risk skipping a real handler name.
static bool IsXamlBooleanLiteral(const wstring& name) {
    return _wcsicmp(name.c_str(), L"True") == 0 || _wcsicmp(name.c_str(), L"False") == 0;
}

static vector<wstring> ExtractXamlDeclaredNames(const wstring& xaml) {
    vector<wstring> names;
    try {
        wregex nameRe(LR"XN((?:^|[\s<])(?:x:Name|Name)\s*=\s*"([A-Za-z_]\w*)")XN");
        for (wsregex_iterator it(xaml.begin(), xaml.end(), nameRe), end; it != end; ++it)
            names.push_back((*it)[1].str());
        for (wsregex_iterator it(xaml.begin(), xaml.end(), XamlEventAttrRegex()), end; it != end; ++it) {
            wstring evName = (*it)[1].str();
            if (IsXamlBooleanLiteral(evName)) continue;
            names.push_back(evName);
        }
    }
    catch (const regex_error&) {
        LogMessage(L"WARN: XAML pairing: name scan failed (regex_error).", CLR_WARN);
    }
    return names;
}

static bool XamlLookupObfName(const wstring& name,
    const map<wstring, wstring, less<>>& classMap,
    const map<wstring, wstring, less<>>& methodMap,
    const map<wstring, wstring, less<>>& fieldMap,
    wstring& outObf) {
    auto itC = classMap.find(name); if (itC != classMap.end()) { outObf = itC->second; return true; }
    auto itM = methodMap.find(name); if (itM != methodMap.end()) { outObf = itM->second; return true; }
    auto itF = fieldMap.find(name); if (itF != fieldMap.end()) { outObf = itF->second; return true; }
    return false;
}

static vector<wstring> FindUnresolvedXamlHandlers(const wstring& xamlOriginal, ObfuscationEngine& engine) {
    vector<wstring> missing;
    set<wstring> seen;
    try {
        for (wsregex_iterator it(xamlOriginal.begin(), xamlOriginal.end(), XamlEventAttrRegex()), end; it != end; ++it) {
            wstring name = (*it)[1].str();
            if (IsXamlBooleanLiteral(name)) continue;
            if (!seen.insert(name).second) continue;
            if (engine.getMethodMap().count(name) || engine.getFieldMap().count(name) || engine.getPreserveSet().count(name)) continue;
            missing.push_back(name);
        }
    }
    catch (const regex_error&) {
        LogMessage(L"WARN: XAML pairing: verification scan failed (regex_error).", CLR_WARN);
    }
    return missing;
}

static wstring BuildXamlVerificationReport(const wstring& xamlOriginal, ObfuscationEngine& engine) {
    wstringstream out;
    set<wstring> seen;
    try {
        for (wsregex_iterator it(xamlOriginal.begin(), xamlOriginal.end(), XamlEventAttrRegex()), end; it != end; ++it) {
            wstring name = (*it)[1].str();
            if (IsXamlBooleanLiteral(name)) continue;
            if (!seen.insert(name).second) continue;
            out << L"  " << name << L": ";
            auto itM = engine.getMethodMap().find(name);
            auto itF = engine.getFieldMap().find(name);
            if (itM != engine.getMethodMap().end()) out << L"renamed (method) -> " << itM->second;
            else if (itF != engine.getFieldMap().end()) out << L"renamed (field) -> " << itF->second;
            else if (engine.getPreserveSet().count(name)) out << L"preserved";
            else out << L"UNRESOLVED";
            out << L"\r\n";
        }
    }
    catch (const regex_error&) {
        LogMessage(L"WARN: XAML pairing: verification report scan failed (regex_error).", CLR_WARN);
    }
    return out.str();
}

static int ReplaceCapturedNamesIfMapped(wstring& text, const wregex& re,
    const function<bool(const wstring&, wstring&)>& lookup) {
    wstring out;
    out.reserve(text.size());
    size_t last = 0;
    int count = 0;
    for (wsregex_iterator it(text.begin(), text.end(), re), end; it != end; ++it) {
        auto& m = *it;
        size_t g1start = (size_t)m.position(1);
        size_t g1end = g1start + (size_t)m.length(1);
        wstring name = m[1].str();
        wstring obfName;
        out.append(text, last, g1start - last);
        if (lookup(name, obfName)) { out += obfName; count++; }
        else out += name;
        last = g1end;
    }
    out.append(text, last, text.size() - last);
    text.swap(out);
    return count;
}

struct XamlPairInfo {
    bool active = false;
    bool rewrite = false;
    wstring xamlPath;
    wstring xamlOriginalText;
};

static XamlPairInfo PrepareXamlPairing(const wstring& csPath, Lang lang, ObfuscationEngine& engine) {
    XamlPairInfo info;
    if (lang != Lang::CSharp) return info;

    wstring xamlPath;
    static const wstring suffix = L".xaml.cs";
    if (csPath.size() > suffix.size() &&
        toLowerW(csPath.substr(csPath.size() - suffix.size())) == suffix) {
        xamlPath = csPath.substr(0, csPath.size() - 3);
    }
    else {
        size_t ls2 = csPath.find_last_of(L"\\/");
        size_t ld2 = csPath.find_last_of(L'.');
        if (ld2 != wstring::npos && ld2 > ls2)
            xamlPath = csPath.substr(0, ld2) + L".xaml";
    }
    if (xamlPath.empty() ||
        GetFileAttributes(xamlPath.c_str()) == INVALID_FILE_ATTRIBUTES) return info;

    wstring fileOnly = xamlPath;
    size_t slashPos = fileOnly.find_last_of(L"\\/");
    if (slashPos != wstring::npos) fileOnly = fileOnly.substr(slashPos + 1);

    wstring prompt =
        L"A matching XAML file was found:\n\n    " + fileOnly +
        L"\n\nCreate a rewritten copy whose x:Name / Name / {Binding} / {x:Bind} / "
        L"{TemplateBinding} references and event-handler attributes (Click=, "
        L"MouseDown=, KeyDown=, etc.) match the obfuscated .cs names?\n\n"
        L"This overwrites " + fileOnly + L" in place (after a backup copy is made "
        L"-- see bak\\).\n\n"
        L"Choose No to instead keep every name declared in the XAML un-obfuscated "
        L"on the C# side, so the existing XAML keeps working unmodified.";
    int choice = MessageBox(hWndMain, prompt.c_str(), L"XAML pairing detected", MB_YESNO | MB_ICONQUESTION);

    ifstream ifs(xamlPath.c_str(), ios::binary);
    if (!ifs) {
        LogMessage(L"WARNING: XAML pairing: could not open \"" + fileOnly + L"\" -- skipped.", CLR_WARN);
        return info;
    }
    string utf8xaml((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    ifs.close();
    if (utf8xaml.size() >= 3 &&
        (unsigned char)utf8xaml[0] == 0xEF &&
        (unsigned char)utf8xaml[1] == 0xBB &&
        (unsigned char)utf8xaml[2] == 0xBF)
        utf8xaml.erase(0, 3);
    wstring xamlText = stringToWstring(utf8xaml);

    info.active = true;
    info.xamlPath = xamlPath;
    info.xamlOriginalText = xamlText;

    if (choice == IDYES) {
        info.rewrite = true;
        LogMessage(L"XAML pairing: \"" + fileOnly + L"\" will be rewritten after obfuscation.", CLR_INFO);
    }
    else {
        vector<wstring> declared = ExtractXamlDeclaredNames(xamlText);
        if (!declared.empty()) {
            engine.addPreserveList(declared);
            LogMessage(L"XAML pairing: preserving " + to_wstring(declared.size()) +
                L" name(s) declared in \"" + fileOnly + L"\" so the XAML keeps matching.", CLR_TEXT_DIM);
        }
    }
    return info;
}

static XamlPairInfo PrepareXamlPairingPreAnswered(const wstring& csPath, Lang lang,
    ObfuscationEngine& engine, bool rewriteChoice) {
    XamlPairInfo info;
    if (lang != Lang::CSharp) return info;
    wstring xamlPath;
    static const wstring suffix = L".xaml.cs";
    if (csPath.size() > suffix.size() &&
        toLowerW(csPath.substr(csPath.size() - suffix.size())) == suffix) {
        xamlPath = csPath.substr(0, csPath.size() - 3);
    }
    else {
        size_t ls2 = csPath.find_last_of(L"\\/");
        size_t ld2 = csPath.find_last_of(L'.');
        if (ld2 != wstring::npos && ld2 > ls2)
            xamlPath = csPath.substr(0, ld2) + L".xaml";
    }
    if (xamlPath.empty() || GetFileAttributes(xamlPath.c_str()) == INVALID_FILE_ATTRIBUTES) return info;

    ifstream ifs(xamlPath.c_str(), ios::binary);
    if (!ifs) return info;
    string utf8xaml((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    ifs.close();
    if (utf8xaml.size() >= 3 &&
        (unsigned char)utf8xaml[0] == 0xEF &&
        (unsigned char)utf8xaml[1] == 0xBB &&
        (unsigned char)utf8xaml[2] == 0xBF)
        utf8xaml.erase(0, 3);
    wstring xamlText = stringToWstring(utf8xaml);

    info.active = true;
    info.xamlPath = xamlPath;
    info.xamlOriginalText = xamlText;

    if (rewriteChoice) {
        info.rewrite = true;
    }
    else {
        vector<wstring> declared = ExtractXamlDeclaredNames(xamlText);
        if (!declared.empty()) engine.addPreserveList(declared);
    }
    return info;
}

// ============================================================================
// §3a: Designer pairing
// ============================================================================
// Single-file counterpart to PrepareDesignerPairing below: that one only
// runs from OnObfuscateProjectWorker, so opening just Builder.cs (one half
// of a partial class split across Builder.cs/Builder.Designer.cs) used to
// be processed as a closed world -- this.button1, this.label1,
// InitializeComponent(), etc. are declared only in the unseen designer
// half, so fieldMap either never saw them (lookup miss -> "the name '...'
// does not exist") or, if it had stale knowledge some other way, renamed
// this.X while the designer file -- never touched -- kept the original
// name, producing the same class of mismatch either way.
//
// Fix: before extractIdentifiers() runs, detect the sibling by the same
// "<Stem>.Designer.cs"/"<Stem>.Designer.vb" naming convention Visual Studio
// uses, and if found, add it to engine.sources so preScan()/
// extractIdentifiers() see its field/control declarations too (both loop
// over ALL sources already). The caller is then responsible for also
// writing the sibling back via engine.obfuscateOne() at the returned
// index, exactly as the project path already does for every source --
// otherwise the two halves would renamed-vs-original mismatch instead of
// missing-vs-missing, which is no better.
static wstring DetectAndAddDesignerSiblingSingleFile(const wstring& primaryPath, Lang lang, ObfuscationEngine& engine) {
    if (lang != Lang::CSharp && lang != Lang::VB) return L"";
    wstring lower = toLowerW(primaryPath);
    wstring designerPath;
    if (endsWithW(lower, L".designer.cs")) designerPath = primaryPath.substr(0, primaryPath.size() - 12) + L".cs";
    else if (endsWithW(lower, L".designer.vb")) designerPath = primaryPath.substr(0, primaryPath.size() - 12) + L".vb";
    else if (endsWithW(lower, L".cs")) {
        wstring cand = primaryPath.substr(0, primaryPath.size() - 3) + L".Designer.cs";
        if (fileExistsW(cand)) designerPath = cand;
    }
    else if (endsWithW(lower, L".vb")) {
        wstring cand = primaryPath.substr(0, primaryPath.size() - 3) + L".Designer.vb";
        if (fileExistsW(cand)) designerPath = cand;
    }
    if (designerPath.empty() || toLowerW(designerPath) == lower || !fileExistsW(designerPath)) return L"";

    ifstream ifs(designerPath.c_str(), ios::binary);
    if (!ifs) return L"";
    string u((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    ifs.close();
    if (u.size() >= 3 && (unsigned char)u[0] == 0xEF && (unsigned char)u[1] == 0xBB && (unsigned char)u[2] == 0xBF) u.erase(0, 3);

    engine.addSource(stringToWstring(u), designerPath, lang);
    LogMessage(L"Designer pairing: \"" + baseNameOf(designerPath) +
        L"\" is the other half of this partial class -- adding it so cross-file "
        L"references (this.button1, InitializeComponent, etc.) resolve correctly. "
        L"It will be rewritten too.", CLR_INFO);
    return designerPath;
}

static vector<wstring> PrepareDesignerPairing(const LoadedProject& proj, ObfuscationEngine& engine) {
    vector<wstring> resxToRewrite;
    set<wstring> seenResx;

    set<wstring> sourceLabelsLower;
    for (auto& s : engine.sources) sourceLabelsLower.insert(toLowerW(s.label));

    vector<wstring> allFiles = proj.files;

    for (auto& f : allFiles) {
        wstring lower = toLowerW(f);
        wstring designerPath, partnerPath, stem, ext;

        static const wstring kDesignerCs = L".designer.cs";
        static const wstring kDesignerVb = L".designer.vb";
        if (lower.size() > kDesignerCs.size() &&
            lower.compare(lower.size() - kDesignerCs.size(), kDesignerCs.size(), kDesignerCs) == 0) {
            designerPath = f;
            stem = f.substr(0, f.size() - kDesignerCs.size());
            ext = L".cs";
        }
        else if (lower.size() > kDesignerVb.size() &&
            lower.compare(lower.size() - kDesignerVb.size(), kDesignerVb.size(), kDesignerVb) == 0) {
            designerPath = f;
            stem = f.substr(0, f.size() - kDesignerVb.size());
            ext = L".vb";
        }

        if (!designerPath.empty()) {
            partnerPath = stem + ext;
        }
        else {
            if (endsWithW(lower, L".cs")) {
                wstring cand = f.substr(0, f.size() - 3) + L".Designer.cs";
                if (fileExistsW(cand)) {
                    designerPath = cand;
                    partnerPath = f;
                    stem = f.substr(0, f.size() - 3);
                    ext = L".cs";
                }
            }
            else if (endsWithW(lower, L".vb")) {
                wstring cand = f.substr(0, f.size() - 3) + L".Designer.vb";
                if (fileExistsW(cand)) {
                    designerPath = cand;
                    partnerPath = f;
                    stem = f.substr(0, f.size() - 3);
                    ext = L".vb";
                }
            }
        }

        if (designerPath.empty()) continue;

        auto addIfMissing = [&](const wstring& path) {
            wstring plower = toLowerW(path);
            if (sourceLabelsLower.count(plower)) return;
            ifstream ifs(path.c_str(), ios::binary);
            if (!ifs) return;
            string u((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
            ifs.close();
            if (u.size() >= 3 && (unsigned char)u[0] == 0xEF && (unsigned char)u[1] == 0xBB && (unsigned char)u[2] == 0xBF)
                u.erase(0, 3);
            engine.addSource(stringToWstring(u), path, proj.lang);
            sourceLabelsLower.insert(plower);
            LogMessage(L"Designer pairing: added missing half \"" + baseNameOf(path) + L"\" to sources.", CLR_INFO);
            };
        addIfMissing(designerPath);
        if (!partnerPath.empty()) addIfMissing(partnerPath);

        ifstream difs(designerPath.c_str(), ios::binary);
        if (difs) {
            string dc((istreambuf_iterator<char>(difs)), istreambuf_iterator<char>());
            difs.close();
            if (dc.size() >= 3 && (unsigned char)dc[0] == 0xEF && (unsigned char)dc[1] == 0xBB && (unsigned char)dc[2] == 0xBF)
                dc.erase(0, 3);
            wstring dtext = stringToWstring(dc);

            wregex fieldDeclRe;
            if (ext == L".vb") {
                fieldDeclRe = wregex(LR"XN(\b(?:Friend|Private|Protected|Public|Dim)\s+(?:WithEvents\s+)?([A-Za-z_]\w*)\s+As\s+[A-Za-z_])XN", regex::icase);
            }
            else {
                fieldDeclRe = wregex(LR"XN(\b(?:private|protected|public|internal)\s+(?:static\s+)?[A-Za-z_][\w.]*\s+([A-Za-z_]\w*)\s*;)XN");
            }

            for (wsregex_iterator it(dtext.begin(), dtext.end(), fieldDeclRe), end; it != end; ++it) {
                wstring name = (*it)[1].str();
                if (name.empty()) continue;
                engine.addPreserveList({ name });
                LogMessage(L"Designer pairing: preserving control field \"" + name + L"\" (" +
                    baseNameOf(designerPath) + L").", CLR_TEXT_DIM);
            }
        }

        wstring resxPath = stem + L".resx";
        if (fileExistsW(resxPath)) {
            wstring rlower = toLowerW(resxPath);
            if (seenResx.insert(rlower).second) resxToRewrite.push_back(resxPath);
        }
    }
    return resxToRewrite;
}

// ============================================================================
// §3b: RewriteResxInPlace
// ============================================================================
static bool RewriteResxInPlace(const wstring& resxPath, ObfuscationEngine& engine,
    const wstring& bakDir, const wstring& rootDir) {
        {
            wstring rel = StripPrefixRel(resxPath, rootDir);
            wstring bakPath = bakDir + L"\\" + rel;
            if (!fileExistsW(bakPath)) {
                LogMessage(L"RewriteResxInPlace: backup missing for " + resxPath, CLR_ERROR);
                return false;
            }
        }

        ifstream ifs(resxPath.c_str(), ios::binary);
        if (!ifs) { LogMessage(L"WARN: could not open .resx: " + resxPath, CLR_WARN); return false; }
        string utf8((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
        ifs.close();
        if (utf8.size() >= 3 && (unsigned char)utf8[0] == 0xEF && (unsigned char)utf8[1] == 0xBB && (unsigned char)utf8[2] == 0xBF)
            utf8.erase(0, 3);
        wstring text = stringToWstring(utf8);

        auto lookup = [&](const wstring& name, wstring& out) -> bool {
            auto qf = engine.getFieldMap().find(name);
            if (qf != engine.getFieldMap().end()) { out = qf->second; return true; }
            return false;
            };

        int rewrites = 0;
        try {
            wregex resxNameRe(LR"XN(\bname\s*=\s*"([A-Za-z_]\w*)\.([A-Za-z_]\w*)")XN");
            wstring out;
            size_t last = 0;
            for (wsregex_iterator it(text.begin(), text.end(), resxNameRe), end; it != end; ++it) {
                size_t g1pos = (size_t)(*it).position(1);
                wstring comp = (*it)[1].str();
                wstring propPath = (*it)[2].str();
                out.append(text, last, g1pos - last);
                wstring obf;
                if (lookup(comp, obf)) { out += obf; rewrites++; }
                else out += comp;
                out += L".";
                out += propPath;
                last = g1pos + comp.size() + 1 + propPath.size();
            }
            out.append(text, last, text.size() - last);
            text.swap(out);
        }
        catch (const regex_error&) {
            LogMessage(L"WARN: .resx rewrite regex failed", CLR_WARN);
            return false;
        }

        ofstream ofs(resxPath.c_str(), ios::binary);
        if (!ofs) { LogMessage(L"WARN: could not write .resx: " + resxPath, CLR_WARN); return false; }
        ofs << wstringToString(text);
        ofs.close();
        LogMessage(L".resx pairing: wrote " + to_wstring(rewrites) + L" reference(s) to " + baseNameOf(resxPath), CLR_SUCCESS);
        return true;
}

// ============================================================================
// §4b: RewriteUiInPlace
// ============================================================================
static bool RewriteUiInPlace(const wstring& uiPath, ObfuscationEngine& engine) {
    ifstream ifs(uiPath.c_str(), ios::binary);
    if (!ifs) { LogMessage(L"WARN: could not open .ui file: " + baseNameOf(uiPath), CLR_WARN); return false; }
    string utf8((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    ifs.close();
    if (utf8.size() >= 3 && (unsigned char)utf8[0] == 0xEF && (unsigned char)utf8[1] == 0xBB && (unsigned char)utf8[2] == 0xBF)
        utf8.erase(0, 3);
    wstring text = stringToWstring(utf8);

    auto lookup = [&](const wstring& name, wstring& out) -> bool {
        if (!engine.getLocalFields().count(name) &&
            !engine.getLocalMethods().count(name) &&
            !engine.getLocalClasses().count(name)) return false;
        auto qf = engine.getFieldMap().find(name); if (qf != engine.getFieldMap().end()) { out = qf->second; return true; }
        auto qm = engine.getMethodMap().find(name); if (qm != engine.getMethodMap().end()) { out = qm->second; return true; }
        auto qc = engine.getClassMap().find(name); if (qc != engine.getClassMap().end()) { out = qc->second; return true; }
        return false;
        };

    int rewrites = 0;
    try {
        wregex widgetNameRe(LR"XN(<(?:widget|action|layout)\b[^>]*?\bname\s*=\s*"([A-Za-z_]\w*)")XN");
        rewrites += ReplaceCapturedNamesIfMapped(text, widgetNameRe, lookup);

        wregex senderReceiverRe(LR"XN(<(?:sender|receiver)>([A-Za-z_]\w*)</(?:sender|receiver)>)XN");
        rewrites += ReplaceCapturedNamesIfMapped(text, senderReceiverRe, lookup);

        wregex slotSignalRe(LR"XN(<(?:slot|signal)>([A-Za-z_]\w*)\(\s*\)</(?:slot|signal)>)XN");
        rewrites += ReplaceCapturedNamesIfMapped(text, slotSignalRe, lookup);

        wregex classRe(LR"XN(<class>([A-Za-z_]\w*)</class>)XN");
        rewrites += ReplaceCapturedNamesIfMapped(text, classRe, lookup);
    }
    catch (const regex_error&) {
        LogMessage(L"WARN: .ui rewrite regex failed", CLR_WARN);
        return false;
    }

    ofstream ofs(uiPath.c_str(), ios::binary);
    if (!ofs) { LogMessage(L"WARN: could not write .ui file: " + baseNameOf(uiPath), CLR_WARN); return false; }
    ofs << wstringToString(text);
    ofs.close();
    LogMessage(L".ui pairing: wrote " + to_wstring(rewrites) + L" reference(s) to " + baseNameOf(uiPath), CLR_SUCCESS);
    return true;
}

static bool RewriteXamlToMatch(const XamlPairInfo& xamlPair, ObfuscationEngine& engine) {
    wstring rewritten = xamlPair.xamlOriginalText;

    function<bool(const wstring&, wstring&)> lookup = [&](const wstring& name, wstring& obf) -> bool {
        // XAML contains many framework property/event names in the same syntactic
        // positions as application-owned names (Setter.Property, Binding paths,
        // TemplateBinding, etc.). Never rewrite a name already classified as a
        // framework/API symbol, even if a local symbol map happens to contain a
        // same-spelled identifier. This deliberately prefers preserving an
        // ambiguous framework name over producing invalid WPF/XAML.
        if (isFrameworkMember(name)) return false;
        return XamlLookupObfName(name, engine.getClassMap(), engine.getMethodMap(), engine.getFieldMap(), obf);
        };

    int replaced = 0;
    try {
        wregex nameAttrRe(LR"XN((?:^|[\s<])(?:x:Name|Name)\s*=\s*"([A-Za-z_]\w*)")XN");
        wregex templateBindingRe(LR"(\{TemplateBinding\s+([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*))");
        auto replaceDottedPath = [&](const wregex& re) -> int {
            wstring out; size_t last = 0; int cnt = 0;
            for (wsregex_iterator it(rewritten.begin(), rewritten.end(), re), end; it != end; ++it) {
                size_t gs = (size_t)(*it).position(1);
                wstring pathName = (*it)[1].str();
                out.append(rewritten, last, gs - last);
                size_t st = 0;
                for (size_t q = 0; q <= pathName.size(); q++) {
                    if (q != pathName.size() && pathName[q] != L'.') continue;
                    wstring comp = pathName.substr(st, q - st);
                    st = q + 1;
                    wstring obf;
                    if (lookup(comp, obf)) { out += obf; cnt++; }
                    else out += comp;
                    if (q != pathName.size()) out += L'.';
                }
                last = gs + pathName.size();
            }
            out.append(rewritten, last, rewritten.size() - last);
            rewritten.swap(out);
            return cnt;
            };
        // Same idea as replaceDottedPath, but tolerant of the parenthesized
        // PropertyPath syntax WPF uses for attached-property targets, e.g.
        // Property="(local:Foo.Bar)" on a Setter, or the (Type.Prop) / chained
        // (Type.Prop).(Type2.Prop2) form Storyboard.TargetProperty uses. '(' and
        // ')' are passed through unchanged; only the identifier segments between
        // them (and between dots) get looked up. For a plain, unparenthesized
        // path this behaves exactly like replaceDottedPath.
        auto replacePropertyPath = [&](const wregex& re) -> int {
            wstring out; size_t last = 0; int cnt = 0;
            for (wsregex_iterator it(rewritten.begin(), rewritten.end(), re), end; it != end; ++it) {
                size_t gs = (size_t)(*it).position(1);
                wstring pathName = (*it)[1].str();
                out.append(rewritten, last, gs - last);
                size_t st = 0;
                for (size_t q = 0; q <= pathName.size(); q++) {
                    bool atSep = (q == pathName.size()) ||
                        pathName[q] == L'.' || pathName[q] == L'(' || pathName[q] == L')';
                    if (!atSep) continue;
                    wstring comp = pathName.substr(st, q - st);
                    st = q + 1;
                    if (!comp.empty()) {
                        wstring obf;
                        if (lookup(comp, obf)) { out += obf; cnt++; }
                        else out += comp;
                    }
                    if (q != pathName.size()) out += pathName[q];
                }
                last = gs + pathName.size();
            }
            out.append(rewritten, last, rewritten.size() - last);
            rewritten.swap(out);
            return cnt;
            };
        replaced += ReplaceCapturedNamesIfMapped(rewritten, nameAttrRe, lookup);
        replaced += replaceDottedPath(templateBindingRe);
        replaced += ReplaceCapturedNamesIfMapped(rewritten, XamlEventAttrRegex(), lookup);
        replaced += ReplaceCapturedNamesIfMapped(rewritten,
            wregex(LR"XN((?:ElementName|Storyboard\.TargetName)\s*=\s*"([A-Za-z_]\w*)")XN"), lookup);
        replaced += ReplaceCapturedNamesIfMapped(rewritten,
            wregex(LR"XN(AncestorType\s*=\s*([A-Za-z_]\w*))XN"), lookup);
        // TargetType/DataType, markup-extension form: TargetType="{x:Type local:Foo}"
        replaced += ReplaceCapturedNamesIfMapped(rewritten,
            wregex(LR"XN(\{x:Type\s+(?:[A-Za-z_]\w*:)?([A-Za-z_]\w*)\s*\})XN"), lookup);
        // TargetType/DataType, bare attribute form: TargetType="local:Foo" / DataType="Foo"
        replaced += ReplaceCapturedNamesIfMapped(rewritten,
            wregex(LR"XN((?:TargetType|DataType)\s*=\s*"(?:[A-Za-z_]\w*:)?([A-Za-z_]\w*)")XN"), lookup);
        // Setter's Property= value -- covers a plain DP name ("IsEnabled"), an
        // attached-property reference ("local:Foo.Bar"), AND the parenthesized
        // PropertyPath form WPF requires for attached properties and for
        // Storyboard.TargetProperty ("(local:Foo.Bar)", or the chained
        // "(Type.Prop).(Type2.Prop2)"). The old version only matched the bare
        // dotted form -- a parenthesized value didn't match the regex at all,
        // so the original (now-renamed-elsewhere) name was left untouched and
        // the markup compiler couldn't resolve it. This is exactly the gap
        // behind "Invalid value for property 'Property'" (XLS0431/XDG0012):
        // a custom attached/DP member renamed on the C# side but left under
        // its original name here because the parens made the match fail.
        replaced += replacePropertyPath(
            wregex(LR"XN(\bProperty\s*=\s*"((?:[A-Za-z_]\w*:)?\(?[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*\)?(?:\.\(?[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*\)?)*)")XN"));
        // Storyboard.TargetProperty (and any other *TargetProperty attribute) --
        // same PropertyPath grammar as above, not handled at all previously.
        replaced += replacePropertyPath(
            wregex(LR"XN(\bTargetProperty\s*=\s*"((?:[A-Za-z_]\w*:)?\(?[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*\)?(?:\.\(?[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*\)?)*)")XN"));
        {
            wregex xClassRe(LR"XN(\bx:Class\s*=\s*"([^"]+)")XN");
            wstring out; size_t last = 0; int cnt = 0;
            for (wsregex_iterator it(rewritten.begin(), rewritten.end(), xClassRe), end; it != end; ++it) {
                size_t gs = (size_t)(*it).position(1);
                wstring full = (*it)[1].str();
                size_t dotPos = full.rfind(L'.');
                wstring cls = (dotPos == wstring::npos) ? full : full.substr(dotPos + 1);
                wstring obf;
                out.append(rewritten, last, gs - last);
                if (lookup(cls, obf)) {
                    out += (dotPos == wstring::npos) ? obf : full.substr(0, dotPos + 1) + obf;
                    cnt++;
                }
                else out += full;
                last = gs + full.size();
            }
            out.append(rewritten, last, rewritten.size() - last);
            rewritten.swap(out);
            replaced += cnt;
        }
        // FIX: every pass above only ever rewrote attribute VALUES. A class
        // used directly as a markup ELEMENT -- a custom control, converter, or
        // behavior instantiated as "<local:MyControl>" / "<local:MyConverter
        // x:Key=\"c\"/>" -- was never touched, so if that class got renamed via
        // classMap the compiled XAML still tried to instantiate the OLD name
        // and failed to resolve the type at compile/runtime. Only prefixed
        // tags ("prefix:Name") are considered: unprefixed tags are always
        // framework controls from the default WPF/UWP namespace (Button,
        // Grid, TextBox, ...), which are never in classMap, so restricting to
        // the prefixed form avoids any chance of touching those. This
        // deliberately checks classMap ONLY (never methodMap/fieldMap) since a
        // tag name can only ever name a type.
        {
            function<bool(const wstring&, wstring&)> lookupClassOnly = [&](const wstring& name, wstring& obf) -> bool {
                auto it = engine.getClassMap().find(name);
                if (it != engine.getClassMap().end()) { obf = it->second; return true; }
                return false;
                };
            replaced += ReplaceCapturedNamesIfMapped(rewritten,
                wregex(LR"XN(</?[A-Za-z_][\w.]*:([A-Za-z_]\w*))XN"), lookupClassOnly);
        }
        replaced += replaceDottedPath(wregex(LR"(\{Binding\s+([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*))"));
        replaced += replaceDottedPath(wregex(LR"(\{x:Bind\s+([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*))"));
    }
    catch (const regex_error&) {
        LogMessage(L"WARNING: XAML pairing: rewrite failed (regex_error) -- nothing written.", CLR_WARN);
        return false;
    }

    vector<wstring> unmappedHandlers;
    try {
        set<wstring> seen;
        for (wsregex_iterator it(rewritten.begin(), rewritten.end(), XamlEventAttrRegex()), end; it != end; ++it) {
            wstring name = (*it)[1].str();
            if (IsXamlBooleanLiteral(name)) continue;
            if (!seen.insert(name).second) continue;

            bool isRenamedValue = false;
            for (auto* m : { &engine.getClassMap(), &engine.getMethodMap(), &engine.getFieldMap() }) {
                for (auto& kv : *m) if (kv.second == name) { isRenamedValue = true; break; }
                if (isRenamedValue) break;
            }
            if (isRenamedValue) continue;
            if (engine.getPreserveSet().count(name)) continue;
            unmappedHandlers.push_back(name);
        }
    }
    catch (const regex_error&) {
        LogMessage(L"WARNING: XAML pairing: verification scan failed (regex_error) -- nothing written.", CLR_WARN);
        return false;
    }
    if (!unmappedHandlers.empty()) {
        wstring list;
        for (size_t i = 0; i < unmappedHandlers.size(); i++) { if (i) list += L", "; list += unmappedHandlers[i]; }
        LogMessage(L"WARNING: XAML pairing: handler attribute(s) neither renamed nor preserved: " +
            list + L". Nothing written.", CLR_WARN);
        return false;
    }

    wstring outPath = xamlPair.xamlPath;
    ofstream xout(outPath.c_str(), ios::binary);
    if (!xout) {
        LogMessage(L"WARNING: XAML pairing: could not write \"" + outPath + L"\".", CLR_WARN);
        return false;
    }
    xout << wstringToString(rewritten);
    xout.close();

    LogMessage(L"XAML pairing: wrote " + to_wstring(replaced) + L" rewritten reference(s) to \"" +
        baseNameOf(outPath) + L"\" (original preserved in bak\\).", CLR_SUCCESS);
    return true;
}

// ============================================================================
// v3.4: DECRYPTOR HOST PICKER -- themed single-select dialog
// ============================================================================
namespace DecryptorHostPick {

    static vector<pair<wstring, wstring>> g_entries;   // (full path, display name)
    static vector<HWND> g_checks;
    static int g_checkedIndex = -1;   // index into g_checks of the checked row, -1 = none
    static HWND g_hwnd = nullptr;
    static wstring g_result;
    static bool g_done = false;
    static int g_scrollY = 0, g_contentH = 0, g_viewH = 0, g_rows = 0;
    static HBRUSH g_bgBrush = nullptr;
    static const int IDC_PICK_BASE = 4000, IDC_PICK_OK = 4998, IDC_PICK_CANCEL = 4999;
    static const int kRowH = 26, kHeaderH = 46, kInstrH = 40, kFooterH = 52, kMaxVisible = 12, kPad = 14;
    static const wchar_t* kPickClass = L"ObfHostPickWindow";
    static RECT g_rcClose = { 0,0,0,0 };

    static void layoutRows() {
        int y0 = kHeaderH + kInstrH;
        int viewTop = y0, viewBottom = y0 + g_viewH;
        HDWP hdwp = BeginDeferWindowPos((int)g_checks.size());
        for (size_t i = 0; i < g_checks.size(); i++) {
            int y = y0 + (int)i * kRowH - g_scrollY;
            bool visible = (y + (kRowH - 4) > viewTop) && (y < viewBottom);
            UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
            hdwp = DeferWindowPos(hdwp, g_checks[i], nullptr, kPad - 4, y, 540, kRowH - 4, flags);
        }
        if (hdwp) EndDeferWindowPos(hdwp);
    }

    static void drawCheckbox(LPDRAWITEMSTRUCT dis) {
        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;
        HBRUSH bg = CreateSolidBrush(CLR_BG);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        int rowIdx = (int)(dis->CtlID - IDC_PICK_BASE);
        bool checked = (rowIdx == g_checkedIndex);
        int box = 15;
        int bx = rc.left + 4;
        int by = rc.top + ((rc.bottom - rc.top) - box) / 2;
        RECT brc = { bx, by, bx + box, by + box };
        HBRUSH bb = CreateSolidBrush(checked ? CLR_SEG_ON : CLR_BTN_PRIMARY);
        FillRect(hdc, &brc, bb);
        DeleteObject(bb);
        HPEN pen = CreatePen(PS_SOLID, 1, checked ? CLR_ACCENT : CLR_BORDER_HI);
        HGDIOBJ op = SelectObject(hdc, pen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, brc.left, brc.top, brc.right, brc.bottom);
        if (checked) {
            HPEN cp = CreatePen(PS_SOLID, 2, CLR_BG);
            SelectObject(hdc, cp);
            MoveToEx(hdc, bx + 3, by + 7, nullptr);
            LineTo(hdc, bx + 6, by + 11);
            LineTo(hdc, bx + 12, by + 3);
            DeleteObject(cp);
        }
        SelectObject(hdc, op);
        DeleteObject(pen);

        wchar_t text[512] = {};
        GetWindowTextW(dis->hwndItem, text, 511);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, checked ? CLR_TEXT_BRIGHT : CLR_TEXT);
        SelectObject(hdc, hFontUI);
        RECT trc = { rc.left + 28, rc.top, rc.right - 4, rc.bottom };
        DrawTextW(hdc, text, -1, &trc, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    static void drawButton(LPDRAWITEMSTRUCT dis) {
        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;
        bool primary = (dis->CtlID == IDC_PICK_OK);
        HBRUSH bb = CreateSolidBrush(primary ? CLR_BTN_ACTION : CLR_BTN_PRIMARY);
        FillRect(hdc, &rc, bb);
        DeleteObject(bb);
        HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER_HI);
        HGDIOBJ op = SelectObject(hdc, pen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, op);
        DeleteObject(pen);
        wchar_t text[64] = {};
        GetWindowTextW(dis->hwndItem, text, 63);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, primary ? CLR_TEXT_BRIGHT : CLR_TEXT);
        SelectObject(hdc, hFontBold);
        DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }

    static LRESULT CALLBACK PickWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_ERASEBKGND: {
            RECT rc; GetClientRect(hwnd, &rc);
            if (!g_bgBrush) g_bgBrush = CreateSolidBrush(CLR_BG);
            FillRect((HDC)wp, &rc, g_bgBrush);
            return 1;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);
            if (!g_bgBrush) g_bgBrush = CreateSolidBrush(CLR_BG);
            FillRect(hdc, &rc, g_bgBrush);
            // header bar
            HBRUSH hb = CreateSolidBrush(CLR_CARD_TOP);
            RECT hr = { 0, 0, rc.right, kHeaderH };
            FillRect(hdc, &hr, hb);
            DeleteObject(hb);
            HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER_HI);
            HGDIOBJ op = SelectObject(hdc, pen);
            MoveToEx(hdc, 0, kHeaderH, nullptr);
            LineTo(hdc, rc.right, kHeaderH);
            SelectObject(hdc, op);
            DeleteObject(pen);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, CLR_ACCENT);
            SelectObject(hdc, hFontSection);
            RECT tr = { 14, 0, rc.right - 44, kHeaderH };
            DrawTextW(hdc, L"SELECT DECRYPTOR HOST FILE", -1, &tr, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            // close X
            GetClientRect(hwnd, &rc);
            g_rcClose = { rc.right - 36, 12, rc.right - 12, 36 };
            SetTextColor(hdc, CLR_ERROR);
            SelectObject(hdc, hFontBold);
            DrawTextW(hdc, L"X", -1, &g_rcClose, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            // instruction
            SetTextColor(hdc, CLR_TEXT_DIM);
            SelectObject(hdc, hFontSmall);
            RECT ir = { kPad, kHeaderH, rc.right - kPad, kHeaderH + kInstrH };
            DrawTextW(hdc,
                L"Pick ONE existing file that will hold the public mxcncbs decryptor class.\r\n"
                L"All other project files will reference it there -- no standalone file is created.",
                -1, &ir, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wp;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, CLR_TEXT_DIM);
            if (!g_bgBrush) g_bgBrush = CreateSolidBrush(CLR_BG);
            return (LRESULT)g_bgBrush;
        }
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lp;
            if (dis->CtlType != ODT_BUTTON) break;
            if (dis->CtlID >= IDC_PICK_BASE && dis->CtlID < IDC_PICK_BASE + 1000) drawCheckbox(dis);
            else drawButton(dis);
            return TRUE;
        }
        case WM_VSCROLL: {
            int maxTop = g_contentH - g_viewH;
            int old = g_scrollY;
            switch (LOWORD(wp)) {
            case SB_LINEUP: g_scrollY -= kRowH; break;
            case SB_LINEDOWN: g_scrollY += kRowH; break;
            case SB_PAGEUP: g_scrollY -= g_viewH; break;
            case SB_PAGEDOWN: g_scrollY += g_viewH; break;
            case SB_THUMBTRACK: g_scrollY = HIWORD(wp); break;
            }
            if (g_scrollY < 0) g_scrollY = 0;
            if (g_scrollY > maxTop) g_scrollY = maxTop;
            if (g_scrollY != old) {
                SetScrollPos(hwnd, SB_VERT, g_scrollY, TRUE);
                layoutRows();
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int d = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            SendMessage(hwnd, WM_VSCROLL, d > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            if (PtInRect(&g_rcClose, pt)) { g_result.clear(); g_done = true; return 0; }
            if (pt.y < kHeaderH) {
                POINT scr; GetCursorPos(&scr);
                ReleaseCapture();
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(scr.x, scr.y));
                return 0;
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) { g_result.clear(); g_done = true; return 0; }
            break;
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == IDC_PICK_OK) {
                g_result.clear();
                if (g_checkedIndex >= 0 && g_checkedIndex < (int)g_entries.size()) {
                    g_result = g_entries[g_checkedIndex].first;
                }
                if (g_result.empty()) {
                    MessageBox(hwnd, L"Check one file first (only one can be checked).",
                        L"OBFUSCATOR", MB_ICONINFORMATION);
                    return 0;
                }
                g_done = true;
                return 0;
            }
            if (id == IDC_PICK_CANCEL) { g_result.clear(); g_done = true; return 0; }
            if (id >= IDC_PICK_BASE && id < IDC_PICK_BASE + 1000 && HIWORD(wp) == BN_CLICKED) {
                // The button's own check state is unreliable for an owner-draw
                // button of this style, so we track which row is checked ourselves.
                int rowIdx = id - IDC_PICK_BASE;
                int oldIndex = g_checkedIndex;
                g_checkedIndex = (g_checkedIndex == rowIdx) ? -1 : rowIdx;
                // Invalidating the parent does not cascade to child button windows,
                // so explicitly repaint the old and new checkbox rows.
                if (oldIndex >= 0 && oldIndex < (int)g_checks.size())
                    InvalidateRect(g_checks[oldIndex], nullptr, TRUE);
                if (g_checkedIndex >= 0 && g_checkedIndex < (int)g_checks.size())
                    InvalidateRect(g_checks[g_checkedIndex], nullptr, TRUE);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            g_result.clear(); g_done = true;
            return 0;
        case WM_DESTROY:
            return 0;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    // Returns the chosen project file, or L"" if the user cancelled.
    static wstring PromptDecryptorHostFile(const LoadedProject& proj, Lang lang) {
        g_entries.clear();
        for (const auto& f : proj.files) {
            wstring disp = StripPrefixRel(f, proj.rootDir);
            g_entries.push_back({ f, disp });
        }
        if (g_entries.empty()) return L"";

        static bool clsDone = false;
        if (!clsDone) {
            WNDCLASS wc = {};
            wc.lpfnWndProc = PickWndProc;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = kPickClass;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            RegisterClass(&wc);
            clsDone = true;
        }
        if (!g_bgBrush) g_bgBrush = CreateSolidBrush(CLR_BG);

        g_rows = (int)std::min(g_entries.size(), (size_t)kMaxVisible);
        int W = 560;
        int listH = g_rows * kRowH;
        int H = kHeaderH + kInstrH + listH + kFooterH;
        g_contentH = (int)g_entries.size() * kRowH;
        g_viewH = listH;
        g_scrollY = 0;
        g_result.clear();
        g_done = false;
        g_checks.clear();
        g_checkedIndex = -1;

        // center over the main window
        RECT orc; GetWindowRect(hWndMain, &orc);
        int x = orc.left + ((orc.right - orc.left) - W) / 2;
        int y = orc.top + ((orc.bottom - orc.top) - H) / 2;
        if (x < 0) x = 0; if (y < 0) y = 0;

        HWND hwnd = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, kPickClass,
            L"Decryptor host", WS_POPUP | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN,
            x, y, W, H, hWndMain, nullptr, GetModuleHandle(nullptr), nullptr);
        if (!hwnd) return L"";
        g_hwnd = hwnd;

        for (size_t i = 0; i < g_entries.size(); i++) {
            HWND chk = CreateWindow(L"BUTTON", g_entries[i].second.c_str(),
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                0, 0, 540, kRowH - 4, hwnd, (HMENU)(INT_PTR)(IDC_PICK_BASE + i),
                GetModuleHandle(nullptr), nullptr);
            SendMessage(chk, WM_SETFONT, (WPARAM)hFontUI, TRUE);
            g_checks.push_back(chk);
        }
        if (g_contentH > g_viewH) {
            SCROLLINFO si = { sizeof(si), SIF_RANGE | SIF_PAGE, 0, g_contentH - 1, (UINT)g_viewH };
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        }
        else {
            ShowScrollBar(hwnd, SB_VERT, FALSE);
        }
        layoutRows();

        HWND ok = CreateWindow(L"BUTTON", L"OK",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            W - 200, H - kFooterH + 10, 88, 30, hwnd, (HMENU)(INT_PTR)IDC_PICK_OK,
            GetModuleHandle(nullptr), nullptr);
        HWND cancel = CreateWindow(L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            W - 104, H - kFooterH + 10, 88, 30, hwnd, (HMENU)(INT_PTR)IDC_PICK_CANCEL,
            GetModuleHandle(nullptr), nullptr);
        SendMessage(ok, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        SendMessage(cancel, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        SetForegroundWindow(hwnd);
        EnableWindow(hWndMain, FALSE);
        MSG m;
        while (!g_done && GetMessage(&m, nullptr, 0, 0) > 0) {
            TranslateMessage(&m);
            DispatchMessage(&m);
        }
        EnableWindow(hWndMain, TRUE);
        SetForegroundWindow(hWndMain);
        DestroyWindow(hwnd);
        g_hwnd = nullptr;
        for (HWND h : g_checks) (void)h;
        g_checks.clear();
        return g_result;
    }

} // namespace DecryptorHostPick

// ============================================================================
// ON-OBFUSCATE (single file)
// ============================================================================
// Runs on a background thread (spawned by OnObfuscateSingleFile below) so the
// UI message loop keeps pumping -- repainting, accepting input, animating the
// log/progress bar -- while the regex-heavy obfuscation pass runs. The Win32
// calls below (SendMessage/SetWindowText/EnableWindow/MessageBox) are all
// safe to call from a non-UI thread: Windows marshals them onto the thread
// that owns the window and blocks the caller until they're processed, so no
// extra synchronization is needed here beyond the g_obfuscationBusy flag that
// keeps a second run from starting while this one is still in flight.
static void OnObfuscateSingleFileWorker(wstring path, wstring bakDir) {
    ShowWindow(hProgress, SW_SHOW);
    SendMessage(hProgress, PBM_SETPOS, 0, 0);
    SetWindowText(hLog, L"");
    resetLayerHits();

    g_logCollapsed = false;
    g_logAnimT = 0.0;
    ShowWindow(hBtnShowLog, SW_HIDE);
    if (hWndMain) LayoutControls(hWndMain);

    Lang lang = GetSelectedLang();
    RunLogInfo runLog;
    runLog.sourcePath = path;
    runLog.backupPath = bakDir;
    runLog.lang = lang;
    runLog.checkboxStates = CollectCheckboxStates();
    {
        wchar_t cb[128] = {};
        int sel = (int)SendMessage(hCmbCount, CB_GETCURSEL, 0, 0);
        if (sel >= 0) SendMessage(hCmbCount, CB_GETLBTEXT, sel, (LPARAM)cb);
        runLog.deadCodeCountSel = cb;
    }

    try {
        LogMessage(wstring(L">> target language: ") + langName(lang), CLR_INFO);

        ifstream ifs(path.c_str(), ios::binary);
        if (!ifs) throw runtime_error("cannot open file");
        string utf8src((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
        ifs.close();
        if (utf8src.size() >= 3 &&
            (unsigned char)utf8src[0] == 0xEF &&
            (unsigned char)utf8src[1] == 0xBB &&
            (unsigned char)utf8src[2] == 0xBF)
            utf8src.erase(0, 3);

        wstring source = stringToWstring(utf8src);
        LogMessage(L"  loaded " + to_wstring(source.size()) + L" chars", CLR_TEXT_DIM);
        SendMessage(hProgress, PBM_SETPOS, 20, 0);

        ObfuscationEngine engine(source, lang);
        engine.excludeClassNames = (SendMessage(hChkClasses, BM_GETCHECK, 0, 0) == BST_CHECKED);
        engine.excludeControlNames = (SendMessage(hChkControls, BM_GETCHECK, 0, 0) == BST_CHECKED); // now supports VB, C#, and (where applicable) C++ control-field detection
        engine.excludePublicDeclarations = (lang != Lang::Cpp) && (SendMessage(hChkPublic, BM_GETCHECK, 0, 0) == BST_CHECKED);
        if (lang == Lang::Cpp) {
            engine.preserveMacros = (SendMessage(hChkMacros, BM_GETCHECK, 0, 0) == BST_CHECKED);
            engine.preserveLabels = (SendMessage(hChkLabels, BM_GETCHECK, 0, 0) == BST_CHECKED);
            engine.preserveOperators = (SendMessage(hChkOperators, BM_GETCHECK, 0, 0) == BST_CHECKED);
            engine.preserveNamespaces = (SendMessage(hChkNamespaces, BM_GETCHECK, 0, 0) == BST_CHECKED);
            engine.preserveTypedefs = (SendMessage(hChkTypedefs, BM_GETCHECK, 0, 0) == BST_CHECKED);
            engine.preserveTemplates = (SendMessage(hChkTemplates, BM_GETCHECK, 0, 0) == BST_CHECKED);
            engine.skipPreprocessorZero = (SendMessage(hChkPrecomp, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }

        if (SendMessage(hChkVWV, BM_GETCHECK, 0, 0) == BST_CHECKED) engine.setCharset(ObfuscationCharset::vwv);
        else if (SendMessage(hChkI1L, BM_GETCHECK, 0, 0) == BST_CHECKED) engine.setCharset(ObfuscationCharset::I1lL);
        else engine.setCharset(ObfuscationCharset::O0o);

        // "Skip external members" now applies to VB, C#, and C++. For VB it
        // still gates the Me/MyBase/MyClass/local-class receiver check in
        // processLineVB (see skipExternalMembers usage there). For C#/C++ the
        // equivalent protection (never rename a member unless the receiver is
        // provably "this" or a locally-declared class) is already always-on
        // in the dot-access rewrite, so setting this flag has no additional
        // effect there yet -- it's wired through regardless so the checkbox
        // behaves consistently rather than silently doing nothing per language.
        bool wantSkipExternal = (SendMessage(hChkSkipExternal, BM_GETCHECK, 0, 0) == BST_CHECKED);
        bool wantDeadCode = (SendMessage(hChkDeadCode, BM_GETCHECK, 0, 0) == BST_CHECKED);
        bool wantStrEnc = (SendMessage(hChkStrEnc, BM_GETCHECK, 0, 0) == BST_CHECKED);
        engine.skipExternalMembers = wantSkipExternal;
        engine.enableDeadCode = wantDeadCode;
        engine.deadCodeCount = wantDeadCode ? (((int)SendMessage(hCmbCount, CB_GETCURSEL, 0, 0) + 1) * 3) : 0;
        engine.mixDeadCode = wantDeadCode && (SendMessage(hChkMix, BM_GETCHECK, 0, 0) == BST_CHECKED);
        engine.enableStringEncryption = wantStrEnc;
        engine.shouldInjectDecryptModule = wantStrEnc && (SendMessage(hChkInjectDecrypt, BM_GETCHECK, 0, 0) == BST_CHECKED);
        engine.excludeAlreadyEncrypted = wantStrEnc && (SendMessage(hChkSkipEncrypted, BM_GETCHECK, 0, 0) == BST_CHECKED);
        engine.enableStringExclusion = wantStrEnc && (SendMessage(hChkStrExcl, BM_GETCHECK, 0, 0) == BST_CHECKED);
        if (engine.enableStringExclusion) engine.stringExclusionList = ParseCustomNames(hEditStrExcl);

        wstring designerSiblingPath = DetectAndAddDesignerSiblingSingleFile(path, lang, engine);

        auto pre = engine.preScan();
        if (!pre.classNames.empty()) {
            wstring s = L"Detected classes: ";
            for (size_t i = 0; i < pre.classNames.size(); i++) { if (i) s += L", "; s += pre.classNames[i]; }
            LogMessage(s, CLR_TEXT_DIM);
        }
        if (!pre.controlNames.empty()) {
            wstring s = L"Detected controls: ";
            for (size_t i = 0; i < pre.controlNames.size(); i++) { if (i) s += L", "; s += pre.controlNames[i]; }
            LogMessage(s, CLR_TEXT_DIM);
        }
        if (!pre.publicNames.empty()) {
            wstring s = L"Detected public declarations: ";
            for (size_t i = 0; i < pre.publicNames.size(); i++) { if (i) s += L", "; s += pre.publicNames[i]; }
            LogMessage(s, CLR_TEXT_DIM);
        }
        if (SendMessage(hChkCustom, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            auto cn = ParseCustomNames();
            if (!cn.empty()) { engine.addPreserveList(cn); LogMessage(L"Preserving " + to_wstring(cn.size()) + L" custom names.", CLR_TEXT_DIM); }
        }
        if (lang == Lang::VB || lang == Lang::CSharp) {
            engine.addPreserveList({
                L"Main", L"InitializeComponent", L"Dispose", L"ToString", L"Equals", L"GetHashCode",
                L"GetType", L"MemberwiseClone", L"Finalize", L"New", L"Me", L"MyBase", L"MyClass",
                L"Value", L"Text", L"Name", L"Tag", L"Enabled", L"Visible", L"Checked", L"Selected",
                L"Items", L"Controls", L"Components", L"Add", L"AddRange", L"Clear", L"Remove",
                L"RemoveAt", L"Insert", L"Contains", L"IndexOf", L"Count", L"Length", L"Capacity",
                L"ToArray", L"ToList", L"ToDictionary", L"Select", L"Where", L"OrderBy", L"GroupBy",
                L"First", L"FirstOrDefault", L"Single", L"SingleOrDefault", L"Last", L"LastOrDefault",
                L"Any", L"All", L"Distinct", L"Skip", L"Take", L"Reverse", L"Concat", L"Union",
                L"Intersect", L"Except", L"Sum", L"Max", L"Min", L"Average",
                L"GetEnumerator", L"MoveNext", L"Current", L"Reset"
                });
        }
        else {
            engine.addPreserveList({
                L"main", L"WinMain", L"wWinMain", L"DllMain",
                L"begin", L"end", L"cbegin", L"cend", L"rbegin", L"rend",
                L"size", L"empty", L"data", L"front", L"back", L"at",
                L"push_back", L"pop_back", L"push_front", L"pop_front",
                L"insert", L"erase", L"clear", L"find", L"count", L"swap",
                L"operator", L"operator==", L"operator!=", L"operator<<", L"operator>>"
                });
        }

        XamlPairInfo xamlPair = PrepareXamlPairing(wstring(path), lang, engine);
        runLog.xamlPairAttempted = xamlPair.active;

        SendMessage(hProgress, PBM_SETPOS, 40, 0);
        SetWindowText(hStatus, L"extracting identifiers...");
        engine.extractIdentifiers();
        LogMessage(L"Found " + to_wstring(engine.getLocalClasses().size()) + L" classes, " +
            to_wstring(engine.getLocalMethods().size()) + L" methods, " +
            to_wstring(engine.getLocalFields().size()) + L" fields.", CLR_TEXT_DIM);

        SendMessage(hProgress, PBM_SETPOS, 70, 0);
        SetWindowText(hStatus, L"obfuscating...");
        wstring result = engine.obfuscate();
        result = engine.patchReflectionStrings(result);
        result = engine.patchResolvedLabels(result);

        if (xamlPair.active) {
            runLog.xamlVerificationReport = BuildXamlVerificationReport(xamlPair.xamlOriginalText, engine);
            vector<wstring> unresolved = FindUnresolvedXamlHandlers(xamlPair.xamlOriginalText, engine);
            if (!unresolved.empty()) {
                wstring list;
                for (size_t i = 0; i < unresolved.size(); i++) { if (i) list += L", "; list += unresolved[i]; }
                runLog.xamlPairSucceeded = false;
                runLog.xamlPairError = L"handler(s) not resolved by obfuscation: " + list;
                LogMessage(L"ABORT: XAML pairing failed verification — handler(s) not resolved: " + list +
                    L" — re-run with XAML pairing = No.", CLR_ERROR);
                throw runtime_error("XAML pairing: rewrite verification failed — re-run with XAML pairing = No");
            }
        }

        if (xamlPair.active && xamlPair.rewrite) {
            bool xamlOk = RewriteXamlToMatch(xamlPair, engine);
            runLog.xamlPairSucceeded = xamlOk;
            if (xamlOk) runLog.xamlOutputPath = xamlPair.xamlPath;
            else {
                runLog.xamlPairError = L"rewrite verification failed — re-run with XAML pairing set to No (preserve XAML-declared names)";
                LogMessage(L"ABORT: XAML pairing failed verification — re-run with XAML pairing = No.", CLR_ERROR);
                throw runtime_error("XAML pairing: rewrite verification failed — re-run with XAML pairing set to No");
            }
        }
        else if (xamlPair.active) {
            runLog.xamlPairSucceeded = true;
        }

        SendMessage(hProgress, PBM_SETPOS, 80, 0);

        if (engine.enableStringEncryption) {
            result = engine.encryptStringsVB(result);
            if (engine.stringEncryptionLog.empty())
                LogMessage(L"WARNING: string encryption produced 0 replacements — "
                    L"method detection failed; output NOT encrypted.", CLR_ERROR);
            else if (engine.shouldInjectDecryptModule)
                result = engine.injectDecryptModule(result);
        }
        if (engine.enableDeadCode) {
            SetWindowText(hStatus, L"injecting dead code...");
            result = engine.injectDeadCode(result);
            SendMessage(hProgress, PBM_SETPOS, 95, 0);
        }

        wstring outPath = wstring(path);

        ofstream ofs(outPath.c_str(), ios::binary);
        if (!ofs) throw runtime_error("cannot write output file");
        ofs << wstringToString(result); ofs.close();

        if (!designerSiblingPath.empty()) {
            wstring designerResult = engine.obfuscateOne(1);
            designerResult = engine.patchReflectionStrings(designerResult);
            designerResult = engine.patchResolvedLabels(designerResult);
            ofstream dofs(designerSiblingPath.c_str(), ios::binary);
            if (!dofs) {
                LogMessage(L"WARNING: could not write paired designer file \"" +
                    baseNameOf(designerSiblingPath) + L"\" -- it was backed up but NOT rewritten; "
                    L"its names will no longer match " + baseNameOf(outPath) + L".", CLR_WARN);
            }
            else {
                dofs << wstringToString(designerResult); dofs.close();
                LogMessage(L"Designer pairing: also rewrote \"" + baseNameOf(designerSiblingPath) + L"\".", CLR_SUCCESS);
            }
        }

        SendMessage(hProgress, PBM_SETPOS, 100, 0);
        LogMessage(L"obfuscation complete!", CLR_SUCCESS);
        LogMessage(L"output: " + outPath, CLR_INFO);

        {
            int t = g_totalLookups.load(), me = g_hitMembers.load(), ff = g_hitFreeFn.load();
            int w32 = g_hitWin32.load(), tp = g_hitThirdParty.load(), ho = g_hitHeaderOnly.load(), ne = g_hitNet.load(), ms = g_hitMiss.load();
            LogMessage(L"=== layer hit report ===", CLR_ACCENT);
            LogMessage(L"  total=" + to_wstring(t) + L" protected=" + to_wstring(me + ff + w32 + tp + ho + ne) + L" missed=" + to_wstring(ms), CLR_TEXT_DIM);
            LogMessage(L"  reserved=" + to_wstring(me) + L" freefn=" + to_wstring(ff) + L" win32=" + to_wstring(w32) +
                L" tp=" + to_wstring(tp) + L" ho=" + to_wstring(ho) + L" net=" + to_wstring(ne), CLR_INFO);
        }
        SetWindowText(hStatus, L"done.");

        if (runLog.xamlPairAttempted && !runLog.xamlPairSucceeded) {
            throw runtime_error("XAML pairing: did not succeed");
        }

        runLog.success = true;
        runLog.outputPath = outPath;
        runLog.stringEncryptionDone = engine.enableStringEncryption && !engine.stringEncryptionLog.empty();
        runLog.stringPairs = engine.stringEncryptionLog;
        runLog.classRenames = (int)engine.getClassMap().size();
        runLog.methodRenames = (int)engine.getMethodMap().size();
        runLog.fieldRenames = (int)engine.getFieldMap().size();
        WriteRunLogs(runLog);
        LogMessage(L"logged: " + g_lastRunLogFolder, CLR_TEXT_DIM);

        g_logCollapsed = true;
        g_logAnimT = 0.0;
    }
    catch (const exception& ex) {
        LogMessage(L"ERROR: " + stringToWstring(ex.what()), CLR_ERROR);
        SetWindowText(hStatus, L"obfuscation failed.");

        runLog.success = false;
        runLog.errorMsg = stringToWstring(ex.what());
        WriteRunLogs(runLog);
    }
    EnableWindow(hBtnObfuscate, TRUE);
    EnableWindow(hBtnBrowse, TRUE);
    ShowWindow(hProgress, SW_HIDE);
    g_obfuscationBusy.store(false);
}

// Thin, fast, UI-thread entry point: does only the quick up-front checks that
// need to run before anything else (file picked? exists? symbol lists
// loaded? backup made?), then hands the slow part off to a background thread
// so the click handler returns immediately and the window keeps repainting.
void OnObfuscateSingleFile() {
    if (g_obfuscationBusy.exchange(true)) {
        LogMessage(L"WARN: an obfuscation run is already in progress — please wait for it to finish.", CLR_WARN);
        return;
    }

    wchar_t path[MAX_PATH];
    if (g_singleFilePath.empty()) {
        LogMessage(L"ERROR: No file selected.", CLR_ERROR);
        g_obfuscationBusy.store(false);
        return;
    }
    wcsncpy_s(path, g_singleFilePath.c_str(), MAX_PATH - 1);

    if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES) {
        LogMessage(L"ERROR: File does not exist.", CLR_ERROR);
        g_obfuscationBusy.store(false);
        return;
    }

    if (!symbolsFolderHasFiles()) {
        LogMessage(L"ERROR: .\\elements\\ is missing or empty — no symbol-exclusion lists loaded.", CLR_ERROR);
        SymbolToast::show(L"No symbol files in .\\elements\\. Load or generate symbols first.");
        g_obfuscationBusy.store(false);
        return;
    }

    wstring bakDir = PrepareBackupSingleFile(wstring(path));
    if (bakDir.empty()) {
        g_obfuscationBusy.store(false);
        return;
    }

    // Disable immediately, on the UI thread, so a rapid second click (the
    // custom button's WM_LBUTTONUP handler checks IsWindowEnabled) can't
    // race the worker thread for this.
    EnableWindow(hBtnObfuscate, FALSE);
    EnableWindow(hBtnBrowse, FALSE);

    std::thread(OnObfuscateSingleFileWorker, wstring(path), bakDir).detach();
}

// ============================================================================
// §5d (v3.3): OnObfuscateProject -- writes ONE standalone decryptor per run
// ============================================================================
// Stage every transformed source beside its destination before replacing any
// project file. This prevents a regex/transform/write failure halfway through
// the source loop from leaving a mixture of original and obfuscated files.
// If a commit fails after one or more replacements, restore those files from
// the run's pre-obfuscation backup.
// User-reviewed, project-wide whole-identifier replacement.
namespace GlobalRenameReview {
    static const wchar_t* kClass = L"ObfGlobalRenameReview";
    static const int GRR_ID_SEARCH = 7101, GRR_ID_LIST = 7102, GRR_ID_REMOVE = 7103;
    static const int GRR_ID_REPLACE = 7104, GRR_ID_CANCEL = 7105, GRR_ID_STATUS = 7106;
    static HWND g_search = nullptr, g_list = nullptr, g_status = nullptr;
    static bool g_done = false, g_accepted = false;
    // Save the textbox contents before DestroyWindow destroys the child edit control.
    static wstring g_finalText;
    static HBRUSH g_bg = nullptr;

    static wstring ReadControlText(HWND h) {
        int n = GetWindowTextLengthW(h);
        wstring v((size_t)n + 1, L'\0');
        GetWindowTextW(h, &v[0], n + 1);
        v.resize((size_t)n);
        return v;
    }
    static vector<pair<wstring, wstring>> ParseList(const wstring& text) {
        vector<pair<wstring, wstring>> out;
        wistringstream in(text); wstring line;
        while (getline(in, line)) {
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            size_t p = line.find(L" => ");
            if (p == wstring::npos) continue;
            wstring a = line.substr(0, p), b = line.substr(p + 4);
            if (!a.empty() && !b.empty()) out.push_back({ a,b });
        }
        return out;
    }
    static wstring FormatList(const vector<pair<wstring, wstring>>& entries) {
        wostringstream out;
        for (const auto& e : entries) out << e.first << L" => " << e.second << L"\r\n";
        return out.str();
    }
    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wp; SetTextColor(dc, CLR_TEXT); SetBkColor(dc, CLR_BG);
            return (LRESULT)g_bg;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == GRR_ID_REMOVE && HIWORD(wp) == BN_CLICKED) {
                wstring q = ReadControlText(g_search);
                if (q.empty()) { SetWindowTextW(g_status, L"Enter a keyword to search for before removing it."); return 0; }
                auto entries = ParseList(ReadControlText(g_list));
                auto it = find_if(entries.begin(), entries.end(), [&](const auto& e) {return toLowerW(e.first).find(toLowerW(q)) != wstring::npos; });
                if (it == entries.end()) SetWindowTextW(g_status, L"No remaining keyword matches that search.");
                else { wstring removed = it->first; entries.erase(it); wstring formatted = FormatList(entries); SetWindowTextW(g_list, formatted.c_str()); wstring msg = L"Removed: " + removed; SetWindowTextW(g_status, msg.c_str()); }
                return 0;
            }
            if (id == GRR_ID_REPLACE && HIWORD(wp) == BN_CLICKED) {
                // Capture the user's edited list while g_list is still a valid HWND.
                // Reading it after DestroyWindow returned an empty mapping list.
                g_finalText = ReadControlText(g_list);
                g_accepted = true; g_done = true; DestroyWindow(hwnd); return 0;
            }
            if (id == GRR_ID_CANCEL && HIWORD(wp) == BN_CLICKED) {
                g_accepted = false; g_done = true; DestroyWindow(hwnd); return 0;
            }
            break;
        }
        case WM_CLOSE: g_accepted = false; g_done = true; DestroyWindow(hwnd); return 0;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    // The textbox is intentionally editable: users can remove or alter lines
    // directly. Search + Remove Match is a convenience for deleting one entry.
    static bool Show(const map<wstring, wstring, less<>>& mappings,
        vector<pair<wstring, wstring>>& selected) {
        selected.clear();
        vector<pair<wstring, wstring>> entries(mappings.begin(), mappings.end());
        if (entries.empty()) return true;
        static bool registered = false;
        if (!registered) { WNDCLASSW wc = {}; wc.lpfnWndProc = Proc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = kClass; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = nullptr; RegisterClassW(&wc); registered = true; }
        if (!g_bg) g_bg = CreateSolidBrush(CLR_BG);
        const int W = 920, H = 740;
        RECT r = { 0,0,0,0 }; if (hWndMain) GetWindowRect(hWndMain, &r);
        int x = r.left + ((r.right - r.left) - W) / 2, y = r.top + ((r.bottom - r.top) - H) / 2;
        if (x < 0)x = 0; if (y < 0)y = 0;
        g_done = false; g_accepted = false; g_finalText.clear();
        HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, kClass, L"Review global identifier replacements",
            WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, x, y, W, H, hWndMain, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!hwnd) return false;
        auto make = [&](DWORD ex, const wchar_t* cls, const wchar_t* title, DWORD style, int px, int py, int w, int h, int id) {
            HWND c = CreateWindowExW(ex, cls, title, WS_CHILD | WS_VISIBLE | style, px, py, w, h, hwnd, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
            if (c) SendMessageW(c, WM_SETFONT, (WPARAM)hFontUI, TRUE); return c;
            };
        make(0, L"STATIC", L"Review the original identifier => replacement pairs below. Edit the list or remove entries before proceeding.", SS_LEFT, 18, 14, W - 36, 30, 0);
        make(0, L"STATIC", L"Search original keyword:", SS_LEFT, 18, 52, 150, 22, 0);
        g_search = make(WS_EX_CLIENTEDGE, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 170, 48, 500, 26, GRR_ID_SEARCH);
        make(0, L"BUTTON", L"Remove Match", BS_PUSHBUTTON | WS_TABSTOP, 684, 47, 190, 28, GRR_ID_REMOVE);
        g_list = make(WS_EX_CLIENTEDGE, L"EDIT", L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL | ES_NOHIDESEL | WS_TABSTOP, 18, 84, W - 36, 560, GRR_ID_LIST);
        SendMessageW(g_list, EM_SETLIMITTEXT, 4 * 1024 * 1024, 0);
        wstring initialList = FormatList(entries);
        SetWindowTextW(g_list, initialList.c_str());
        g_status = make(0, L"STATIC", L"One entry per line: original_identifier => replacement_identifier. Remove Match removes the first matching original keyword.", SS_LEFT, 18, 654, W - 36, 28, GRR_ID_STATUS);
        HWND cancel = make(0, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, W - 218, H - 66, 88, 30, GRR_ID_CANCEL);
        HWND replace = make(0, L"BUTTON", L"Replace", BS_DEFPUSHBUTTON | WS_TABSTOP, W - 118, H - 66, 100, 30, GRR_ID_REPLACE);
        (void)cancel; (void)replace;
        SetForegroundWindow(hwnd); if (hWndMain) EnableWindow(hWndMain, FALSE);
        MSG m; while (!g_done && GetMessageW(&m, nullptr, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
        if (hWndMain) EnableWindow(hWndMain, TRUE); if (hWndMain) SetForegroundWindow(hWndMain);
        if (g_accepted) selected = ParseList(g_finalText);
        if (IsWindow(hwnd)) DestroyWindow(hwnd);
        g_search = g_list = g_status = nullptr;
        return g_accepted;
    }
}

static bool IsResourcePathForGlobalReplace(const wstring& path, const wstring& content) {
    wstring lower = toLowerW(path);
    auto ends = [&](const wchar_t* ext) {
        size_t n = wcslen(ext);
        return lower.size() >= n && lower.compare(lower.size() - n, n, ext) == 0;
        };
    if (ends(L".resx") || ends(L".resources") || ends(L".resource") ||
        ends(L".baml") || ends(L".resources.dll")) return true;

    // Resource files and generated resource wrappers are not global-rename targets.
    wstring normalized = lower;
    replace(normalized.begin(), normalized.end(), L'/', L'\\');
    if (normalized.find(L"\\resources\\") != wstring::npos) return true;
    size_t slash = normalized.find_last_of(L"\\");
    wstring leaf = slash == wstring::npos ? normalized : normalized.substr(slash + 1);
    if (leaf == L"resources.cs" || leaf == L"resources.vb" ||
        leaf == L"resources.designer.cs" || leaf == L"resources.designer.vb") return true;

    // A standalone WPF ResourceDictionary is resource content, not source code.
    if (ends(L".xaml") && content.find(L"<ResourceDictionary") != wstring::npos &&
        content.find(L"x:Class=") == wstring::npos) return true;
    return false;
}

static bool IsIdentifierCharForGlobalReplace(wchar_t c) {
    return (c == L'_') || iswalnum(c);
}

// Replace project identifiers in code tokens only. Comments and string/character
// literals are deliberately left intact so resource keys and user text survive.
static wstring ReplaceWholeIdentifierGlobal(const wstring& input,
    const vector<pair<wstring, wstring>>& replacements, Lang language,
    map<wstring, size_t, less<wstring>>& counts) {
    if (replacements.empty() || input.empty()) return input;
    vector<pair<wstring, wstring>> ordered = replacements;
    stable_sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
        return a.first.size() > b.first.size();
        });
    auto matchAt = [&](size_t pos, const wstring& key) {
        if (key.empty() || pos + key.size() > input.size()) return false;
        if (language == Lang::VB) {
            for (size_t k = 0; k < key.size(); ++k)
                if (towlower(input[pos + k]) != towlower(key[k])) return false;
        }
        else if (input.compare(pos, key.size(), key) != 0) return false;
        bool left = pos == 0 || !IsIdentifierCharForGlobalReplace(input[pos - 1]);
        size_t e = pos + key.size();
        bool right = e == input.size() || !IsIdentifierCharForGlobalReplace(input[e]);
        return left && right;
        };

    wstring out; out.reserve(input.size());
    const bool vb = language == Lang::VB;
    for (size_t i = 0; i < input.size();) {
        // Comments: VB apostrophe, C#/C++ // and /* */.
        if (vb && input[i] == L'\'') {
            size_t e = input.find(L'\n', i); if (e == wstring::npos) e = input.size();
            out.append(input, i, e - i); i = e; continue;
        }
        if (vb && i + 3 <= input.size() && towlower(input[i]) == L'r' &&
            towlower(input[i + 1]) == L'e' && towlower(input[i + 2]) == L'm' &&
            (i == 0 || input[i - 1] == L':' || iswspace(input[i - 1])) &&
            (i + 3 == input.size() || iswspace(input[i + 3]))) {
            size_t e = input.find(L'\n', i); if (e == wstring::npos) e = input.size();
            out.append(input, i, e - i); i = e; continue;
        }
        if (!vb && i + 1 < input.size() && input[i] == L'/' && input[i + 1] == L'/') {
            size_t e = input.find(L'\n', i); if (e == wstring::npos)e = input.size();
            out.append(input, i, e - i); i = e; continue;
        }
        if (!vb && i + 1 < input.size() && input[i] == L'/' && input[i + 1] == L'*') {
            size_t e = input.find(L"*/", i + 2); e = (e == wstring::npos ? input.size() : e + 2);
            out.append(input, i, e - i); i = e; continue;
        }

        // Skip string and character literals (including C# verbatim/interpolated
        // prefixes conservatively). This also protects resource lookup keys.
        size_t quotePos = i;
        if (!vb && (input[i] == L'@' || input[i] == L'$')) {
            size_t j = i;
            while (j < input.size() && (input[j] == L'@' || input[j] == L'$') && j - i < 2) ++j;
            if (j < input.size() && input[j] == L'"') quotePos = j;
        }
        // C# raw strings (including interpolated raw strings) and C++ raw strings.
        if (!vb && quotePos + 2 < input.size() &&
            input.compare(quotePos, 3, L"\"\"\"") == 0) {
            size_t e = input.find(L"\"\"\"", quotePos + 3);
            e = (e == wstring::npos ? input.size() : e + 3);
            out.append(input, i, e - i); i = e; continue;
        }
        if (language == Lang::Cpp && i + 1 < input.size() && input[i] == L'R' && input[i + 1] == L'\"') {
            size_t open = input.find(L'(', i + 2);
            if (open != wstring::npos && open - (i + 2) <= 16) {
                wstring delimiter = input.substr(i + 2, open - (i + 2));
                wstring close = L")" + delimiter + L"\"";
                size_t e = input.find(close, open + 1);
                e = (e == wstring::npos ? input.size() : e + close.size());
                out.append(input, i, e - i); i = e; continue;
            }
        }
        if (input[quotePos] == L'"' || (!vb && input[quotePos] == L'\'')) {
            wchar_t q = input[quotePos];
            bool verbatim = !vb && input[quotePos] == L'"' && quotePos > i &&
                input.substr(i, quotePos - i).find(L'@') != wstring::npos;
            size_t e = quotePos + 1;
            while (e < input.size()) {
                if (input[e] == q) {
                    if (verbatim && e + 1 < input.size() && input[e + 1] == q) { e += 2; continue; }
                    ++e; break;
                }
                if (!verbatim && input[e] == L'\\' && e + 1 < input.size()) { e += 2; continue; }
                ++e;
            }
            out.append(input, i, e - i); i = e; continue;
        }

        bool replaced = false;
        for (const auto& r : ordered) {
            if (matchAt(i, r.first)) {
                out += r.second; counts[r.first]++; i += r.first.size(); replaced = true; break;
            }
        }
        if (!replaced) out += input[i++];
    }
    return out;
}

static bool WriteGlobalReplacementLog(const wstring& projectRoot,
    const vector<pair<wstring, wstring>>& replacements,
    const map<wstring, size_t, less<wstring>>& counts,
    size_t skippedResourceFiles) {
    wstring path = projectRoot;
    if (!path.empty() && path.back() != L'\\' && path.back() != L'/') path += L"\\";
    path += L"replaced_strings.txt";
    wofstream out(path, ios::binary | ios::trunc);
    if (!out) return false;
    out << L"Global identifier replacement report\r\n"
        << L"====================================\r\n"
        << L"Resource files excluded: " << skippedResourceFiles << L"\r\n"
        << L"Mappings reviewed: " << replacements.size() << L"\r\n\r\n";
    size_t total = 0;
    for (const auto& r : replacements) {
        auto it = counts.find(r.first); size_t n = (it == counts.end() ? 0 : it->second); total += n;
        out << r.first << L" => " << r.second << L" | occurrences replaced: " << n << L"\r\n";
    }
    out << L"\r\nTOTAL occurrences replaced: " << total << L"\r\n";
    return (bool)out;
}

static bool CommitProjectSourceWrites(
    const vector<pair<wstring, wstring>>& writes,
    const wstring& projectRoot,
    const wstring& backupDir,
    wstring& error) {
    struct Stage { wstring target; wstring temp; };
    vector<Stage> staged;
    staged.reserve(writes.size());
    const wstring nonce = to_wstring(GetCurrentProcessId()) + L"_" + to_wstring(GetTickCount64());

    auto cleanup = [&]() {
        for (const auto& x : staged) if (!x.temp.empty()) DeleteFileW(x.temp.c_str());
        };

    for (size_t i = 0; i < writes.size(); ++i) {
        const wstring& target = writes[i].first;
        wstring temp = target + L".obf-stage-" + nonce + L"-" + to_wstring(i);
        HANDLE h = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            error = L"cannot create staging file for \"" + target + L"\" (" + Win32ErrorText(GetLastError()) + L")";
            cleanup();
            return false;
        }
        const string bytes = wstringToString(writes[i].second);
        size_t offset = 0;
        bool ok = true;
        while (offset < bytes.size()) {
            DWORD chunk = (DWORD)min<size_t>(bytes.size() - offset, 1u << 20);
            DWORD done = 0;
            if (!WriteFile(h, bytes.data() + offset, chunk, &done, nullptr) || done == 0) { ok = false; break; }
            offset += done;
        }
        if (ok && !FlushFileBuffers(h)) ok = false;
        CloseHandle(h);
        if (!ok) {
            DWORD e = GetLastError();
            DeleteFileW(temp.c_str());
            error = L"cannot write staged output for \"" + target + L"\" (" + Win32ErrorText(e) + L")";
            cleanup();
            return false;
        }
        staged.push_back({ target, temp });
    }

    size_t committed = 0;
    for (; committed < staged.size(); ++committed) {
        if (!MoveFileExW(staged[committed].temp.c_str(), staged[committed].target.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            DWORD e = GetLastError();
            error = L"cannot commit transformed file \"" + staged[committed].target + L"\" (" + Win32ErrorText(e) + L")";
            break;
        }
        staged[committed].temp.clear();
    }

    if (committed != staged.size()) {
        // Best-effort rollback of already committed files. Backups are created
        // before the worker starts; never silently leave a partial project.
        for (size_t i = 0; i < committed; ++i) {
            wstring rel = StripPrefixRel(staged[i].target, projectRoot);
            wstring backup = backupDir + L"\\" + rel;
            if (!CopyFileW(backup.c_str(), staged[i].target.c_str(), FALSE)) {
                error += L"; rollback also failed for \"" + staged[i].target + L"\" (" + Win32ErrorText(GetLastError()) + L")";
            }
        }
        cleanup();
        return false;
    }
    cleanup();
    return true;
}

// Same split as OnObfuscateSingleFile above: the worker below does the heavy
// lifting on a background thread.
static void OnObfuscateProjectWorker(LoadedProject proj, wstring bakDir) {
    ShowWindow(hProgress, SW_SHOW);
    SendMessage(hProgress, PBM_SETPOS, 0, 0);
    SetWindowText(hLog, L"");
    resetLayerHits();

    g_logCollapsed = false;
    g_logAnimT = 0.0;
    ShowWindow(hBtnShowLog, SW_HIDE);
    if (hWndMain) LayoutControls(hWndMain);

    Lang lang = proj.lang;
    RunLogInfo runLog;
    runLog.sourcePath = proj.displayName;
    runLog.backupPath = bakDir;
    runLog.lang = lang;
    runLog.checkboxStates = CollectCheckboxStates();
    runLog.projectMode = true;
    runLog.projectFiles = proj.files;
    {
        wchar_t cb[128] = {};
        int sel = (int)SendMessage(hCmbCount, CB_GETCURSEL, 0, 0);
        if (sel >= 0) SendMessage(hCmbCount, CB_GETLBTEXT, sel, (LPARAM)cb);
        runLog.deadCodeCountSel = cb;
    }

    try {
        LogMessage(L">> project mode: " + to_wstring(proj.files.size()) + L" file(s), lang=" + langName(lang), CLR_INFO);

        // Part B: discover this project's own package/assembly references
        // and protect their public surface from renaming before the rename
        // pass below ever runs. No-op (returns "") for Folder mode, which has
        // no project file to read references from.
        {
            wstring refSummary = RunProjectReferenceProtection(proj);
            if (!refSummary.empty()) LogMessage(L"  " + refSummary, CLR_TEXT_DIM);
        }

        ObfuscationEngine engine(L"", lang);
        engine.sources.clear();

        engine.excludeClassNames = (SendMessage(hChkClasses, BM_GETCHECK, 0, 0) == BST_CHECKED);
        engine.excludeControlNames = (SendMessage(hChkControls, BM_GETCHECK, 0, 0) == BST_CHECKED); // now supports VB, C#, and (where applicable) C++ control-field detection
        engine.excludePublicDeclarations = (lang != Lang::Cpp) && (SendMessage(hChkPublic, BM_GETCHECK, 0, 0) == BST_CHECKED);
        if (lang == Lang::Cpp) {
            engine.preserveMacros = (SendMessage(hChkMacros, BM_GETCHECK, 0, 0) == BST_CHECKED);
            engine.preserveLabels = (SendMessage(hChkLabels, BM_GETCHECK, 0, 0) == BST_CHECKED);
            engine.preserveOperators = (SendMessage(hChkOperators, BM_GETCHECK, 0, 0) == BST_CHECKED);
            engine.preserveNamespaces = (SendMessage(hChkNamespaces, BM_GETCHECK, 0, 0) == BST_CHECKED);
            engine.preserveTypedefs = (SendMessage(hChkTypedefs, BM_GETCHECK, 0, 0) == BST_CHECKED);
            engine.preserveTemplates = (SendMessage(hChkTemplates, BM_GETCHECK, 0, 0) == BST_CHECKED);
            engine.skipPreprocessorZero = (SendMessage(hChkPrecomp, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
        if (SendMessage(hChkVWV, BM_GETCHECK, 0, 0) == BST_CHECKED) engine.setCharset(ObfuscationCharset::vwv);
        else if (SendMessage(hChkI1L, BM_GETCHECK, 0, 0) == BST_CHECKED) engine.setCharset(ObfuscationCharset::I1lL);
        else engine.setCharset(ObfuscationCharset::O0o);

        // "Skip external members" now applies to VB, C#, and C++ (see the
        // matching comment at the other call site for why this is a no-op
        // beyond VB today, rather than something C#/C++ branch on).
        bool wantSkipExternal = (SendMessage(hChkSkipExternal, BM_GETCHECK, 0, 0) == BST_CHECKED);
        bool wantDeadCode = (SendMessage(hChkDeadCode, BM_GETCHECK, 0, 0) == BST_CHECKED);
        bool wantStrEnc = (SendMessage(hChkStrEnc, BM_GETCHECK, 0, 0) == BST_CHECKED);
        engine.skipExternalMembers = wantSkipExternal;
        engine.enableDeadCode = wantDeadCode;
        engine.deadCodeCount = wantDeadCode ? (((int)SendMessage(hCmbCount, CB_GETCURSEL, 0, 0) + 1) * 3) : 0;
        engine.mixDeadCode = wantDeadCode && (SendMessage(hChkMix, BM_GETCHECK, 0, 0) == BST_CHECKED);
        engine.enableStringEncryption = wantStrEnc;
        engine.shouldInjectDecryptModule = wantStrEnc && (SendMessage(hChkInjectDecrypt, BM_GETCHECK, 0, 0) == BST_CHECKED);
        engine.excludeAlreadyEncrypted = wantStrEnc && (SendMessage(hChkSkipEncrypted, BM_GETCHECK, 0, 0) == BST_CHECKED);
        engine.enableStringExclusion = wantStrEnc && (SendMessage(hChkStrExcl, BM_GETCHECK, 0, 0) == BST_CHECKED);
        if (engine.enableStringExclusion) engine.stringExclusionList = ParseCustomNames(hEditStrExcl);

        if (SendMessage(hChkCustom, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            auto cn = ParseCustomNames();
            if (!cn.empty()) engine.addPreserveList(cn);
        }
        if (lang == Lang::VB || lang == Lang::CSharp) {
            engine.addPreserveList({
                L"Main", L"InitializeComponent", L"Dispose", L"ToString", L"Equals", L"GetHashCode",
                L"GetType", L"MemberwiseClone", L"Finalize", L"New", L"Me", L"MyBase", L"MyClass",
                L"Value", L"Text", L"Name", L"Tag", L"Enabled", L"Visible", L"Checked", L"Selected",
                L"Items", L"Controls", L"Components", L"Add", L"AddRange", L"Clear", L"Remove",
                L"RemoveAt", L"Insert", L"Contains", L"IndexOf", L"Count", L"Length", L"Capacity",
                L"ToArray", L"ToList", L"ToDictionary", L"Select", L"Where", L"OrderBy", L"GroupBy",
                L"First", L"FirstOrDefault", L"Single", L"SingleOrDefault", L"Last", L"LastOrDefault",
                L"Any", L"All", L"Distinct", L"Skip", L"Take", L"Reverse", L"Concat", L"Union",
                L"Intersect", L"Except", L"Sum", L"Max", L"Min", L"Average",
                L"GetEnumerator", L"MoveNext", L"Current", L"Reset"
                });
        }
        else {
            engine.addPreserveList({
                L"main", L"WinMain", L"wWinMain", L"DllMain",
                L"begin", L"end", L"cbegin", L"cend", L"rbegin", L"rend",
                L"size", L"empty", L"data", L"front", L"back", L"at",
                L"push_back", L"pop_back", L"push_front", L"pop_front",
                L"insert", L"erase", L"clear", L"find", L"count", L"swap",
                L"operator", L"operator==", L"operator!=", L"operator<<", L"operator>>"
                });
        }

        for (auto& f : proj.files) {
            ifstream ifs(f.c_str(), ios::binary);
            if (!ifs) throw runtime_error("cannot open " + wstringToString(f));
            string u((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
            ifs.close();
            if (u.size() >= 3 && (unsigned char)u[0] == 0xEF && (unsigned char)u[1] == 0xBB && (unsigned char)u[2] == 0xBF)
                u.erase(0, 3);
            engine.addSource(stringToWstring(u), f, proj.lang);
        }

        SendMessage(hProgress, PBM_SETPOS, 10, 0);
        SetWindowText(hStatus, L"pairing detection...");

        vector<wstring> resxToRewrite = PrepareDesignerPairing(proj, engine);

        vector<wstring> xamlPaths;
        for (auto& f : proj.files) {
            wstring lower = toLowerW(f);
            if (endsWithW(lower, L".cs")) {
                wstring xaml;
                if (endsWithW(lower, L".xaml.cs")) xaml = f.substr(0, f.size() - 3);
                else {
                    size_t ld = f.find_last_of(L'.');
                    xaml = f.substr(0, ld) + L".xaml";
                }
                if (fileExistsW(xaml)) xamlPaths.push_back(xaml);
            }
        }

        bool rewriteXaml = false;
        if (!xamlPaths.empty()) {
            wstring prompt = to_wstring(xamlPaths.size()) +
                L" matching XAML file(s) found — rewrite them to match the obfuscated .cs names?\n\n"
                L"Yes = rewrite XAML in place (backup in bak\\).\n"
                L"No  = preserve XAML-declared names on the C# side (XAML stays untouched).";
            int choice = MessageBox(hWndMain, prompt.c_str(), L"XAML pairing", MB_YESNO | MB_ICONQUESTION);
            rewriteXaml = (choice == IDYES);
        }

        vector<XamlPairInfo> xamlPairs;
        for (auto& xp : xamlPaths) {
            XamlPairInfo pi = PrepareXamlPairingPreAnswered(xp, proj.lang, engine, rewriteXaml);
            if (pi.active) xamlPairs.push_back(pi);
        }
        runLog.xamlPairAttempted = !xamlPairs.empty();

        vector<wstring> uiToRewrite;
        {
            set<wstring> seenUi;
            for (auto& f : proj.files) {
                wstring lower = toLowerW(f);
                size_t ld = lower.find_last_of(L'.');
                if (ld == wstring::npos) continue;
                wstring ext = lower.substr(ld);
                if (ext == L".cpp" || ext == L".h" || ext == L".hpp" || ext == L".cc" || ext == L".cxx") {
                    wstring ui = f.substr(0, ld) + L".ui";
                    if (fileExistsW(ui)) {
                        if (seenUi.insert(toLowerW(ui)).second) uiToRewrite.push_back(ui);
                    }
                }
            }
        }

        SendMessage(hProgress, PBM_SETPOS, 20, 0);
        SetWindowText(hStatus, L"extracting identifiers...");
        auto pre = engine.preScan();
        LogMessage(L"Detected " + to_wstring(pre.classNames.size()) + L" classes, "
            + to_wstring(pre.controlNames.size()) + L" controls, "
            + to_wstring(pre.publicNames.size()) + L" public decls.", CLR_TEXT_DIM);
        engine.extractIdentifiers();
        LogMessage(L"Found " + to_wstring(engine.getLocalClasses().size()) + L" classes, " +
            to_wstring(engine.getLocalMethods().size()) + L" methods, " +
            to_wstring(engine.getLocalFields().size()) + L" fields.", CLR_TEXT_DIM);
        if (rewriteXaml) {
            for (auto& xp : xamlPairs) {
                vector<wstring> unresolved = FindUnresolvedXamlHandlers(xp.xamlOriginalText, engine);
                if (!unresolved.empty()) {
                    wstring list;
                    for (size_t i = 0; i < unresolved.size(); i++) { if (i) list += L", "; list += unresolved[i]; }
                    LogMessage(L"ABORT: XAML pairing failed verification — handler(s) not resolved: " + list, CLR_ERROR);
                    throw runtime_error("XAML pairing: rewrite verification failed");
                }
            }
        }

        SendMessage(hProgress, PBM_SETPOS, 30, 0);
        SetWindowText(hStatus, L"obfuscating...");

        // v3.4: ask the user which existing file hosts the public decryptor.
        bool wantHostDecryptor =
            engine.enableStringEncryption && engine.shouldInjectDecryptModule;
        wstring decryptorHostPath;
        if (wantHostDecryptor) {
            decryptorHostPath = DecryptorHostPick::PromptDecryptorHostFile(proj, lang);
            if (decryptorHostPath.empty())
                throw runtime_error("cancelled by user: no decryptor host file selected");
            LogMessage(L"Decryptor host: \"" + baseNameOf(decryptorHostPath) +
                L"\" (public mxcncbs injected there; other files reference it).", CLR_INFO);
        }
        wstring hostBody;
        bool hostWritten = false;
        bool cppHostIsHeader = false;
        if (wantHostDecryptor) {
            hostBody = engine.makeHostDecryptorBody(lang);
            wstring hl = toLowerW(decryptorHostPath);
            cppHostIsHeader = endsWithW(hl, L".h") || endsWithW(hl, L".hpp") ||
                endsWithW(hl, L".hxx") || endsWithW(hl, L".hh");
        }

        int totalSources = (int)engine.sources.size();
        bool wroteAnyEncrypted = false;
        vector<pair<wstring, wstring>> pendingSourceWrites;
        pendingSourceWrites.reserve(engine.sources.size());
        for (int i = 0; i < totalSources; i++) {
            wstring out = engine.obfuscateOne((size_t)i);
            out = engine.patchReflectionStrings(out);
            out = engine.patchResolvedLabels(out);
            engine.setActiveLang(engine.sources[i].lang);
            bool fileHadEncrypted = false;

            if (engine.enableStringEncryption) {
                size_t before = engine.stringEncryptionLog.size();
                out = engine.encryptStringsVB(out);
                bool hadReplacements = engine.stringEncryptionLog.size() > before;
                if (hadReplacements) {
                    wroteAnyEncrypted = true;
                    fileHadEncrypted = true;
                    // v3.4: no per-file edit for C#/VB -- call sites resolve
                    // mxcncbs.ertrymn against the public class in the host file.
                }
            }
            if (engine.enableDeadCode) {
                out = engine.injectDeadCode(out);
            }

            if (wantHostDecryptor && iequals(engine.sources[i].label, decryptorHostPath)) {
                out += hostBody;
                hostWritten = true;
                LogMessage(L"  >> injected public mxcncbs into " + baseNameOf(decryptorHostPath), CLR_SUCCESS);
            }
            else if (wantHostDecryptor && fileHadEncrypted && lang == Lang::Cpp && cppHostIsHeader) {
                wstring inc = MakeIncludeLineForFile(engine.sources[i].label, decryptorHostPath);
                if (out.find(inc) == wstring::npos)
                    out = injectStandaloneInclude(out, inc);
            }

            pendingSourceWrites.push_back({ engine.sources[i].label, std::move(out) });
            LogMessage(L"staged " + baseNameOf(engine.sources[i].label), CLR_INFO);

            SendMessage(hProgress, PBM_SETPOS, 30 + (i + 1) * 60 / (totalSources > 0 ? totalSources : 1), 0);
        }

        // Build the global rename list, let the user review/remove entries, then
        // perform whole-identifier replacements across every staged project file.
        vector<pair<wstring, wstring>> globalReplacementSelection;
        map<wstring, size_t, less<wstring>> globalReplacementCounts;
        size_t globalSkippedResourceFiles = 0;
        {
            map<wstring, wstring, less<>> allMappings; set<wstring> ambiguous;
            auto merge = [&](const map<wstring, wstring, less<>>& src) {
                for (const auto& kv : src) {
                    if (ambiguous.count(kv.first)) continue;
                    auto it = allMappings.find(kv.first);
                    if (it == allMappings.end()) allMappings.emplace(kv.first, kv.second);
                    else if (it->second != kv.second) { allMappings.erase(it); ambiguous.insert(kv.first); }
                }
                };
            merge(engine.getClassMap()); merge(engine.getMethodMap()); merge(engine.getFieldMap());
            if (!GlobalRenameReview::Show(allMappings, globalReplacementSelection))
                throw runtime_error("cancelled by user during global rename review");
            size_t changedFiles = 0;
            for (const auto& r : globalReplacementSelection) globalReplacementCounts.emplace(r.first, 0);
            for (size_t fileIndex = 0; fileIndex < pendingSourceWrites.size(); ++fileIndex) {
                auto& item = pendingSourceWrites[fileIndex];
                if (IsResourcePathForGlobalReplace(item.first, item.second)) {
                    ++globalSkippedResourceFiles;
                    LogMessage(L"Global rename: excluded resource file " + baseNameOf(item.first), CLR_INFO);
                    continue;
                }
                Lang fileLang = fileIndex < engine.sources.size() ? engine.sources[fileIndex].lang : lang;
                wstring fixed = ReplaceWholeIdentifierGlobal(item.second, globalReplacementSelection, fileLang, globalReplacementCounts);
                if (fixed != item.second) { item.second.swap(fixed); ++changedFiles; }
            }
            size_t totalOccurrences = 0;
            for (const auto& c : globalReplacementCounts) totalOccurrences += c.second;
            LogMessage(L"Global identifier replacement: reviewed " + to_wstring(globalReplacementSelection.size()) +
                L" mapping(s); replaced " + to_wstring(totalOccurrences) + L" occurrence(s) in " +
                to_wstring(changedFiles) + L" staged file(s); excluded " + to_wstring(globalSkippedResourceFiles) +
                L" resource file(s) and " + to_wstring(ambiguous.size()) + L" ambiguous mapping(s).", CLR_INFO);
        }

        // Emit after transformation so this also includes project identifiers
        // whose declarations were renamed but whose member-access use sites were
        // conservatively left unchanged. std::set guarantees one line per name.
        {
            wstring ignoredPath = proj.rootDir;
            if (!ignoredPath.empty() && ignoredPath.back() != L'\\' && ignoredPath.back() != L'/') ignoredPath += L"\\";
            ignoredPath += L"ignored.txt";
            if (engine.writeIgnoredSymbols(ignoredPath))
                LogMessage(L"Wrote ignored-symbol report: " + ignoredPath + L" (" + to_wstring(engine.getIgnoredSymbols().size()) + L" unique name(s)).", CLR_INFO);
            else
                LogMessage(L"WARN: could not write ignored-symbol report: " + ignoredPath, CLR_WARN);
        }

        SetWindowText(hStatus, L"committing transformed files...");
        wstring commitError;
        if (!CommitProjectSourceWrites(pendingSourceWrites, proj.rootDir, bakDir, commitError))
            throw runtime_error(wstringToString(commitError));
        for (const auto& item : pendingSourceWrites)
            LogMessage(L"wrote " + baseNameOf(item.first), CLR_INFO);
        if (!WriteGlobalReplacementLog(proj.rootDir, globalReplacementSelection,
            globalReplacementCounts, globalSkippedResourceFiles))
            LogMessage(L"WARN: could not write replaced_strings.txt in project directory.", CLR_WARN);
        else
            LogMessage(L"Wrote replacement count report: " + proj.rootDir + L"\\replaced_strings.txt", CLR_SUCCESS);

        // v3.4: the decryptor lives inside the chosen host file -- no standalone file.
        if (wantHostDecryptor) {
            if (!hostWritten) {
                LogMessage(L"WARNING: decryptor host file was not among the obfuscated sources; "
                    L"public mxcncbs was NOT injected.", CLR_WARN);
            }
            else if (wroteAnyEncrypted) {
                LogMessage(L"Decryptor: public mxcncbs lives in \"" + baseNameOf(decryptorHostPath) +
                    L"\"; all other files reference it from the same project.", CLR_SUCCESS);
                if (lang == Lang::Cpp && !cppHostIsHeader)
                    LogMessage(L"NOTE: C++ host is not a header -- other TUs with encrypted strings cannot "
                        L"see mxcncbs. Pick a .h/.hpp host next time, or move the class into a shared header.", CLR_WARN);
            }
        }

        SetWindowText(hStatus, L"rewriting paired files...");
        SendMessage(hProgress, PBM_SETPOS, 92, 0);

        for (auto& rp : resxToRewrite) {
            RewriteResxInPlace(rp, engine, bakDir, proj.rootDir);
        }

        if (rewriteXaml) {
            for (auto& xp : xamlPairs) {
                bool ok = RewriteXamlToMatch(xp, engine);
                if (!ok) {
                    runLog.xamlPairSucceeded = false;
                    runLog.xamlPairError = L"rewrite failed: " + baseNameOf(xp.xamlPath);
                    LogMessage(L"XAML rewrite failed for " + baseNameOf(xp.xamlPath), CLR_WARN);
                }
            }
        }

        for (auto& ui : uiToRewrite) {
            RewriteUiInPlace(ui, engine);
        }

        SendMessage(hProgress, PBM_SETPOS, 100, 0);
        LogMessage(L"project obfuscation complete!", CLR_SUCCESS);

        runLog.classRenames = (int)engine.getClassMap().size();
        runLog.methodRenames = (int)engine.getMethodMap().size();
        runLog.fieldRenames = (int)engine.getFieldMap().size();

        {
            int t = g_totalLookups.load(), me = g_hitMembers.load(), ff = g_hitFreeFn.load();
            int w32 = g_hitWin32.load(), tp = g_hitThirdParty.load(), ho = g_hitHeaderOnly.load(), ne = g_hitNet.load(), ms = g_hitMiss.load();
            LogMessage(L"=== layer hit report ===", CLR_ACCENT);
            LogMessage(L"  total=" + to_wstring(t) + L" protected=" + to_wstring(me + ff + w32 + tp + ho + ne) + L" missed=" + to_wstring(ms), CLR_TEXT_DIM);
        }

        SetWindowText(hStatus, L"done.");
        runLog.success = true;
        runLog.outputPath = bakDir;
        runLog.xamlPairSucceeded = (runLog.xamlPairAttempted && runLog.xamlPairError.empty());
        runLog.stringEncryptionDone = engine.enableStringEncryption && !engine.stringEncryptionLog.empty();
        runLog.stringPairs = engine.stringEncryptionLog;
        WriteRunLogs(runLog);
        LogMessage(L"logged: " + g_lastRunLogFolder, CLR_TEXT_DIM);

        g_logCollapsed = true;
        g_logAnimT = 0.0;
    }
    catch (const exception& ex) {
        wstring em = stringToWstring(ex.what());
        if (em.find(L"cancelled by user") == 0) {
            LogMessage(L"Project run cancelled by user before project files were committed.", CLR_WARN);
            SetWindowText(hStatus, L"cancelled.");
        }
        else {
            LogMessage(L"ERROR: " + em, CLR_ERROR);
            SetWindowText(hStatus, L"project obfuscation failed.");
            LogMessage(L"NOTE: pre-obfuscation originals are in " + bakDir, CLR_WARN);
        }
        runLog.success = false;
        runLog.errorMsg = em;
        WriteRunLogs(runLog);
    }
    EnableWindow(hBtnObfuscate, TRUE);
    EnableWindow(hBtnBrowse, TRUE);
    ShowWindow(hProgress, SW_HIDE);
    g_obfuscationBusy.store(false);
}

// Thin, fast, UI-thread entry point -- see OnObfuscateSingleFile above for
// why the split exists. PrepareBackup() just copies files, so it's kept here
// (quick, and needed before we can report a backup path on failure); the
// slow regex/rename/pairing work happens in OnObfuscateProjectWorker.
void OnObfuscateProject(const LoadedProject& proj) {
    if (g_obfuscationBusy.exchange(true)) {
        LogMessage(L"WARN: an obfuscation run is already in progress — please wait for it to finish.", CLR_WARN);
        return;
    }
    wstring bakDir = PrepareBackup(proj);
    if (bakDir.empty()) {
        g_obfuscationBusy.store(false);
        return;
    }

    EnableWindow(hBtnObfuscate, FALSE);
    EnableWindow(hBtnBrowse, FALSE);

    std::thread(OnObfuscateProjectWorker, proj, bakDir).detach();
}

// ============================================================================
// §5c: OnObfuscate dispatch
// ============================================================================
void OnObfuscate() {
    if (g_projectLoaded && !g_currentProject.files.empty()) {
        OnObfuscateProject(g_currentProject);
        return;
    }
    OnObfuscateSingleFile();
}

// ============================================================================
// CUSTOM BUTTONS
// ============================================================================
#define BTN_BROWSE_CLASS  L"CardBrowseBtn"
#define BTN_ACTION_CLASS  L"CardActionBtn"
#define BTN_SEG_CLASS     L"CardSegBtn"

struct BtnState {
    bool hovered = false;
    bool pressed = false;
    bool tracking = false;
};

static void PaintRoundedRect(HDC hdc, RECT rc, int radius, COLORREF fill, COLORREF border, int borderW) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, borderW, border);
    HGDIOBJ oldBr = SelectObject(hdc, br);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(br);
    DeleteObject(pen);
}

static void PaintRoundedGradientRect(HDC hdc, RECT rc, int radius, COLORREF top, COLORREF bottom, COLORREF border, int borderW) {
    HRGN rgn = CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1, radius * 2, radius * 2);
    HRGN oldClip = CreateRectRgn(0, 0, 0, 0);
    int hadOldClip = GetClipRgn(hdc, oldClip);
    SelectClipRgn(hdc, rgn);

    TRIVERTEX vert[2];
    vert[0].x = rc.left;  vert[0].y = rc.top;
    vert[0].Red = (COLOR16)(GetRValue(top) << 8); vert[0].Green = (COLOR16)(GetGValue(top) << 8);
    vert[0].Blue = (COLOR16)(GetBValue(top) << 8); vert[0].Alpha = 0;
    vert[1].x = rc.right; vert[1].y = rc.bottom;
    vert[1].Red = (COLOR16)(GetRValue(bottom) << 8); vert[1].Green = (COLOR16)(GetGValue(bottom) << 8);
    vert[1].Blue = (COLOR16)(GetBValue(bottom) << 8); vert[1].Alpha = 0;
    GRADIENT_RECT gRect = { 0, 1 };
    GradientFill(hdc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

    SelectClipRgn(hdc, hadOldClip == 1 ? oldClip : nullptr);
    DeleteObject(oldClip);
    DeleteObject(rgn);

    HPEN pen = CreatePen(PS_SOLID, borderW, border);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static COLORREF LerpColor(COLORREF a, COLORREF b, double t) {
    if (t < 0) t = 0; if (t > 1) t = 1;
    int r = (int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t);
    int g = (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t);
    int bl = (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t);
    return RGB(r, g, bl);
}

static void PaintGlowRing(HDC hdc, RECT rc, COLORREF core, COLORREF bg, int rings) {
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    for (int i = rings; i >= 1; i--) {
        COLORREF c = LerpColor(core, bg, (double)i / (rings + 1));
        HPEN p = CreatePen(PS_SOLID, 1, c);
        HGDIOBJ oldPen = SelectObject(hdc, p);
        RECT r2 = { rc.left - i, rc.top - i, rc.right + i, rc.bottom + i };
        int rad = 20 + i * 2;
        RoundRect(hdc, r2.left, r2.top, r2.right, r2.bottom, rad, rad);
        SelectObject(hdc, oldPen); DeleteObject(p);
    }
    SelectObject(hdc, oldBr);
}

static COLORREF PaintControlBackdrop(HWND child, HDC mdc, int w, int h);

LRESULT CALLBACK BrowseBtnProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BtnState* st = (BtnState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)new BtnState());
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;
        HDC mdc = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, w > 0 ? w : 1, h > 0 ? h : 1);
        HGDIOBJ ob = SelectObject(mdc, bmp);
        PaintControlBackdrop(hwnd, mdc, w, h);
        COLORREF fill = (st && st->pressed) ? RGB(0, 70, 30)
            : (st && st->hovered) ? CLR_BTN_PRIMARY_H : CLR_BTN_PRIMARY;
        PaintRoundedRect(mdc, rc, 6, fill, CLR_ACCENT_DIM, 1);
        SetBkMode(mdc, TRANSPARENT); SetTextColor(mdc, CLR_TEXT_BRIGHT);
        HFONT oF = (HFONT)SelectObject(mdc, hFontBold);
        DrawText(mdc, L"Browse\u2026", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(mdc, oF);
        BitBlt(hdc, 0, 0, w, h, mdc, 0, 0, SRCCOPY);
        SelectObject(mdc, ob); DeleteObject(bmp); DeleteDC(mdc);
        EndPaint(hwnd, &ps); return 0;
    }
    case WM_MOUSEMOVE:
        if (st && !st->tracking) { TRACKMOUSEEVENT t = { sizeof(t), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&t); st->tracking = true; }
        if (st && !st->hovered) { st->hovered = true; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    case WM_MOUSELEAVE:
        if (st) { st->hovered = false; st->tracking = false; st->pressed = false; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    case WM_LBUTTONDOWN: if (st) { st->pressed = true; SetCapture(hwnd); InvalidateRect(hwnd, NULL, FALSE); } return 0;
    case WM_LBUTTONUP: {
        if (st) {
            st->pressed = false;
            ReleaseCapture();
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
            RECT rc; GetClientRect(hwnd, &rc);
            if (PtInRect(&rc, pt)) OnBrowse();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_DESTROY: delete st; SetWindowLongPtr(hwnd, GWLP_USERDATA, 0); return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ActionBtnProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BtnState* st = (BtnState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)new BtnState());
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;
        HDC mdc = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, w > 0 ? w : 1, h > 0 ? h : 1);
        HGDIOBJ ob = SelectObject(mdc, bmp);
        PaintControlBackdrop(hwnd, mdc, w, h);
        bool en = IsWindowEnabled(hwnd) != 0;
        COLORREF fill = !en ? RGB(30, 40, 30)
            : (st && st->pressed) ? RGB(0, 120, 50)
            : (st && st->hovered) ? CLR_BTN_ACTION_H : CLR_BTN_ACTION;
        PaintRoundedRect(mdc, rc, 8, fill, en ? CLR_ACCENT : RGB(50, 70, 50), 2);
        SetBkMode(mdc, TRANSPARENT); SetTextColor(mdc, en ? RGB(0, 0, 0) : RGB(80, 100, 80));
        HFONT oF = (HFONT)SelectObject(mdc, hFontBold);
        DrawText(mdc, L"Obfuscate  \u2192", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(mdc, oF);
        BitBlt(hdc, 0, 0, w, h, mdc, 0, 0, SRCCOPY);
        SelectObject(mdc, ob); DeleteObject(bmp); DeleteDC(mdc);
        EndPaint(hwnd, &ps); return 0;
    }
    case WM_MOUSEMOVE:
        if (st && !st->tracking) { TRACKMOUSEEVENT t = { sizeof(t), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&t); st->tracking = true; }
        if (st && !st->hovered) { st->hovered = true; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    case WM_MOUSELEAVE:
        if (st) { st->hovered = false; st->tracking = false; st->pressed = false; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    case WM_LBUTTONDOWN: if (st && IsWindowEnabled(hwnd)) { st->pressed = true; SetCapture(hwnd); InvalidateRect(hwnd, NULL, FALSE); } return 0;
    case WM_LBUTTONUP: {
        if (st && IsWindowEnabled(hwnd)) {
            st->pressed = false;
            ReleaseCapture();
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
            RECT rc; GetClientRect(hwnd, &rc);
            if (PtInRect(&rc, pt)) OnObfuscate();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_ENABLE: InvalidateRect(hwnd, NULL, FALSE); return 0;
    case WM_DESTROY: delete st; SetWindowLongPtr(hwnd, GWLP_USERDATA, 0); return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SegBtnProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BtnState* st = (BtnState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)new BtnState());
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;
        HDC mdc = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, w > 0 ? w : 1, h > 0 ? h : 1);
        HGDIOBJ ob = SelectObject(mdc, bmp);
        PaintControlBackdrop(hwnd, mdc, w, h);
        bool active = (g_symbolMode == (GetDlgCtrlID(hwnd) == IDC_BTN_EMBEDDED ? SymbolMode::Embedded : SymbolMode::Generate));
        COLORREF fill = active ? CLR_SEG_ON : (st && st->hovered ? RGB(40, 55, 40) : CLR_SEG_OFF);
        PaintRoundedRect(mdc, rc, 5, fill, active ? CLR_ACCENT : CLR_BORDER, 1);
        SetBkMode(mdc, TRANSPARENT); SetTextColor(mdc, active ? RGB(0, 0, 0) : CLR_TEXT);
        HFONT oF = (HFONT)SelectObject(mdc, hFontBold);
        wchar_t buf[128]; GetWindowText(hwnd, buf, 128);
        DrawText(mdc, buf, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(mdc, oF);
        BitBlt(hdc, 0, 0, w, h, mdc, 0, 0, SRCCOPY);
        SelectObject(mdc, ob); DeleteObject(bmp); DeleteDC(mdc);
        EndPaint(hwnd, &ps); return 0;
    }
    case WM_MOUSEMOVE:
        if (st && !st->tracking) { TRACKMOUSEEVENT t = { sizeof(t), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&t); st->tracking = true; }
        if (st && !st->hovered) { st->hovered = true; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    case WM_MOUSELEAVE:
        if (st) { st->hovered = false; st->tracking = false; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    case WM_LBUTTONUP: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
        RECT rc; GetClientRect(hwnd, &rc);
        if (PtInRect(&rc, pt)) {
            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), (LPARAM)hwnd);
        }
        return 0;
    }
    case WM_DESTROY: delete st; SetWindowLongPtr(hwnd, GWLP_USERDATA, 0); return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void RegisterCustomButtonClasses(HINSTANCE hInst) {
    WNDCLASS wc = {};
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_HAND);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpfnWndProc = BrowseBtnProc; wc.lpszClassName = BTN_BROWSE_CLASS; RegisterClass(&wc);
    wc.lpfnWndProc = ActionBtnProc; wc.lpszClassName = BTN_ACTION_CLASS; RegisterClass(&wc);
    wc.lpfnWndProc = SegBtnProc;    wc.lpszClassName = BTN_SEG_CLASS;    RegisterClass(&wc);
}

// ============================================================================
// CARD / SECTION PAINTING
// ============================================================================
static void PaintCard(HDC hdc, RECT rc, const wchar_t* sectionTitle) {
    PaintRoundedGradientRect(hdc, rc, 10, CLR_CARD_TOP, CLR_CARD_BOTTOM, CLR_ACCENT, 1);
    if (sectionTitle && *sectionTitle) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_ACCENT);
        HFONT oF = (HFONT)SelectObject(hdc, hFontSection);
        TextOut(hdc, rc.left + 16, rc.top + 12, sectionTitle, (int)wcslen(sectionTitle));
        SelectObject(hdc, oF);
        HPEN p = CreatePen(PS_SOLID, 1, CLR_BORDER);
        HGDIOBJ op = SelectObject(hdc, p);
        MoveToEx(hdc, rc.left + 16, rc.top + 34, nullptr);
        LineTo(hdc, rc.right - 16, rc.top + 34);
        SelectObject(hdc, op); DeleteObject(p);
    }
}

static void DrawGreenFrame(HWND hwnd, bool windowDC) {
    RECT rc; GetWindowRect(hwnd, &rc); OffsetRect(&rc, -rc.left, -rc.top);
    HDC dc = windowDC ? GetWindowDC(hwnd) : GetDC(hwnd);
    if (!dc) return;
    if (!windowDC) GetClientRect(hwnd, &rc);
    HBRUSH br = CreateSolidBrush(CLR_ACCENT_DIM);
    FrameRect(dc, &rc, br);
    InflateRect(&rc, -1, -1);
    FrameRect(dc, &rc, br);
    DeleteObject(br);
    ReleaseDC(hwnd, dc);
}

static LRESULT CALLBACK GreenFrameSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR isCombo) {
    switch (msg) {
    case WM_NCDESTROY: RemoveWindowSubclass(hwnd, GreenFrameSubclassProc, 2); break;
    case WM_NCPAINT: {
        LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);
        DrawGreenFrame(hwnd, true);
        return r;
    }
    case WM_PAINT: {
        LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);
        if (isCombo) DrawGreenFrame(hwnd, false);
        return r;
    }
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_ENABLE: {
        LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);
        RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
        return r;
    }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK ComboSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_NCDESTROY: RemoveWindowSubclass(hwnd, ComboSubclassProc, 3); break;
    case WM_PAINT:
    case WM_PRINTCLIENT: {
        PAINTSTRUCT ps; HDC out = (msg == WM_PAINT) ? BeginPaint(hwnd, &ps) : (HDC)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;
        HDC mdc = CreateCompatibleDC(out);
        HBITMAP bmp = CreateCompatibleBitmap(out, w, h);
        HGDIOBJ ob = SelectObject(mdc, bmp);

        HBRUSH bg = CreateSolidBrush(CLR_CTRL_BG); FillRect(mdc, &rc, bg); DeleteObject(bg);

        bool dropped = SendMessage(hwnd, CB_GETDROPPEDSTATE, 0, 0) != 0;
        RECT btn = { w - 24, 2, w - 2, h - 2 };
        HBRUSH bb = CreateSolidBrush(dropped ? CLR_ACCENT_DIM : RGB(0, 70, 25));
        FillRect(mdc, &btn, bb); DeleteObject(bb);

        int cx = (btn.left + btn.right) / 2, cy = (btn.top + btn.bottom) / 2;
        POINT tri[3] = { { cx - 5, cy - 2 }, { cx + 5, cy - 2 }, { cx, cy + 3 } };
        HBRUSH ab = CreateSolidBrush(CLR_ACCENT); HPEN ap = CreatePen(PS_SOLID, 1, CLR_ACCENT);
        HGDIOBJ oB = SelectObject(mdc, ab), oP = SelectObject(mdc, ap);
        Polygon(mdc, tri, 3);
        SelectObject(mdc, oB); SelectObject(mdc, oP); DeleteObject(ab); DeleteObject(ap);

        wchar_t txt[256] = {};
        int sel = (int)SendMessage(hwnd, CB_GETCURSEL, 0, 0);
        if (sel >= 0) SendMessage(hwnd, CB_GETLBTEXT, sel, (LPARAM)txt);
        HFONT f = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
        HGDIOBJ oF = SelectObject(mdc, f ? f : GetStockObject(DEFAULT_GUI_FONT));
        SetBkMode(mdc, TRANSPARENT); SetTextColor(mdc, CLR_TEXT_BRIGHT);
        RECT tr = { 8, 0, btn.left - 4, h };
        DrawText(mdc, txt, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        SelectObject(mdc, oF);

        HBRUSH fb = CreateSolidBrush(CLR_ACCENT_DIM);
        RECT fr = rc; FrameRect(mdc, &fr, fb); InflateRect(&fr, -1, -1); FrameRect(mdc, &fr, fb);
        DeleteObject(fb);

        BitBlt(out, 0, 0, w, h, mdc, 0, 0, SRCCOPY);
        SelectObject(mdc, ob); DeleteObject(bmp); DeleteDC(mdc);
        if (msg == WM_PAINT) EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_KEYDOWN: case WM_KEYUP:
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_ENABLE: case CB_SETCURSEL: case CB_SHOWDROPDOWN: {
        LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
        return r;
    }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK CheckSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_NCDESTROY: RemoveWindowSubclass(hwnd, CheckSubclassProc, 1); break;
    case WM_PAINT:
    case WM_PRINTCLIENT: {
        PAINTSTRUCT ps; HDC hdcOut = (msg == WM_PAINT) ? BeginPaint(hwnd, &ps) : (HDC)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;
        HDC mdc = CreateCompatibleDC(hdcOut);
        HBITMAP bmp = CreateCompatibleBitmap(hdcOut, w, h);
        HGDIOBJ oldBmp = SelectObject(mdc, bmp);

        COLORREF bgc = PaintControlBackdrop(hwnd, mdc, w, h);

        bool en = IsWindowEnabled(hwnd) != FALSE;
        bool checked = SendMessage(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
        COLORREF ink = en ? CLR_TEXT : RGB(70, 170, 95);
        COLORREF edge = en ? CLR_ACCENT : RGB(40, 110, 60);

        const int bs = 12; int by = (h - bs) / 2;
        RECT box = { 1, by, 1 + bs, by + bs };
        PaintRoundedRect(mdc, box, 3, bgc, edge, 1);
        if (checked) {
            HPEN pen = CreatePen(PS_SOLID, 2, en ? CLR_ACCENT : RGB(70, 170, 95));
            HGDIOBJ op = SelectObject(mdc, pen);
            MoveToEx(mdc, box.left + 3, by + 7, nullptr);
            LineTo(mdc, box.left + 6, by + 10);
            LineTo(mdc, box.left + 11, by + 4);
            SelectObject(mdc, op); DeleteObject(pen);
        }

        wchar_t txt[256] = {}; GetWindowText(hwnd, txt, 256);
        HFONT f = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
        HGDIOBJ oldF = SelectObject(mdc, f ? f : GetStockObject(DEFAULT_GUI_FONT));
        SetBkMode(mdc, TRANSPARENT); SetTextColor(mdc, ink);
        RECT tr = { bs + 9, 0, w, h };
        DrawText(mdc, txt, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(mdc, oldF);

        BitBlt(hdcOut, 0, 0, w, h, mdc, 0, 0, SRCCOPY);
        SelectObject(mdc, oldBmp); DeleteObject(bmp); DeleteDC(mdc);
        if (msg == WM_PAINT) EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK: case WM_KEYDOWN: case WM_KEYUP:
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_ENABLE: case WM_SETTEXT: case BM_SETCHECK: case BM_CLICK: {
        LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
        return r;
    }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// MAIN WINDOW
// ============================================================================
static const int kCardSourceH = 240;
static const int kCardStyleH = 220;
static const int kCardEncH = 124;
static const int kCardOptH = 328;
static const int kCardSymH = 96;
static const int kCardActH = 104;
static const int kCardGap = 12;

static COLORREF PaintControlBackdrop(HWND child, HDC mdc, int w, int h) {
    HWND parent = GetParent(child);
    RECT rc; GetWindowRect(child, &rc);
    MapWindowPoints(HWND_DESKTOP, parent, (LPPOINT)&rc, 2);
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;

    int cardTop = -1, cardBottom = -1;

    int lx1 = ColAX(), lx2 = lx1 + ColAW();
    int rx1 = RightX(), rx2 = rx1 + RightW();
    if (cx >= lx1 && cx < lx2) {
        const int t0 = 76;
        const int tops[4] = {
            t0,
            t0 + kCardSourceH + kCardGap,
            t0 + kCardSourceH + kCardGap + kCardStyleH + kCardGap,
            g_optCardTop
        };
        const int hs[4] = { kCardSourceH, kCardStyleH, kCardEncH, g_optCardH };
        for (int i = 0; i < 4; i++) {
            if (cy >= tops[i] && cy < tops[i] + hs[i]) {
                cardTop = tops[i]; cardBottom = tops[i] + hs[i];
                break;
            }
        }
    }
    else if (cx >= rx1 && cx < rx2) {
        const int t0 = 76;
        if (cy >= t0 && cy < t0 + kCardSymH) {
            cardTop = t0; cardBottom = t0 + kCardSymH;
        }
        else if (cy >= g_actionCardTop && cy < g_actionCardTop + kCardActH) {
            cardTop = g_actionCardTop; cardBottom = g_actionCardTop + kCardActH;
        }
        else if (cy >= g_logCardTop && cy < g_logCardBottom) {
            cardTop = g_logCardTop; cardBottom = g_logCardBottom;
        }
    }

    COLORREF c = CLR_CTRL_BG;
    if (cardTop >= 0 && cardBottom > cardTop) {
        double t = (double)(cy - cardTop) / (double)(cardBottom - cardTop);
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        c = LerpColor(CLR_CARD_TOP, CLR_CARD_BOTTOM, t);
    }

    HBRUSH br = CreateSolidBrush(c);
    RECT fill = { 0, 0, w, h };
    FillRect(mdc, &fill, br);
    DeleteObject(br);
    return c;
}

static LRESULT CALLBACK CardLabelSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR dimText) {
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_NCDESTROY: RemoveWindowSubclass(hwnd, CardLabelSubclassProc, 4); break;
    case WM_PAINT:
    case WM_PRINTCLIENT: {
        PAINTSTRUCT ps; HDC out = (msg == WM_PAINT) ? BeginPaint(hwnd, &ps) : (HDC)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;
        HDC mdc = CreateCompatibleDC(out);
        HBITMAP bmp = CreateCompatibleBitmap(out, w > 0 ? w : 1, h > 0 ? h : 1);
        HGDIOBJ ob = SelectObject(mdc, bmp);
        PaintControlBackdrop(hwnd, mdc, w, h);
        wchar_t txt[256] = {};
        GetWindowTextW(hwnd, txt, 256);
        HFONT f = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
        HGDIOBJ of = SelectObject(mdc, f ? f : GetStockObject(DEFAULT_GUI_FONT));
        SetBkMode(mdc, TRANSPARENT);
        SetTextColor(mdc, dimText ? CLR_TEXT_DIM : CLR_TEXT);
        DrawTextW(mdc, txt, -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(mdc, of);
        BitBlt(out, 0, 0, w, h, mdc, 0, 0, SRCCOPY);
        SelectObject(mdc, ob); DeleteObject(bmp); DeleteDC(mdc);
        if (msg == WM_PAINT) EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SETTEXT: {
        LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);
        InvalidateRect(hwnd, NULL, FALSE);
        return r;
    }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static void LayoutControls(HWND hwnd) {
    RECT cr; GetClientRect(hwnd, &cr);
    g_clientW = cr.right; g_clientH = cr.bottom;

    int axc = ColAX(), awc = ColAW();
    int ax = axc + CardInner();
    int aw = awc - CardInner() * 2;
    int ay = 76;

    MoveWindow(hComboLang, ax, ay + 32, aw, 150, TRUE);
    MoveWindow(hEditPath, ax, ay + 92, aw, 84, TRUE);
    MoveWindow(hBtnBrowse, ax, ay + 184, 90, 22, TRUE);
    ay += kCardSourceH + kCardGap;

    {
        int rowH3 = 24, colGap = 8;
        int thirdW = (aw - colGap * 2) / 3;
        int sy = ay + 20;
        MoveWindow(hChkO0o, ax, sy, thirdW, rowH3, TRUE);
        MoveWindow(hChkVWV, ax + thirdW + colGap, sy, thirdW, rowH3, TRUE);
        MoveWindow(hChkI1L, ax + (thirdW + colGap) * 2, sy, thirdW, rowH3, TRUE);
        sy += rowH3 + 24;
        MoveWindow(hLblAdvanced2, ax, sy, aw, 14, TRUE);
        sy += 14 + 26;
        MoveWindow(hChkDeadCode, ax, sy, aw, rowH3, TRUE);
        sy += rowH3 + 16;
        MoveWindow(hChkMix, ax, sy, aw, rowH3, TRUE);
        sy += rowH3 + 16;
        MoveWindow(hCmbCount, ax, sy, 160, 20, TRUE);
    }
    ay += kCardStyleH + kCardGap;

    {
        int rowH3 = 18, colGap = 8;
        int halfW = (aw - colGap) / 2;
        int sy = ay + 16;
        MoveWindow(hChkStrEnc, ax, sy, halfW, rowH3, TRUE);
        MoveWindow(hChkInjectDecrypt, ax + halfW + colGap, sy, halfW, rowH3, TRUE);
        sy += rowH3 + 14;
        MoveWindow(hChkSkipEncrypted, ax, sy, halfW, rowH3, TRUE);
        MoveWindow(hChkStrExcl, ax + halfW + colGap, sy, halfW, rowH3, TRUE);
        sy += rowH3 + 16;
        MoveWindow(hEditStrExcl, ax, sy, aw, 20, TRUE);
    }
    ay += kCardEncH + kCardGap;

    g_optCardTop = ay;
    g_optCardH = g_clientH - 24 - g_optCardTop;
    if (g_optCardH < kCardOptH) g_optCardH = kCardOptH;
    {
        int rowH = (g_optCardH - 44) / 12;
        if (rowH < 20) rowH = 20;
        if (rowH > 32) rowH = 32;
        int row = g_optCardTop + 16;
        HWND optChks[] = {
            hChkClasses, hChkControls, hChkPublic, hChkCustom, hChkSkipExternal,
            hChkMacros, hChkLabels, hChkOperators, hChkNamespaces,
            hChkTypedefs, hChkTemplates, hChkPrecomp
        };
        for (int i = 0; i < 12; i++) MoveWindow(optChks[i], ax, row + rowH * i, aw, 20, TRUE);
        MoveWindow(hEditCustom, ax, row + rowH * 12, aw, 22, TRUE);
    }
    ay += g_optCardH;

    g_stackHCache = ay - 92;

    int bxc = RightX(), bwc = RightW();
    int bx = bxc + CardInner();
    int bw = bwc - CardInner() * 2;
    int by = 76;

    MoveWindow(hSegEmbedded, bx, by + 36, bw / 2 - 8, 26, TRUE);
    MoveWindow(hSegGenerate, bx + bw / 2 + 8, by + 36, bw / 2 - 8, 26, TRUE);
    MoveWindow(hBtnRefresh, bx, by + 66, bw, 24, TRUE);
    by += kCardSymH + kCardGap;

    MoveWindow(hBtnObfuscate, bx, by + 36, bw, 30, TRUE);
    MoveWindow(hProgress, bx, by + 72, bw, 6, TRUE);
    MoveWindow(hStatus, bx, by + 82, bw, 16, TRUE);
    g_actionCardTop = by;
    by += kCardActH + kCardGap;

    int rxc = RightX(), rwc = RightW();
    g_logCardTop = by;
    int logTop = g_logCardTop + 34;
    int logBottom = g_optCardTop + g_optCardH;
    g_logCardBottom = logBottom;
    g_cardLogHCache = logBottom - g_logCardTop;

    int fullLogH = logBottom - logTop - 1;
    double t = g_logAnimT; if (t < 0.0) t = 0.0; if (t > 1.0) t = 1.0;
    int btnReserve = kShowLogBtnH + 8;
    int curLogH = fullLogH - (int)(btnReserve * t + 0.5);
    if (curLogH < 20) curLogH = 20;
    int curTop = logTop;
    int showLogY = logBottom - kShowLogBtnH - 8;

    MoveWindow(hLog, rxc + 1, curTop, rwc - 2, curLogH, TRUE);

    int logLen = GetWindowTextLength(hLog);
    SendMessage(hLog, EM_SETSEL, logLen, logLen);
    SendMessage(hLog, EM_SCROLLCARET, 0, 0);

    MoveWindow(hBtnShowLog, rxc + 1, showLogY, rwc - 2, kShowLogBtnH, TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        InitFonts();

        hTitleStatic = NULL;
        hVersion = NULL;

        SetTimer(hwnd, 1, 60, NULL);
        g_animStart = GetTickCount64();

        hComboLang = CreateWindow(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL, 0, 0, 100, 150, hwnd, (HMENU)IDC_LANG_COMBO, GetModuleHandle(NULL), NULL);
        SetWindowSubclass(hComboLang, ComboSubclassProc, 3, 0);
        SendMessage(hComboLang, CB_SETITEMHEIGHT, (WPARAM)-1, 20);
        SendMessage(hComboLang, CB_SETITEMHEIGHT, 0, 20);
        SetWindowTheme(hComboLang, L"", L"");
        SendMessage(hComboLang, WM_SETFONT, (WPARAM)hFontUI, TRUE);
        SendMessage(hComboLang, CB_ADDSTRING, 0, (LPARAM)L"VB.NET (.vb)");
        SendMessage(hComboLang, CB_ADDSTRING, 0, (LPARAM)L"C# (.cs)");
        SendMessage(hComboLang, CB_ADDSTRING, 0, (LPARAM)L"C++ (.cpp/.h)");
        SendMessage(hComboLang, CB_SETCURSEL, 2, 0);

        hEditPath = CreateWindowEx(0, L"LISTBOX", L"", WS_VISIBLE | WS_CHILD | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
            0, 0, 100, 84, hwnd, (HMENU)IDC_FILEPATH, GetModuleHandle(NULL), NULL);
        SendMessage(hEditPath, WM_SETFONT, (WPARAM)hFontTiny, TRUE);

        hBtnBrowse = CreateWindow(BTN_BROWSE_CLASS, L"", WS_VISIBLE | WS_CHILD | WS_TABSTOP, 0, 0, 90, 24, hwnd, (HMENU)IDC_BROWSE, GetModuleHandle(NULL), NULL);

        auto mkCheck = [&](const wchar_t* text, int id, bool checked) {
            HWND h = CreateWindow(L"BUTTON", text, WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 100, 22, hwnd, (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
            SendMessage(h, WM_SETFONT, (WPARAM)hFontUI, TRUE);
            SetWindowTheme(h, L"", L"");
            SetWindowSubclass(h, CheckSubclassProc, 1, 0);
            if (checked) SendMessage(h, BM_SETCHECK, BST_CHECKED, 0);
            return h;
            };

        hChkClasses = mkCheck(L"Exclude class names", IDC_CHK_CLASSES, true);
        hChkControls = mkCheck(L"Exclude control names", IDC_CHK_CONTROLS, false);
        hChkPublic = mkCheck(L"Skip public declarations", IDC_CHK_PUBLIC, true);
        hChkCustom = mkCheck(L"Exclude custom names", IDC_CHK_CUSTOM, false);

        hEditCustom = CreateWindowEx(0, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 0, 0, 100, 22, hwnd, (HMENU)IDC_CUSTOM_NAMES, GetModuleHandle(NULL), NULL);
        SendMessage(hEditCustom, WM_SETFONT, (WPARAM)hFontMonoSmall, TRUE);
        SendMessage(hEditCustom, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(6, 6));
        EnableWindow(hEditCustom, FALSE);

        hChkMacros = mkCheck(L"Preserve macros", IDC_CHK_MACROS, true);
        hChkLabels = mkCheck(L"Preserve labels", IDC_CHK_LABELS, true);
        hChkOperators = mkCheck(L"Preserve operators", IDC_CHK_OPERATORS, true);
        hChkNamespaces = mkCheck(L"Preserve namespaces", IDC_CHK_NAMESPACES, true);
        hChkTypedefs = mkCheck(L"Preserve typedefs", IDC_CHK_TYPEDEFS, true);
        hChkTemplates = mkCheck(L"Preserve templates", IDC_CHK_TEMPLATES, true);
        hChkPrecomp = mkCheck(L"Skip preprocessor #if 0 blocks", IDC_CHK_PRECOMP, true);
        for (HWND h : { hChkMacros, hChkLabels, hChkOperators, hChkNamespaces, hChkTypedefs, hChkTemplates, hChkPrecomp })
            EnableWindow(h, TRUE);

        EnableWindow(hChkControls, FALSE);
        EnableWindow(hChkPublic, TRUE);

        hChkO0o = mkCheck(L"O0o", IDC_CHK_O0O, true);
        hChkVWV = mkCheck(L"vWw", IDC_CHK_VWV, false);
        hChkI1L = mkCheck(L"I1lL", IDC_CHK_I1L, false);

        hLblAdvanced2 = CreateWindow(L"STATIC", L"[ Protection ]", WS_VISIBLE | WS_CHILD | SS_LEFT, 0, 0, 260, 18, hwnd, NULL, GetModuleHandle(NULL), NULL);
        SendMessage(hLblAdvanced2, WM_SETFONT, (WPARAM)hFontSection, TRUE);
        SetWindowSubclass(hLblAdvanced2, CardLabelSubclassProc, 4, 0);

        hChkDeadCode = mkCheck(L"Dead code injection", IDC_CHK_DEADCODE, false);
        hChkMix = mkCheck(L"Interleave code", IDC_CHK_MIX, false);

        hCmbCount = CreateWindow(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL, 0, 0, 100, 72, hwnd, (HMENU)IDC_CMB_COUNT, GetModuleHandle(NULL), NULL);
        SetWindowSubclass(hCmbCount, ComboSubclassProc, 3, 0);
        SendMessage(hCmbCount, CB_SETITEMHEIGHT, (WPARAM)-1, 16);
        SetWindowTheme(hCmbCount, L"", L"");
        SendMessage(hCmbCount, WM_SETFONT, (WPARAM)hFontUI, TRUE);
        for (const wchar_t* s : { L"3 junk members", L"6 junk members", L"9 junk members", L"12 junk members" })
            SendMessage(hCmbCount, CB_ADDSTRING, 0, (LPARAM)s);
        SendMessage(hCmbCount, CB_SETCURSEL, 1, 0);

        hChkStrEnc = mkCheck(L"String encryption", IDC_CHK_STRENC, false);
        hChkInjectDecrypt = mkCheck(L"Inject decrypt module", IDC_CHK_INJECT_DECRYPT, false);
        hChkSkipEncrypted = mkCheck(L"Skip already-encrypted strings", IDC_CHK_SKIP_ENCRYPTED, true);
        hChkSkipExternal = mkCheck(L"Skip external members", IDC_CHK_SKIP_EXTERNAL, false);
        hChkStrExcl = mkCheck(L"Custom string exclusion list", IDC_CHK_STREXCL, false);

        hEditStrExcl = CreateWindowEx(0, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 0, 0, 100, 22, hwnd, (HMENU)IDC_EDIT_STREXCL, GetModuleHandle(NULL), NULL);
        SendMessage(hEditStrExcl, WM_SETFONT, (WPARAM)hFontMonoSmall, TRUE);
        SendMessage(hEditStrExcl, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(6, 6));
        EnableWindow(hEditStrExcl, FALSE);

        for (HWND h : { hChkDeadCode, hChkMix, hCmbCount, hChkStrEnc, hChkInjectDecrypt, hChkSkipEncrypted, hChkSkipExternal, hChkStrExcl, hEditStrExcl })
            EnableWindow(h, FALSE);

        hSegEmbedded = CreateWindow(BTN_SEG_CLASS, L"Embedded", WS_VISIBLE | WS_CHILD | WS_TABSTOP, 0, 0, 100, 34, hwnd, (HMENU)IDC_BTN_EMBEDDED, GetModuleHandle(NULL), NULL);
        hSegGenerate = CreateWindow(BTN_SEG_CLASS, L"Generate", WS_VISIBLE | WS_CHILD | WS_TABSTOP, 0, 0, 100, 34, hwnd, (HMENU)IDC_BTN_GENERATE, GetModuleHandle(NULL), NULL);
        hBtnRefresh = CreateWindow(L"BUTTON", L"Refresh / Regenerate symbols", WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 100, 24, hwnd, (HMENU)IDC_BTN_REFRESH, GetModuleHandle(NULL), NULL);
        SendMessage(hBtnRefresh, WM_SETFONT, (WPARAM)hFontUI, TRUE);
        hBtnObfuscate = CreateWindow(BTN_ACTION_CLASS, L"", WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_DISABLED, 0, 0, 100, 30, hwnd, (HMENU)IDC_OBFUSCATE, GetModuleHandle(NULL), NULL);

        hProgress = CreateWindow(PROGRESS_CLASS, L"", WS_VISIBLE | WS_CHILD | PBS_SMOOTH, 0, 0, 100, 8, hwnd, (HMENU)IDC_PROGRESS, GetModuleHandle(NULL), NULL);
        SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessage(hProgress, PBM_SETPOS, 0, 0);
        SendMessage(hProgress, PBM_SETBKCOLOR, 0, (LPARAM)RGB(20, 30, 20));
        SendMessage(hProgress, PBM_SETBARCOLOR, 0, (LPARAM)CLR_ACCENT);
        ShowWindow(hProgress, SW_HIDE);

        hStatus = CreateWindow(L"STATIC", L"Ready", WS_VISIBLE | WS_CHILD | SS_LEFT, 0, 0, 100, 20, hwnd, (HMENU)IDC_STATUS, GetModuleHandle(NULL), NULL);
        SetWindowSubclass(hStatus, CardLabelSubclassProc, 4, 1);
        SendMessage(hStatus, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

        LoadLibrary(L"Msftedit.dll");
        hLog = CreateWindowEx(0, MSFTEDIT_CLASS, L"", WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 100, 100, hwnd, (HMENU)IDC_LOG, GetModuleHandle(NULL), NULL);
        SendMessage(hLog, WM_SETFONT, (WPARAM)hFontMono, TRUE);
        SendMessage(hLog, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(6, 6));
        SendMessage(hLog, EM_SETBKGNDCOLOR, 0, (LPARAM)CLR_LOG_BG);

        hBtnShowLog = CreateWindow(L"BUTTON", L"Show Log", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 100, kShowLogBtnH, hwnd, (HMENU)IDC_BTN_SHOWLOG, GetModuleHandle(NULL), NULL);

        LogMessage(L"// Obfuscator v3.3 ready", CLR_ACCENT);
        LogMessage(L"// symbol source: .\\elements\\", CLR_TEXT_DIM);
        LogSymbolCounts(L"// loaded");
        {
            for (HWND h : { hChkDeadCode, hChkMix, hCmbCount, hChkStrEnc })
                EnableWindow(h, TRUE);
            EnableWindow(hChkControls, TRUE);
            EnableWindow(hChkSkipExternal, TRUE); // now applies to VB, C#, and C++
            UpdateStringEncryptionDependents();
        }

        if (!g_currentProject.files.empty()) {
            int sel = (g_currentProject.lang == Lang::VB) ? 0 :
                (g_currentProject.lang == Lang::CSharp) ? 1 : 2;
            SendMessage(hComboLang, CB_SETCURSEL, sel, 0);
            PopulateFileList(g_currentProject);
            EnableWindow(hBtnObfuscate, TRUE);
            EnableWindow(hBtnRescanRefs, !g_currentProject.projectFiles.empty());
            LogMessage(L"// project loaded: " + to_wstring(g_currentProject.files.size()) + L" files", CLR_INFO);
        }
        if (g_pendingSingleFileBrowse) {
            g_pendingSingleFileBrowse = false;
            PostMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_BROWSE, BN_CLICKED), 0);
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        wchar_t cls[16] = {};
        GetClassNameW(hCtrl, cls, 16);
        if (_wcsicmp(cls, L"Edit") == 0) {
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, CLR_EDIT_BG_OFF);
            SetTextColor(hdc, CLR_TEXT_DIM);
            static HBRUSH s_brOff = CreateSolidBrush(CLR_EDIT_BG_OFF);
            return (LRESULT)s_brOff;
        }
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, (hCtrl == hStatus) ? CLR_TEXT_DIM : CLR_TEXT);
        return (LRESULT)GetStockObject(HOLLOW_BRUSH);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, CLR_EDIT_BG);
        SetTextColor(hdc, CLR_TEXT_BRIGHT);
        static HBRUSH s_br = CreateSolidBrush(CLR_EDIT_BG);
        return (LRESULT)s_br;
    }
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, CLR_EDIT_BG);
        SetTextColor(hdc, CLR_TEXT_BRIGHT);
        static HBRUSH s_br = CreateSolidBrush(CLR_EDIT_BG);
        return (LRESULT)s_br;
    }
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mi = (MEASUREITEMSTRUCT*)lParam;
        if (mi && mi->CtlType == ODT_COMBOBOX) { mi->itemHeight = 20; return TRUE; }
        break;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lParam;
        if (di && (di->CtlID == IDC_LANG_COMBO || di->CtlID == IDC_CMB_COUNT)) {
            if ((int)di->itemID < 0) return TRUE;
            bool hot = (di->itemState & ODS_SELECTED) != 0;
            HBRUSH br = CreateSolidBrush(hot ? CLR_ACCENT : CLR_CTRL_BG);
            FillRect(di->hDC, &di->rcItem, br); DeleteObject(br);
            wchar_t t[256] = {};
            SendMessage(di->hwndItem, CB_GETLBTEXT, di->itemID, (LPARAM)t);
            SetBkMode(di->hDC, TRANSPARENT);
            SetTextColor(di->hDC, hot ? RGB(0, 60, 20) : CLR_TEXT);
            HFONT oF = (HFONT)SelectObject(di->hDC, hFontUI);
            RECT tr = di->rcItem; tr.left += 8;
            DrawText(di->hDC, t, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(di->hDC, oF);
            return TRUE;
        }
        if (di && (di->CtlID == IDC_BTN_REFRESH || di->CtlID == IDC_BTN_SHOWLOG || di->CtlID == IDC_BTN_RESCAN_REFS)) {
            RECT rc = di->rcItem;
            int w = rc.right - rc.left, h = rc.bottom - rc.top;
            HDC mdc = CreateCompatibleDC(di->hDC);
            HBITMAP bmp = CreateCompatibleBitmap(di->hDC, w, h);
            HGDIOBJ ob = SelectObject(mdc, bmp);
            HBRUSH cb = CreateSolidBrush(CLR_CARD_BOTTOM); RECT all = { 0, 0, w, h }; FillRect(mdc, &all, cb); DeleteObject(cb);
            bool pressed = (di->itemState & ODS_SELECTED) != 0;
            POINT pt; GetCursorPos(&pt); ScreenToClient(di->hwndItem, &pt);
            bool hot = PtInRect(&all, pt) != FALSE;
            COLORREF fill = pressed ? CLR_ACCENT : (hot ? RGB(0, 200, 65) : CLR_SEG_ON);
            PaintRoundedRect(mdc, all, 6, fill, CLR_ACCENT, 1);
            SetBkMode(mdc, TRANSPARENT); SetTextColor(mdc, RGB(0, 0, 0));
            HFONT oF = (HFONT)SelectObject(mdc, hFontBold);
            wchar_t buf[128]; GetWindowText(di->hwndItem, buf, 128);
            DrawText(mdc, buf, -1, &all, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(mdc, oF);
            BitBlt(di->hDC, rc.left, rc.top, w, h, mdc, 0, 0, SRCCOPY);
            SelectObject(mdc, ob); DeleteObject(bmp); DeleteDC(mdc);
            return TRUE;
        }
        break;
    }
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH br = CreateSolidBrush(CLR_BG);
        FillRect(hdc, &rc, br);
        DeleteObject(br);
        return TRUE;
    }

    case WM_NCCALCSIZE:
        if (wParam == TRUE) return 0;
        break;

    case WM_TIMER: {
        if (wParam == 1) {
            g_animTick++;
            if (g_logCollapsed && g_logAnimT < 1.0) {
                g_logAnimT += 0.22;
                if (g_logAnimT >= 1.0) { g_logAnimT = 1.0; ShowWindow(hBtnShowLog, SW_SHOW); }
                LayoutControls(hwnd);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lParam), my = GET_Y_LPARAM(lParam);
        POINT pt = { mx, my };
        if (PtInRect(&g_btnCloseRect, pt)) { PostMessage(hwnd, WM_CLOSE, 0, 0); return 0; }
        if (PtInRect(&g_btnMinRect, pt)) { ShowWindow(hwnd, SW_MINIMIZE); return 0; }
        if (my < HEADER_H) {
            POINT scr; GetCursorPos(&scr);
            ReleaseCapture();
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(scr.x, scr.y));
            return 0;
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdcScreen = BeginPaint(hwnd, &ps);
        RECT cr; GetClientRect(hwnd, &cr);
        HDC hdc = CreateCompatibleDC(hdcScreen);
        HBITMAP bmpBuf = CreateCompatibleBitmap(hdcScreen, cr.right > 0 ? cr.right : 1, cr.bottom > 0 ? cr.bottom : 1);
        HGDIOBJ oldBuf = SelectObject(hdc, bmpBuf);

        {
            TRIVERTEX vert[2] = {};
            vert[0].x = cr.left; vert[0].y = cr.top;
            vert[0].Red = (COLOR16)(GetRValue(CLR_BG) << 8); vert[0].Green = (COLOR16)(GetGValue(CLR_BG) << 8); vert[0].Blue = (COLOR16)(GetBValue(CLR_BG) << 8);
            vert[1].x = cr.right; vert[1].y = cr.bottom;
            vert[1].Red = (COLOR16)(GetRValue(CLR_BG_LO) << 8); vert[1].Green = (COLOR16)(GetGValue(CLR_BG_LO) << 8); vert[1].Blue = (COLOR16)(GetBValue(CLR_BG_LO) << 8);
            GRADIENT_RECT gr = { 0, 1 };
            GradientFill(hdc, vert, 2, &gr, 1, GRADIENT_FILL_RECT_V);
        }

        {
            const int cw = cr.right, ch = cr.bottom;
            if (cw > 0 && ch > 0) {
                ULONGLONG eff = (GetTickCount64() - g_animStart) / 16ull;
                bool wrapped = (eff < g_lastEff);
                bool clean = wrapped || !g_glitchEnabled;

                bool rolling = false;
                double boost = 1.0;
                if (!clean) {
                    rolling = (eff < g_rollUntil);
                    if (!rolling && eff >= g_rollNext) {
                        g_rollUntil = eff + 10;
                        std::uniform_int_distribution<unsigned long long> rollD(180, 300);
                        g_rollNext = eff + rollD(g_glitchRng);
                    }
                    boost = rolling ? 1.5 : 1.0;
                }

                {
                    int sweepY = (int)((eff + (rolling ? (unsigned long long)ch / 2 : 0ull))
                        % (unsigned long long)ch);
                    for (int d = -2; d <= 2; d++) {
                        int y = sweepY + d;
                        if (y < 0 || y >= ch) continue;
                        double fade = 1.0 - (std::abs(d) / 3.0);
                        if (fade <= 0.0) continue;
                        double k = fade * 0.35 * boost;
                        if (k > 1.0) k = 1.0;
                        HPEN pen = CreatePen(PS_SOLID, 1, LerpColor(CLR_BG, CLR_ACCENT, k));
                        HGDIOBJ op = SelectObject(hdc, pen);
                        MoveToEx(hdc, 0, y, nullptr);
                        LineTo(hdc, cw, y);
                        SelectObject(hdc, op);
                        DeleteObject(pen);
                    }
                }

                if (!clean) {
                    if (eff >= g_tearNext) {
                        std::uniform_int_distribution<int> cntD(1, 3), yD(0, ch - 1),
                            hD(2, 10), magD(8, 40), signD(0, 1), lifeD(4, 8), gapD(40, 90);
                        g_tearCount = cntD(g_glitchRng);
                        for (int i = 0; i < g_tearCount && i < 3; i++) {
                            int y = yD(g_glitchRng);
                            int h = hD(g_glitchRng);
                            if (y + h > ch) h = ch - y;
                            g_tears[i].y = y;
                            g_tears[i].h = h;
                            g_tears[i].dx = magD(g_glitchRng) * (signD(g_glitchRng) ? 1 : -1);
                        }
                        g_tearUntil = eff + (unsigned long long)lifeD(g_glitchRng);
                        g_tearNext = g_tearUntil + (unsigned long long)gapD(g_glitchRng);
                    }
                    if (eff < g_tearUntil) {
                        for (int i = 0; i < g_tearCount && i < 3; i++) {
                            BitBlt(hdc, g_tears[i].dx, g_tears[i].y, cw, g_tears[i].h,
                                hdc, 0, g_tears[i].y, SRCCOPY);
                        }
                    }

                    if (eff >= g_noiseNext) {
                        std::uniform_int_distribution<int> nD(30, 60), xD(0, cw - 1),
                            lenD(4, 24), yD(0, ch - 1);
                        std::uniform_real_distribution<double> kD(0.15, 0.55);
                        g_noiseCount = nD(g_glitchRng);
                        if (g_noiseCount > 60) g_noiseCount = 60;
                        for (int i = 0; i < g_noiseCount; i++) {
                            int x = xD(g_glitchRng);
                            int len = lenD(g_glitchRng);
                            if (x + len > cw) len = cw - x;
                            g_noiseSegs[i].x = x;
                            g_noiseSegs[i].len = len;
                            g_noiseSegs[i].y = yD(g_glitchRng);
                            g_noiseSegs[i].k = kD(g_glitchRng);
                        }
                        g_noiseNext = eff + 6;
                    }
                    for (int i = 0; i < g_noiseCount; i++) {
                        HPEN pen = CreatePen(PS_SOLID, 1, LerpColor(CLR_BG, CLR_ACCENT, g_noiseSegs[i].k));
                        HGDIOBJ op = SelectObject(hdc, pen);
                        MoveToEx(hdc, g_noiseSegs[i].x, g_noiseSegs[i].y, nullptr);
                        LineTo(hdc, g_noiseSegs[i].x + g_noiseSegs[i].len, g_noiseSegs[i].y);
                        SelectObject(hdc, op);
                        DeleteObject(pen);
                    }

                    if (eff % 20 < 3) {
                        if (eff % 40 == 0) {
                            std::uniform_int_distribution<int> gD(2, 4), sD(0, 1);
                            g_ghostDx = gD(g_glitchRng) * (sD(g_glitchRng) ? 1 : -1);
                        }
                        int gy = (int)(eff % (unsigned long long)ch);
                        HPEN pen = CreatePen(PS_SOLID, 1, LerpColor(CLR_BG, RGB(255, 40, 40), 0.15));
                        HGDIOBJ op = SelectObject(hdc, pen);
                        MoveToEx(hdc, g_ghostDx, gy, nullptr);
                        LineTo(hdc, cw + g_ghostDx, gy);
                        SelectObject(hdc, op);
                        DeleteObject(pen);
                    }
                }

                g_lastEff = eff;
            }
        }

        {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, CLR_TEXT);
            HGDIOBJ oF = SelectObject(hdc, hFontTitle);
            TextOutW(hdc, 24, 14, L"root@obfuscator:~#", 18);
            SelectObject(hdc, hFontSmall);
            const wchar_t* sub = L"// rename obfuscator :: vb.net / c# / c++";
            TextOutW(hdc, 24, 42, sub, (int)wcslen(sub));
            RECT rv = { cr.right - 24 - 160, 34, cr.right - 24, 50 };
            DrawTextW(hdc, L"v3.3", -1, &rv, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(hdc, oF);
        }
        int axc = ColAX(), awc = ColAW();
        int ay = 76;
        int srcTop = ay;
        RECT rSrc = { axc, ay, axc + awc, ay + kCardSourceH };
        PaintCard(hdc, rSrc, L""); ay += kCardSourceH + kCardGap;

        RECT rStyle = { axc, ay, axc + awc, ay + kCardStyleH };
        PaintCard(hdc, rStyle, L""); ay += kCardStyleH + kCardGap;

        RECT rEnc = { axc, ay, axc + awc, ay + kCardEncH };
        PaintCard(hdc, rEnc, L""); ay += kCardEncH + kCardGap;

        RECT rOpt = { axc, g_optCardTop, axc + awc, g_optCardTop + g_optCardH };
        PaintCard(hdc, rOpt, L"");

        int bxc = RightX(), bwc = RightW();
        int by = 76;
        RECT rSym = { bxc, by, bxc + bwc, by + kCardSymH };
        PaintCard(hdc, rSym, L"SYMBOL SOURCE"); by += kCardSymH + kCardGap;

        RECT rAct = { bxc, g_actionCardTop, bxc + bwc, g_actionCardTop + kCardActH };
        {
            double phase = (sin(g_animTick * 0.05) + 1.0) * 0.5;
            COLORREF glowCore = LerpColor(CLR_ACCENT_DIM, CLR_ACCENT, phase);
            PaintGlowRing(hdc, rAct, glowCore, CLR_BG, 4);
        }
        PaintCard(hdc, rAct, L"ACTION");

        RECT rLog = { bxc, g_logCardTop, bxc + bwc, g_logCardTop + g_cardLogHCache };
        PaintCard(hdc, rLog, L"LOG");

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT);
        HFONT oF = (HFONT)SelectObject(hdc, hFontUI);
        TextOut(hdc, axc + CardInner(), srcTop + 10, L"Language", 8);
        TextOut(hdc, axc + CardInner(), srcTop + 74, L"File", 4);
        SelectObject(hdc, oF);

        {
            int bw = 26, bh = 20, gap = 6, top = 14;
            g_btnCloseRect = { g_clientW - 24 - bw, top, g_clientW - 24, top + bh };
            g_btnMinRect = { g_btnCloseRect.left - gap - bw, top, g_btnCloseRect.left - gap, top + bh };

            HFONT oF2 = (HFONT)SelectObject(hdc, hFontBold);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, CLR_TEXT_DIM);
            RECT rMin = g_btnMinRect, rClose = g_btnCloseRect;
            DrawText(hdc, L"_", 1, &rMin, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SetTextColor(hdc, CLR_ERROR);
            DrawText(hdc, L"X", 1, &rClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, oF2);
        }

        {
            RECT full = cr;
            full.right -= 1; full.bottom -= 1;
            HPEN p = CreatePen(PS_SOLID, 1, CLR_BORDER_HI);
            HGDIOBJ op = SelectObject(hdc, p);
            HGDIOBJ ob = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            RoundRect(hdc, full.left, full.top, full.right, full.bottom, 18, 18);
            SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(p);
        }

        {
            HBRUSH fb = CreateSolidBrush(CLR_ACCENT_DIM);
            for (HWND hc : { hEditPath, hEditCustom, hEditStrExcl }) {
                RECT r; GetWindowRect(hc, &r);
                MapWindowPoints(NULL, hwnd, (LPPOINT)&r, 2);
                InflateRect(&r, 2, 2); FrameRect(hdc, &r, fb);
                InflateRect(&r, -1, -1); FrameRect(hdc, &r, fb);
            }
            DeleteObject(fb);
        }

        BitBlt(hdcScreen, 0, 0, cr.right, cr.bottom, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, oldBuf); DeleteObject(bmpBuf); DeleteDC(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE: {
        LayoutControls(hwnd);
        RECT wr; GetWindowRect(hwnd, &wr);
        HRGN rgn = CreateRoundRectRgn(0, 0, wr.right - wr.left + 1, wr.bottom - wr.top + 1, 18, 18);
        SetWindowRgn(hwnd, rgn, TRUE);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)lParam;
        mm->ptMinTrackSize.x = 640;
        mm->ptMinTrackSize.y = 1080;
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam), code = HIWORD(wParam);
        if (id == IDC_CHK_CUSTOM && code == BN_CLICKED) { UpdateCustomTextboxState(); return 0; }
        if (id == IDC_CHK_STRENC && code == BN_CLICKED) { UpdateStringEncryptionDependents(); return 0; }
        if (id == IDC_CHK_STREXCL && code == BN_CLICKED) {
            BOOL c = (SendMessage(hChkStrExcl, BM_GETCHECK, 0, 0) == BST_CHECKED);
            BOOL strEncOn = (SendMessage(hChkStrEnc, BM_GETCHECK, 0, 0) == BST_CHECKED);
            EnableWindow(hEditStrExcl, c && strEncOn);
            if (!c) SetWindowText(hEditStrExcl, L"");
            return 0;
        }
        if (id == IDC_BTN_EMBEDDED && code == BN_CLICKED) {
            g_symbolMode = SymbolMode::Embedded;
            ApplyMode();
            LogMessage(L">> Embedded mode", CLR_INFO);
            LogSymbolCounts(L"  loaded");
            InvalidateRect(hSegEmbedded, NULL, FALSE);
            InvalidateRect(hSegGenerate, NULL, FALSE);
            return 0;
        }
        if (id == IDC_BTN_GENERATE && code == BN_CLICKED) {
            g_symbolMode = SymbolMode::Generate;
            LogMessage(L">> Generate mode (click Refresh to run grabber)", CLR_INFO);
            InvalidateRect(hSegEmbedded, NULL, FALSE);
            InvalidateRect(hSegGenerate, NULL, FALSE);
            return 0;
        }
        if (id == IDC_BTN_SHOWLOG && code == BN_CLICKED) {
            wstring folder = !g_lastRunLogFolder.empty() ? g_lastRunLogFolder : getLogsDir();
            if (!folder.empty()) ShellExecuteW(hwnd, L"open", folder.c_str(), NULL, NULL, SW_SHOWNORMAL);
            return 0;
        }
        if (id == IDC_BTN_REFRESH && code == BN_CLICKED) {
            if (g_symbolMode == SymbolMode::Generate) OnGenerateFromSystem();
            else OnRefreshSymbols();
            return 0;
        }
        if (id == IDC_BTN_RESCAN_REFS && code == BN_CLICKED) {
            // Part B4: re-runs B1-B3 without restarting the app.
            if (!g_projectLoaded || g_currentProject.projectFiles.empty()) {
                LogMessage(L"WARN: re-scan needs a loaded C#/VB.NET project or solution "
                    L"(Folder mode has no project file to read references from).", CLR_WARN);
                return 0;
            }
            if (g_obfuscationBusy.exchange(true)) {
                LogMessage(L"WARN: an obfuscation run is already in progress — please wait for it to finish.", CLR_WARN);
                return 0;
            }
            EnableWindow(hBtnRescanRefs, FALSE);
            std::thread([]() {
                wstring summary = RunProjectReferenceProtection(g_currentProject);
                if (!summary.empty()) LogMessage(summary, CLR_INFO);
                g_obfuscationBusy.store(false);
                EnableWindow(hBtnRescanRefs, TRUE);
                }).detach();
            return 0;
        }
        if (id == IDC_LANG_COMBO && code == CBN_SELCHANGE) {
            if (g_projectLoaded) {
                g_projectLoaded = false;
                g_currentProject = LoadedProject();
                g_singleFilePath.clear();
                SendMessage(hEditPath, LB_RESETCONTENT, 0, 0);
                EnableWindow(hBtnObfuscate, FALSE);
                EnableWindow(hBtnRescanRefs, FALSE);
            }
            Lang lang = GetSelectedLang();
            // "Exclude control names" and "Skip external members" now apply
            // to all three languages (VB, C#, C++), not just VB -- enable
            // both regardless of the selected language.
            EnableWindow(hChkControls, TRUE);
            EnableWindow(hChkPublic, lang != Lang::Cpp);
            BOOL isCpp = (lang == Lang::Cpp);
            EnableWindow(hChkMacros, isCpp); EnableWindow(hChkLabels, isCpp);
            EnableWindow(hChkOperators, isCpp); EnableWindow(hChkNamespaces, isCpp);
            EnableWindow(hChkTypedefs, isCpp); EnableWindow(hChkTemplates, isCpp);
            EnableWindow(hChkPrecomp, isCpp);
            for (HWND h : { hChkDeadCode, hChkMix, hCmbCount, hChkStrEnc })
                EnableWindow(h, TRUE);
            EnableWindow(hChkSkipExternal, TRUE);
            UpdateStringEncryptionDependents();
            LogMessage(wstring(L">> ") + langName(lang), CLR_INFO);
            return 0;
        }
        if ((id == IDC_CHK_O0O || id == IDC_CHK_VWV || id == IDC_CHK_I1L) && code == BN_CLICKED) {
            HWND clicked = (id == IDC_CHK_O0O) ? hChkO0o : (id == IDC_CHK_VWV) ? hChkVWV : hChkI1L;
            if (SendMessage(clicked, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                for (HWND h : { hChkO0o, hChkVWV, hChkI1L }) if (h != clicked) SendMessage(h, BM_SETCHECK, BST_UNCHECKED, 0);
            }
            else SendMessage(clicked, BM_SETCHECK, BST_CHECKED, 0);
            return 0;
        }
        return 0;
    }

    case WM_DESTROY: {
        KillTimer(hwnd, 1);
        DeleteObject(hFontUI); DeleteObject(hFontMono); DeleteObject(hFontMonoSmall);
        DeleteObject(hFontBold); DeleteObject(hFontTitle); DeleteObject(hFontSmall); DeleteObject(hFontSection); DeleteObject(hFontTiny);
        PostQuitMessage(0); return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// MODE PICKER
// ============================================================================
static const wchar_t* kModePickerClass = L"VBObfuscatorModePicker";
static HWND g_hModePicker = nullptr;
static HWND g_hModeLangCombo = nullptr;
static bool g_modePickerDone = false;
static bool g_modePickerSuccess = false;

LRESULT CALLBACK ModePickerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hModeLangCombo = CreateWindow(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL, 30, 120, 380, 150, hwnd, (HMENU)IDC_MODE_LANG, GetModuleHandle(NULL), NULL);
        SetWindowSubclass(g_hModeLangCombo, ComboSubclassProc, 3, 0);
        SendMessage(g_hModeLangCombo, WM_SETFONT, (WPARAM)hFontUI, TRUE);
        SendMessage(g_hModeLangCombo, CB_ADDSTRING, 0, (LPARAM)L"VB.NET (.vb)");
        SendMessage(g_hModeLangCombo, CB_ADDSTRING, 0, (LPARAM)L"C# (.cs)");
        SendMessage(g_hModeLangCombo, CB_ADDSTRING, 0, (LPARAM)L"C++ (.cpp/.h)");
        SendMessage(g_hModeLangCombo, CB_SETCURSEL, 2, 0);

        int y = 160;
        auto mkBtn = [&](const wchar_t* text, int id) {
            return CreateWindow(L"BUTTON", text, WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                30, y, 380, 36, hwnd, (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
            };
        mkBtn(L"Single File...", IDC_MODE_SINGLE); y += 44;
        mkBtn(L"Folder...", IDC_MODE_FOLDER); y += 44;
        mkBtn(L"Project (.csproj/.vbproj/.vcxproj)...", IDC_MODE_PROJECT); y += 44;
        mkBtn(L"Solution (.sln)...", IDC_MODE_SOLUTION);
        SetTimer(hwnd, 1, 60, NULL);
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        TRIVERTEX vert[2] = {};
        vert[0].x = rc.left; vert[0].y = rc.top;
        vert[0].Red = (COLOR16)(GetRValue(CLR_BG) << 8); vert[0].Green = (COLOR16)(GetGValue(CLR_BG) << 8); vert[0].Blue = (COLOR16)(GetBValue(CLR_BG) << 8);
        vert[1].x = rc.right; vert[1].y = rc.bottom;
        vert[1].Red = (COLOR16)(GetRValue(CLR_BG_LO) << 8); vert[1].Green = (COLOR16)(GetGValue(CLR_BG_LO) << 8); vert[1].Blue = (COLOR16)(GetBValue(CLR_BG_LO) << 8);
        GRADIENT_RECT gr = { 0, 1 };
        GradientFill(hdc, vert, 2, &gr, 1, GRADIENT_FILL_RECT_V);
        return TRUE;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT cr; GetClientRect(hwnd, &cr);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT);
        HFONT oF = (HFONT)SelectObject(hdc, hFontTitle);
        TextOutW(hdc, 30, 18, L"root@obfuscator:~#", 18);
        SelectObject(hdc, hFontSmall);
        const wchar_t* sub = L"// select input mode";
        TextOutW(hdc, 30, 46, sub, (int)wcslen(sub));
        SelectObject(hdc, oF);

        SetTextColor(hdc, CLR_TEXT_DIM);
        HFONT oF2 = (HFONT)SelectObject(hdc, hFontUI);
        TextOutW(hdc, 30, 100, L"Language", 8);
        SelectObject(hdc, oF2);

        RECT rClose = { cr.right - 40, 14, cr.right - 14, 34 };
        SetTextColor(hdc, CLR_ERROR);
        HFONT oF3 = (HFONT)SelectObject(hdc, hFontBold);
        DrawText(hdc, L"X", 1, &rClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oF3);

        HPEN p = CreatePen(PS_SOLID, 1, CLR_BORDER_HI);
        HGDIOBJ op = SelectObject(hdc, p);
        HGDIOBJ ob = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(hdc, cr.left + 1, cr.top + 1, cr.right - 1, cr.bottom - 1, 18, 18);
        SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(p);

        EndPaint(hwnd, &ps); return 0;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lParam;
        if (di && (di->CtlID == IDC_MODE_SINGLE || di->CtlID == IDC_MODE_FOLDER ||
            di->CtlID == IDC_MODE_PROJECT || di->CtlID == IDC_MODE_SOLUTION)) {
            RECT rc = di->rcItem;
            int w = rc.right - rc.left, h = rc.bottom - rc.top;
            HDC mdc = CreateCompatibleDC(di->hDC);
            HBITMAP bmp = CreateCompatibleBitmap(di->hDC, w, h);
            HGDIOBJ ob = SelectObject(mdc, bmp);
            HBRUSH cb = CreateSolidBrush(CLR_CARD_BOTTOM); RECT all = { 0, 0, w, h }; FillRect(mdc, &all, cb); DeleteObject(cb);
            bool pressed = (di->itemState & ODS_SELECTED) != 0;
            POINT pt; GetCursorPos(&pt); ScreenToClient(di->hwndItem, &pt);
            bool hot = PtInRect(&all, pt) != FALSE;
            COLORREF fill = pressed ? CLR_ACCENT : (hot ? RGB(0, 200, 65) : CLR_SEG_ON);
            PaintRoundedRect(mdc, all, 6, fill, CLR_ACCENT, 1);
            SetBkMode(mdc, TRANSPARENT); SetTextColor(mdc, RGB(0, 0, 0));
            HFONT oF = (HFONT)SelectObject(mdc, hFontBold);
            wchar_t buf[128]; GetWindowText(di->hwndItem, buf, 128);
            DrawText(mdc, buf, -1, &all, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(mdc, oF);
            BitBlt(di->hDC, rc.left, rc.top, w, h, mdc, 0, 0, SRCCOPY);
            SelectObject(mdc, ob); DeleteObject(bmp); DeleteDC(mdc);
            return TRUE;
        }
        if (di && di->CtlID == IDC_MODE_LANG) {
            if ((int)di->itemID < 0) return TRUE;
            bool hot = (di->itemState & ODS_SELECTED) != 0;
            HBRUSH br = CreateSolidBrush(hot ? CLR_ACCENT : CLR_CTRL_BG);
            FillRect(di->hDC, &di->rcItem, br); DeleteObject(br);
            wchar_t t[256] = {};
            SendMessage(di->hwndItem, CB_GETLBTEXT, di->itemID, (LPARAM)t);
            SetBkMode(di->hDC, TRANSPARENT);
            SetTextColor(di->hDC, hot ? RGB(0, 60, 20) : CLR_TEXT);
            HFONT oF = (HFONT)SelectObject(di->hDC, hFontUI);
            RECT tr = di->rcItem; tr.left += 8;
            DrawText(di->hDC, t, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(di->hDC, oF);
            return TRUE;
        }
        break;
    }
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mi = (MEASUREITEMSTRUCT*)lParam;
        if (mi && mi->CtlType == ODT_COMBOBOX) { mi->itemHeight = 20; return TRUE; }
        break;
    }
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, CLR_EDIT_BG);
        SetTextColor(hdc, CLR_TEXT_BRIGHT);
        static HBRUSH s_br = CreateSolidBrush(CLR_EDIT_BG);
        return (LRESULT)s_br;
    }
    case WM_TIMER: InvalidateRect(hwnd, NULL, FALSE); return 0;
    case WM_LBUTTONDOWN: {
        RECT cr; GetClientRect(hwnd, &cr);
        int mx = GET_X_LPARAM(lParam), my = GET_Y_LPARAM(lParam);
        if (mx >= cr.right - 40 && my >= 14 && my <= 34) {
            g_modePickerDone = true;
            g_modePickerSuccess = false;
            DestroyWindow(hwnd);
            return 0;
        }
        if (my < 52) {
            POINT scr; GetCursorPos(&scr);
            ReleaseCapture();
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(scr.x, scr.y));
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam), code = HIWORD(wParam);
        if (code != BN_CLICKED) return 0;
        Lang lang = Lang::Cpp;
        {
            int sel = (int)SendMessage(g_hModeLangCombo, CB_GETCURSEL, 0, 0);
            lang = (sel == 0) ? Lang::VB : (sel == 1) ? Lang::CSharp : Lang::Cpp;
        }
        if (id == IDC_MODE_SINGLE) {
            g_pendingSingleFileBrowse = true;
            g_modePickerDone = true;
            g_modePickerSuccess = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == IDC_MODE_FOLDER) {
            BROWSEINFOW bi = {};
            bi.hwndOwner = hwnd;
            bi.lpszTitle = L"Select project folder";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (!pidl) return 0;
            wchar_t path[MAX_PATH] = {};
            SHGetPathFromIDListW(pidl, path);
            CoTaskMemFree(pidl);
            LoadedProject proj = LoadFromFolder(path, lang);
            if (proj.files.empty()) {
                LogMessage(L"No matching files found in \"" + wstring(path) + L"\".", CLR_WARN);
                MessageBox(hwnd, L"No matching source files were found in that folder.", L"Empty", MB_OK | MB_ICONWARNING);
                return 0;
            }
            g_currentProject = proj;
            g_projectLoaded = true;
            g_singleFilePath.clear();
            g_modePickerDone = true;
            g_modePickerSuccess = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == IDC_MODE_PROJECT) {
            wchar_t fn[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"Project (*.csproj;*.vbproj;*.vcxproj)\0*.csproj;*.vbproj;*.vcxproj\0All\0*.*\0";
            ofn.lpstrFile = fn; ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (!GetOpenFileNameW(&ofn)) return 0;
            wstring lower = toLowerW(fn);
            Lang inferred = lang;
            if (endsWithW(lower, L".csproj")) inferred = Lang::CSharp;
            else if (endsWithW(lower, L".vbproj")) inferred = Lang::VB;
            else if (endsWithW(lower, L".vcxproj")) inferred = Lang::Cpp;
            LoadedProject proj = LoadFromProject(wstring(fn), inferred);
            if (proj.files.empty()) {
                LogMessage(L"No matching files found in project \"" + baseNameOf(wstring(fn)) + L"\".", CLR_WARN);
                MessageBox(hwnd, L"No matching source files were found in that project.", L"Empty", MB_OK | MB_ICONWARNING);
                return 0;
            }
            g_currentProject = proj;
            g_projectLoaded = true;
            g_singleFilePath.clear();
            g_modePickerDone = true;
            g_modePickerSuccess = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == IDC_MODE_SOLUTION) {
            wchar_t fn[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"Solution (*.sln)\0*.sln\0All\0*.*\0";
            ofn.lpstrFile = fn; ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (!GetOpenFileNameW(&ofn)) return 0;
            bool languageMismatch = false;
            Lang detectedLanguage = lang;
            LoadedProject proj = LoadFromSolution(wstring(fn), lang, &languageMismatch, &detectedLanguage);
            if (proj.files.empty()) {
                if (languageMismatch) {
                    wstring alert = wstring(L"Selected target language: ") + langName(lang) +
                        L"\nSolution project language: " + langName(detectedLanguage) +
                        L"\n\nSelect the matching target language and choose the solution again.";
                    MessageBox(hwnd, alert.c_str(), L"Project Language Mismatch", MB_OK | MB_ICONERROR);
                }
                else {
                    LogMessage(L"No matching files found in solution \"" + baseNameOf(wstring(fn)) + L"\".", CLR_WARN);
                    MessageBox(hwnd, L"No matching source files were found in that solution (or the solution mixes languages — see the log).", L"Empty", MB_OK | MB_ICONWARNING);
                }
                return 0;
            }
            g_currentProject = proj;
            g_projectLoaded = true;
            g_singleFilePath.clear();
            g_modePickerDone = true;
            g_modePickerSuccess = true;
            DestroyWindow(hwnd);
            return 0;
        }
        return 0;
    }
    case WM_CLOSE: {
        g_modePickerDone = true;
        g_modePickerSuccess = false;
        DestroyWindow(hwnd);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Centers a window of size w x h on the work area of the primary monitor
// (excludes the taskbar), so every top-level window in the app opens in
// the same predictable spot instead of wherever the OS defaults to.
static void CenterOnScreen(int w, int h, int& x, int& y) {
    RECT wa;
    if (SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0)) {
        x = wa.left + ((wa.right - wa.left) - w) / 2;
        y = wa.top + ((wa.bottom - wa.top) - h) / 2;
    }
    else {
        x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    }
}

static bool ShowModePicker(HINSTANCE hInstance) {
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = ModePickerProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = kModePickerClass;
        wc.hbrBackground = CreateSolidBrush(CLR_BG);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassEx(&wc);
        classRegistered = true;
    }

    g_modePickerDone = false;
    g_modePickerSuccess = false;

    int mpX, mpY;
    CenterOnScreen(440, 380, mpX, mpY);
    HWND hwnd = CreateWindowEx(WS_EX_COMPOSITED | WS_EX_APPWINDOW, kModePickerClass,
        L"Select Mode", WS_POPUP | WS_THICKFRAME, mpX, mpY, 440, 380,
        nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return false;
    g_hModePicker = hwnd;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (!g_modePickerDone && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (IsWindow(hwnd)) DestroyWindow(hwnd);
    g_hModePicker = nullptr;

    return g_modePickerSuccess;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    ApplyMode();

    INITCOMMONCONTROLSEX iccex = { sizeof(iccex), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&iccex);
    RegisterCustomButtonClasses(hInstance);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    InitFonts();

    if (!ShowModePicker(hInstance)) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        CoUninitialize();
        return 0;
    }

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"VBObfuscatorWindow";
    wc.hbrBackground = CreateSolidBrush(CLR_BG);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassEx(&wc);
    int mainX, mainY;
    CenterOnScreen(840, 1080, mainX, mainY);
    hWndMain = CreateWindowEx(WS_EX_COMPOSITED | WS_EX_APPWINDOW, L"VBObfuscatorWindow",
        L"Obfuscator", WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        mainX, mainY, 840, 1080, NULL, NULL, hInstance, NULL);
    ShowWindow(hWndMain, nCmdShow);
    UpdateWindow(hWndMain);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }

    CoUninitialize();
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}
