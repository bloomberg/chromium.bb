// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/chrome_frame_test_utils.h"

#include <atlbase.h>
#include <atlwin.h>
#include <iepmapi.h>
#include <sddl.h>

#include "base/message_loop.h"
#include "base/registry.h"   // to find IE and firefox
#include "base/scoped_handle.h"
#include "base/scoped_comptr_win.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_frame_test {

const wchar_t kIEImageName[] = L"iexplore.exe";
const wchar_t kIEBrokerImageName[] = L"ieuser.exe";
const wchar_t kFirefoxImageName[] = L"firefox.exe";
const wchar_t kOperaImageName[] = L"opera.exe";
const wchar_t kSafariImageName[] = L"safari.exe";
const wchar_t kChromeImageName[] = L"chrome.exe";

bool IsTopLevelWindow(HWND window) {
  long style = GetWindowLong(window, GWL_STYLE);  // NOLINT
  if (!(style & WS_CHILD))
    return true;

  HWND parent = GetParent(window);
  if (!parent)
    return true;

  if (parent == GetDesktopWindow())
    return true;

  return false;
}

// Callback function for EnumThreadWindows.
BOOL CALLBACK CloseWindowsThreadCallback(HWND hwnd, LPARAM param) {
  int& count = *reinterpret_cast<int*>(param);
  if (IsWindowVisible(hwnd)) {
    if (IsWindowEnabled(hwnd)) {
      DWORD results = 0;
      if (!::SendMessageTimeout(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0, SMTO_BLOCK,
                                10000, &results)) {
        DLOG(WARNING) << "Window hung: " << StringPrintf(L"%08X", hwnd);
      }
      count++;
    } else {
      DLOG(WARNING) << "Skipping disabled window: "
                  << StringPrintf(L"%08X", hwnd);
    }
  }
  return TRUE;  // continue enumeration
}

// Attempts to close all non-child, visible windows on the given thread.
// The return value is the number of visible windows a close request was
// sent to.
int CloseVisibleTopLevelWindowsOnThread(DWORD thread_id) {
  int window_close_attempts = 0;
  EnumThreadWindows(thread_id, CloseWindowsThreadCallback,
                    reinterpret_cast<LPARAM>(&window_close_attempts));
  return window_close_attempts;
}

// Enumerates the threads of a process and attempts to close visible non-child
// windows on all threads of the process.
// The return value is the number of visible windows a close request was
// sent to.
int CloseVisibleWindowsOnAllThreads(HANDLE process) {
  DWORD process_id = ::GetProcessId(process);
  if (process_id == 0) {
    NOTREACHED();
    return 0;
  }

  ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0));
  if (!snapshot.IsValid()) {
    NOTREACHED();
    return 0;
  }

  int window_close_attempts = 0;
  THREADENTRY32 te = { sizeof(THREADENTRY32) };
  if (Thread32First(snapshot, &te)) {
    do {
      if (RTL_CONTAINS_FIELD(&te, te.dwSize, th32OwnerProcessID) &&
          te.th32OwnerProcessID == process_id) {
        window_close_attempts +=
            CloseVisibleTopLevelWindowsOnThread(te.th32ThreadID);
      }
      te.dwSize = sizeof(te);
    } while (Thread32Next(snapshot, &te));
  }

  return window_close_attempts;
}

class ForegroundHelperWindow : public CWindowImpl<ForegroundHelperWindow> {
 public:
BEGIN_MSG_MAP(ForegroundHelperWindow)
  MESSAGE_HANDLER(WM_HOTKEY, OnHotKey)
END_MSG_MAP()

  HRESULT SetForeground(HWND window) {
    DCHECK(::IsWindow(window));
    if (NULL == Create(NULL, NULL, NULL, WS_POPUP))
      return AtlHresultFromLastError();

    static const int kHotKeyId = 0x0000baba;
    static const int kHotKeyWaitTimeout = 2000;

    SetWindowLongPtr(GWLP_USERDATA, reinterpret_cast<ULONG_PTR>(window));
    RegisterHotKey(m_hWnd, kHotKeyId, 0, VK_F22);

    MSG msg = {0};
    PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);

    SendVirtualKey(VK_F22, false);
    // There are scenarios where the WM_HOTKEY is not dispatched by the
    // the corresponding foreground thread. To prevent us from indefinitely
    // waiting for the hotkey, we set a timer and exit the loop.
    SetTimer(kHotKeyId, kHotKeyWaitTimeout, NULL);

    while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (msg.message == WM_HOTKEY)
        break;
      else if (msg.message == WM_TIMER) {
        SetForegroundWindow(window);
        break;
      }
    }

    UnregisterHotKey(m_hWnd, kHotKeyId);
    KillTimer(kHotKeyId);
    DestroyWindow();
    return S_OK;
  }

  LRESULT OnHotKey(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled) {  // NOLINT
    HWND window = reinterpret_cast<HWND>(GetWindowLongPtr(GWLP_USERDATA));
    SetForegroundWindow(window);
    return 1;
  }
};

bool ForceSetForegroundWindow(HWND window) {
  if (GetForegroundWindow() == window)
    return true;
  ForegroundHelperWindow foreground_helper_window;
  HRESULT hr = foreground_helper_window.SetForeground(window);
  return SUCCEEDED(hr);
}

struct PidAndWindow {
  base::ProcessId pid;
  HWND hwnd;
};

BOOL CALLBACK FindWindowInProcessCallback(HWND hwnd, LPARAM param) {
  PidAndWindow* paw = reinterpret_cast<PidAndWindow*>(param);
  base::ProcessId pid;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == paw->pid && IsWindowVisible(hwnd)) {
    paw->hwnd = hwnd;
    return FALSE;
  }

  return TRUE;
}

bool EnsureProcessInForeground(base::ProcessId process_id) {
  HWND hwnd = GetForegroundWindow();
  base::ProcessId current_foreground_pid = 0;
  DWORD active_thread_id = GetWindowThreadProcessId(hwnd,
      &current_foreground_pid);
  if (current_foreground_pid == process_id)
    return true;

  PidAndWindow paw = { process_id };
  EnumWindows(FindWindowInProcessCallback, reinterpret_cast<LPARAM>(&paw));
  if (!IsWindow(paw.hwnd)) {
    DLOG(ERROR) << "failed to find process window";
    return false;
  }

  bool ret = ForceSetForegroundWindow(paw.hwnd);
  DLOG_IF(ERROR, !ret) << "ForceSetForegroundWindow: " << ret;

  return ret;
}

// Iterates through all the characters in the string and simulates
// keyboard input.  The input goes to the currently active application.
bool SendString(const wchar_t* string) {
  DCHECK(string != NULL);

  INPUT input[2] = {0};
  input[0].type = INPUT_KEYBOARD;
  input[0].ki.dwFlags = KEYEVENTF_UNICODE;  // to avoid shift, etc.
  input[1] = input[0];
  input[1].ki.dwFlags |= KEYEVENTF_KEYUP;

  for (const wchar_t* p = string; *p; p++) {
    input[0].ki.wScan = input[1].ki.wScan = *p;
    SendInput(2, input, sizeof(INPUT));
  }

  return true;
}

void SendVirtualKey(int16 key, bool extended) {
  INPUT input = { INPUT_KEYBOARD };
  input.ki.wVk = key;
  input.ki.dwFlags = extended ? KEYEVENTF_EXTENDEDKEY : 0;
  SendInput(1, &input, sizeof(input));
  input.ki.dwFlags = (extended ? KEYEVENTF_EXTENDEDKEY : 0) | KEYEVENTF_KEYUP;
  SendInput(1, &input, sizeof(input));
}

void SendChar(char c) {
  SendVirtualKey(VkKeyScanA(c), false);
}

void SendString(const char* s) {
  while (*s) {
    SendChar(*s);
    s++;
  }
}

// Sends a keystroke to the currently active application with optional
// modifiers set.
bool SendMnemonic(WORD mnemonic_char, bool shift_pressed, bool control_pressed,
                  bool alt_pressed) {
  INPUT special_keys[3] = {0};
  for (int index = 0; index < arraysize(special_keys); ++index) {
    special_keys[index].type = INPUT_KEYBOARD;
    special_keys[index].ki.dwFlags = 0;
  }

  int num_special_keys = 0;
  if (shift_pressed)  {
    special_keys[num_special_keys].ki.wVk = VK_SHIFT;
    num_special_keys++;
  }

  if (control_pressed)  {
    special_keys[num_special_keys].ki.wVk = VK_CONTROL;
    num_special_keys++;
  }

  if (alt_pressed)  {
    special_keys[num_special_keys].ki.wVk = VK_MENU;
    num_special_keys++;
  }

  // Depress the modifiers.
  SendInput(num_special_keys, special_keys, sizeof(INPUT));

  Sleep(100);

  INPUT mnemonic = {0};
  mnemonic.type = INPUT_KEYBOARD;
  mnemonic.ki.wVk = mnemonic_char;

  // Depress and release the mnemonic.
  SendInput(1, &mnemonic, sizeof(INPUT));
  Sleep(100);

  mnemonic.ki.dwFlags |= KEYEVENTF_KEYUP;
  SendInput(1, &mnemonic, sizeof(INPUT));
  Sleep(100);

  // Now release the modifiers.
  for (int index = 0;  index < num_special_keys; index++) {
    special_keys[index].ki.dwFlags |= KEYEVENTF_KEYUP;
  }

  SendInput(num_special_keys, special_keys, sizeof(INPUT));
  Sleep(100);

  return true;
}

std::wstring GetExecutableAppPath(const std::wstring& file) {
  std::wstring kAppPathsKey =
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\";

  std::wstring app_path;
  RegKey key(HKEY_LOCAL_MACHINE, (kAppPathsKey + file).c_str());
  if (key.Handle()) {
    key.ReadValue(NULL, &app_path);
  }

  return app_path;
}

std::wstring FormatCommandForApp(const std::wstring& exe_name,
                                 const std::wstring& argument) {
  std::wstring reg_path(StringPrintf(L"Applications\\%ls\\shell\\open\\command",
                                     exe_name.c_str()));
  RegKey key(HKEY_CLASSES_ROOT, reg_path.c_str());

  std::wstring command;
  if (key.Handle()) {
    key.ReadValue(NULL, &command);
    int found = command.find(L"%1");
    if (found >= 0) {
      command.replace(found, 2, argument);
    }
  }
  return command;
}

base::ProcessHandle LaunchExecutable(const std::wstring& executable,
                                     const std::wstring& argument) {
  base::ProcessHandle process = NULL;
  std::wstring path = GetExecutableAppPath(executable);
  if (path.empty()) {
    path = FormatCommandForApp(executable, argument);
    if (path.empty()) {
      DLOG(ERROR) << "Failed to find executable: " << executable;
    } else {
      CommandLine cmdline = CommandLine::FromString(path);
      base::LaunchApp(cmdline, false, false, &process);
    }
  } else {
    CommandLine cmdline((FilePath(path)));
    cmdline.AppendLooseValue(argument);
    base::LaunchApp(cmdline, false, false, &process);
  }
  return process;
}

base::ProcessHandle LaunchFirefox(const std::wstring& url) {
  return LaunchExecutable(kFirefoxImageName, url);
}

base::ProcessHandle LaunchSafari(const std::wstring& url) {
  return LaunchExecutable(kSafariImageName, url);
}

base::ProcessHandle LaunchChrome(const std::wstring& url) {
  return LaunchExecutable(kChromeImageName,
      StringPrintf(L"--%ls ", switches::kNoFirstRun) + url);
}

base::ProcessHandle LaunchOpera(const std::wstring& url) {
  // NOTE: For Opera tests to work it must be configured to start up with
  // a blank page.  There is an command line switch, -nosession, that's supposed
  // to avoid opening up the previous session, but that switch is not working.
  // TODO(tommi): Include a special ini file (opera6.ini) for opera and launch
  //  with our required settings.  This file is by default stored here:
  // "%USERPROFILE%\Application Data\Opera\Opera\profile\opera6.ini"
  return LaunchExecutable(kOperaImageName, url);
}

base::ProcessHandle LaunchIEOnVista(const std::wstring& url) {
  typedef HRESULT (WINAPI* IELaunchURLPtr)(
      const wchar_t* url,
      PROCESS_INFORMATION *pi,
      VOID *info);

  IELaunchURLPtr launch;
  PROCESS_INFORMATION pi = {0};
  IELAUNCHURLINFO  info = {sizeof info, 0};
  HMODULE h = LoadLibrary(L"ieframe.dll");
  if (!h)
    return NULL;
  launch = reinterpret_cast<IELaunchURLPtr>(GetProcAddress(h, "IELaunchURL"));
  HRESULT hr = launch(url.c_str(), &pi, &info);
  FreeLibrary(h);
  if (SUCCEEDED(hr))
    CloseHandle(pi.hThread);
  return pi.hProcess;
}

base::ProcessHandle LaunchIE(const std::wstring& url) {
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA) {
    return LaunchIEOnVista(url);
  } else {
    return LaunchExecutable(kIEImageName, url);
  }
}

int CloseAllIEWindows() {
  int ret = 0;

  ScopedComPtr<IShellWindows> windows;
  HRESULT hr = ::CoCreateInstance(__uuidof(ShellWindows), NULL, CLSCTX_ALL,
      IID_IShellWindows, reinterpret_cast<void**>(windows.Receive()));
  DCHECK(SUCCEEDED(hr));

  if (SUCCEEDED(hr)) {
    long count = 0;  // NOLINT
    windows->get_Count(&count);
    VARIANT i = { VT_I4 };
    for (i.lVal = 0; i.lVal < count; ++i.lVal) {
      ScopedComPtr<IDispatch> folder;
      windows->Item(i, folder.Receive());
      if (folder != NULL) {
        ScopedComPtr<IWebBrowser2> browser;
        if (SUCCEEDED(browser.QueryFrom(folder))) {
          browser->Quit();
          ++ret;
        }
      }
    }
  }

  return ret;
}

void ShowChromeFrameContextMenu() {
  static const int kChromeFrameContextMenuTimeout = 500;
  HWND renderer_window = GetChromeRendererWindow();
  EXPECT_TRUE(IsWindow(renderer_window));

  SetKeyboardFocusToWindow(renderer_window, 100, 100);

  // Bring up the context menu in the Chrome renderer window.
  PostMessage(renderer_window, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(50, 50));
  PostMessage(renderer_window, WM_RBUTTONUP, MK_RBUTTON, MAKELPARAM(50, 50));

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      NewRunnableFunction(SelectAboutChromeFrame),
      kChromeFrameContextMenuTimeout);
}

void SetKeyboardFocusToWindow(HWND window, int x, int y) {
  if (!IsTopLevelWindow(window)) {
    window = GetAncestor(window, GA_ROOT);
  }
  ForceSetForegroundWindow(window);

  POINT cursor_position = {130, 130};
  ClientToScreen(window, &cursor_position);

  double screen_width = ::GetSystemMetrics( SM_CXSCREEN ) - 1;
  double screen_height = ::GetSystemMetrics( SM_CYSCREEN ) - 1;
  double location_x =  cursor_position.x * (65535.0f / screen_width);
  double location_y =  cursor_position.y * (65535.0f / screen_height);

  INPUT input_info = {0};
  input_info.type = INPUT_MOUSE;
  input_info.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
  input_info.mi.dx = static_cast<long>(location_x);
  input_info.mi.dy = static_cast<long>(location_y);
  ::SendInput(1, &input_info, sizeof(INPUT));

  Sleep(10);

  input_info.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE;
  ::SendInput(1, &input_info, sizeof(INPUT));

  Sleep(10);

  input_info.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE;
  ::SendInput(1, &input_info, sizeof(INPUT));
}

void SendInputToWindow(HWND window, const std::string& input_string) {
  const unsigned long kIntervalBetweenInput = 100;

  for (size_t index = 0; index < input_string.length(); index++) {
    bool is_upper_case = isupper(input_string[index]);
    if (is_upper_case) {
      INPUT input = { INPUT_KEYBOARD };
      input.ki.wVk = VK_SHIFT;
      input.ki.dwFlags = 0;
      SendInput(1, &input, sizeof(input));
      Sleep(kIntervalBetweenInput);
    }

    // The WM_KEYDOWN and WM_KEYUP messages for characters always contain
    // the uppercase character codes.
    SendVirtualKey(toupper(input_string[index]), false);
    Sleep(kIntervalBetweenInput);

    if (is_upper_case) {
      INPUT input = { INPUT_KEYBOARD };
      input.ki.wVk = VK_SHIFT;
      input.ki.dwFlags = KEYEVENTF_KEYUP;
      SendInput(1, &input, sizeof(input));
      Sleep(kIntervalBetweenInput);
    }
  }
}

void SelectAboutChromeFrame() {
  // Send a key up message to enable the About chrome frame option to be
  // selected followed by a return to select it.
  SendVirtualKey(VK_UP, true);
  SendVirtualKey(VK_RETURN, false);
}

BOOL CALLBACK FindChromeRendererWindowProc(
    HWND window, LPARAM lParam) {
  HWND* target_window = reinterpret_cast<HWND*>(lParam);
  wchar_t class_name[MAX_PATH] = {0};

  GetClassName(window, class_name, arraysize(class_name));
  if (!_wcsicmp(class_name, L"Chrome_RenderWidgetHostHWND")) {
    *target_window = window;
    return FALSE;
  }

  return TRUE;
}

BOOL CALLBACK EnumHostBrowserWindowProc(
    HWND window, LPARAM lParam) {
  EnumChildWindows(window, FindChromeRendererWindowProc, lParam);
  HWND* target_window = reinterpret_cast<HWND*>(lParam);
  if (IsWindow(*target_window))
    return FALSE;
  return TRUE;
}

HWND GetChromeRendererWindow() {
  HWND chrome_window = NULL;
  EnumWindows(EnumHostBrowserWindowProc,
              reinterpret_cast<LPARAM>(&chrome_window));
  return chrome_window;
}


LowIntegrityToken::LowIntegrityToken() : impersonated_(false) {
}

LowIntegrityToken::~LowIntegrityToken() {
  RevertToSelf();
}

BOOL LowIntegrityToken::RevertToSelf() {
  BOOL ok = TRUE;
  if (impersonated_) {
    DCHECK(IsImpersonated());
    ok = ::RevertToSelf();
    if (ok)
      impersonated_ = false;
  }

  return ok;
}

BOOL LowIntegrityToken::Impersonate() {
  DCHECK(!impersonated_);
  DCHECK(!IsImpersonated());
  HANDLE process_token_handle = NULL;
  BOOL ok = ::OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE,
                               &process_token_handle);
  if (!ok) {
    DLOG(ERROR) << "::OpenProcessToken failed: " << GetLastError();
    return ok;
  }

  ScopedHandle process_token(process_token_handle);
  // Create impersonation low integrity token.
  HANDLE impersonation_token_handle = NULL;
  ok = ::DuplicateTokenEx(process_token,
      TOKEN_QUERY | TOKEN_IMPERSONATE | TOKEN_ADJUST_DEFAULT, NULL,
      SecurityImpersonation, TokenImpersonation, &impersonation_token_handle);
  if (!ok) {
    DLOG(ERROR) << "::DuplicateTokenEx failed: " << GetLastError();
    return ok;
  }

  // TODO: sandbox/src/restricted_token_utils.cc has SetTokenIntegrityLevel
  // function already.
  ScopedHandle impersonation_token(impersonation_token_handle);
  PSID integrity_sid = NULL;
  TOKEN_MANDATORY_LABEL tml = {0};
  ok = ::ConvertStringSidToSid(SDDL_ML_LOW, &integrity_sid);
  if (!ok) {
    DLOG(ERROR) << "::ConvertStringSidToSid failed: " << GetLastError();
    return ok;
  }

  tml.Label.Attributes = SE_GROUP_INTEGRITY | SE_GROUP_INTEGRITY_ENABLED;
  tml.Label.Sid = integrity_sid;
  ok = ::SetTokenInformation(impersonation_token, TokenIntegrityLevel,
      &tml, sizeof(tml) + ::GetLengthSid(integrity_sid));
  ::LocalFree(integrity_sid);
  if (!ok) {
    DLOG(ERROR) << "::SetTokenInformation failed: " << GetLastError();
    return ok;
  }

  // Switch current thread to low integrity.
  ok = ::ImpersonateLoggedOnUser(impersonation_token);
  if (ok) {
    impersonated_ = true;
  } else {
    DLOG(ERROR) << "::ImpersonateLoggedOnUser failed: " << GetLastError();
  }

  return ok;
}

bool LowIntegrityToken::IsImpersonated() {
  HANDLE token = NULL;
  if (!::OpenThreadToken(::GetCurrentThread(), 0, false, &token) &&
      ::GetLastError() != ERROR_NO_TOKEN) {
    return true;
  }

  if (token)
    ::CloseHandle(token);

  return false;
}

HRESULT LaunchIEAsComServer(IWebBrowser2** web_browser) {
  if (!web_browser)
    return E_INVALIDARG;

  HRESULT hr = S_OK;
  DWORD cocreate_flags = CLSCTX_LOCAL_SERVER;
  chrome_frame_test::LowIntegrityToken token;
  // Vista has a bug which manifests itself when a medium integrity process
  // launches a COM server like IE which runs in protected mode due to UAC.
  // This causes the IWebBrowser2 interface which is returned to be useless,
  // i.e it does not receive any events, etc. Our workaround for this is
  // to impersonate a low integrity token and then launch IE.
  if (win_util::GetWinVersion() == win_util::WINVERSION_VISTA) {
    // Create medium integrity browser that will launch IE broker.
    ScopedComPtr<IWebBrowser2> medium_integrity_browser;
    hr = medium_integrity_browser.CreateInstance(CLSID_InternetExplorer, NULL,
                                                 CLSCTX_LOCAL_SERVER);
    if (FAILED(hr))
      return hr;
    medium_integrity_browser->Quit();
    // Broker remains alive.
    if (!token.Impersonate()) {
      hr = HRESULT_FROM_WIN32(GetLastError());
      return hr;
    }

    cocreate_flags |= CLSCTX_ENABLE_CLOAKING;
  }

  hr = ::CoCreateInstance(CLSID_InternetExplorer, NULL,
                          cocreate_flags, IID_IWebBrowser2,
                          reinterpret_cast<void**>(web_browser));
  // ~LowIntegrityToken() will switch integrity back to medium.
  return hr;
}

_ATL_FUNC_INFO WebBrowserEventSink::kNavigateErrorInfo = {
  CC_STDCALL, VT_EMPTY, 5, {
    VT_DISPATCH,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_BOOL | VT_BYREF,
  }
};

_ATL_FUNC_INFO WebBrowserEventSink::kNavigateComplete2Info = {
  CC_STDCALL, VT_EMPTY, 2, {
    VT_DISPATCH,
    VT_VARIANT | VT_BYREF
  }
};

_ATL_FUNC_INFO WebBrowserEventSink::kBeforeNavigate2Info = {
  CC_STDCALL, VT_EMPTY, 7, {
    VT_DISPATCH,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_BOOL | VT_BYREF
  }
};

_ATL_FUNC_INFO WebBrowserEventSink::kNewWindow3Info = {
  CC_STDCALL, VT_EMPTY, 5, {
    VT_DISPATCH | VT_BYREF,
    VT_BOOL | VT_BYREF,
    VT_UINT,
    VT_BSTR,
    VT_BSTR
  }
};

_ATL_FUNC_INFO WebBrowserEventSink::kVoidMethodInfo = {
    CC_STDCALL, VT_EMPTY, 0, {NULL}};

_ATL_FUNC_INFO WebBrowserEventSink::kDocumentCompleteInfo = {
  CC_STDCALL, VT_EMPTY, 2, {
    VT_DISPATCH,
    VT_VARIANT | VT_BYREF
  }
};

// WebBrowserEventSink member defines
void WebBrowserEventSink::Uninitialize() {
  DisconnectFromChromeFrame();
  if (web_browser2_.get()) {
    DispEventUnadvise(web_browser2_);
    web_browser2_->Quit();
    web_browser2_.Release();
  }
}

STDMETHODIMP WebBrowserEventSink::OnBeforeNavigate2Internal(
    IDispatch* dispatch, VARIANT* url, VARIANT* flags,
    VARIANT* target_frame_name, VARIANT* post_data, VARIANT* headers,
    VARIANT_BOOL* cancel) {
  DLOG(INFO) << __FUNCTION__;
  // Reset any existing reference to chrome frame since this is a new
  // navigation.
  chrome_frame_ = NULL;
  return OnBeforeNavigate2(dispatch, url, flags, target_frame_name,
                           post_data, headers, cancel);
}

STDMETHODIMP_(void) WebBrowserEventSink::OnNavigateComplete2Internal(
    IDispatch* dispatch, VARIANT* url) {
  DLOG(INFO) << __FUNCTION__;
  ConnectToChromeFrame();
  OnNavigateComplete2(dispatch, url);
}

STDMETHODIMP_(void) WebBrowserEventSink::OnDocumentCompleteInternal(
    IDispatch* dispatch, VARIANT* url) {
  DLOG(INFO) << __FUNCTION__;
  OnDocumentComplete(dispatch, url);
}

HRESULT WebBrowserEventSink::OnLoadInternal(const VARIANT* param) {
  DLOG(INFO) << __FUNCTION__ << " " << param->bstrVal;
  OnLoad(param->bstrVal);
  return S_OK;
}

HRESULT WebBrowserEventSink::OnLoadErrorInternal(const VARIANT* param) {
  DLOG(INFO) << __FUNCTION__ << " " << param->bstrVal;
  OnLoadError(param->bstrVal);
  return S_OK;
}

HRESULT WebBrowserEventSink::OnMessageInternal(const VARIANT* param) {
  DLOG(INFO) << __FUNCTION__ << " " << param->bstrVal;
  OnMessage(param->bstrVal);
  return S_OK;
}

HRESULT WebBrowserEventSink::LaunchIEAndNavigate(
    const std::wstring& navigate_url) {
  HRESULT hr = LaunchIEAsComServer(web_browser2_.Receive());
  EXPECT_EQ(S_OK, hr);
  if (hr == S_OK) {
    web_browser2_->put_Visible(VARIANT_TRUE);
    hr = DispEventAdvise(web_browser2_, &DIID_DWebBrowserEvents2);
    EXPECT_TRUE(hr == S_OK);
    hr = Navigate(navigate_url);
  }
  return hr;
}

HRESULT WebBrowserEventSink::Navigate(const std::wstring& navigate_url) {
  VARIANT empty = ScopedVariant::kEmptyVariant;
  ScopedVariant url;
  url.Set(navigate_url.c_str());

  HRESULT hr = S_OK;
  hr = web_browser2_->Navigate2(url.AsInput(), &empty, &empty, &empty, &empty);
  EXPECT_TRUE(hr == S_OK);
  return hr;
}

void WebBrowserEventSink::SetFocusToChrome() {
  chrome_frame_test::SetKeyboardFocusToWindow(
      GetAttachedChromeRendererWindow(), 1, 1);
}

void WebBrowserEventSink::SendInputToChrome(
    const std::string& input_string) {
  chrome_frame_test::SendInputToWindow(
      GetAttachedChromeRendererWindow(), input_string);
}

void WebBrowserEventSink::ConnectToChromeFrame() {
  DCHECK(web_browser2_);
  ScopedComPtr<IShellBrowser> shell_browser;
  DoQueryService(SID_STopLevelBrowser, web_browser2_,
                 shell_browser.Receive());

  if (shell_browser) {
    ScopedComPtr<IShellView> shell_view;
    shell_browser->QueryActiveShellView(shell_view.Receive());
    if (shell_view) {
      shell_view->GetItemObject(SVGIO_BACKGROUND, __uuidof(IChromeFrame),
           reinterpret_cast<void**>(chrome_frame_.Receive()));
    }

    if (chrome_frame_) {
      ScopedVariant onmessage(onmessage_.ToDispatch());
      ScopedVariant onloaderror(onloaderror_.ToDispatch());
      ScopedVariant onload(onload_.ToDispatch());
      EXPECT_HRESULT_SUCCEEDED(chrome_frame_->put_onmessage(onmessage));
      EXPECT_HRESULT_SUCCEEDED(chrome_frame_->put_onloaderror(onloaderror));
      EXPECT_HRESULT_SUCCEEDED(chrome_frame_->put_onload(onload));
    }
  }
}

void WebBrowserEventSink::DisconnectFromChromeFrame() {
  if (chrome_frame_) {
    ScopedVariant dummy(static_cast<IDispatch*>(NULL));
    chrome_frame_->put_onmessage(dummy);
    chrome_frame_->put_onload(dummy);
    chrome_frame_->put_onloaderror(dummy);
    chrome_frame_.Release();
  }
}

HWND WebBrowserEventSink::GetAttachedChromeRendererWindow() {
  DCHECK(chrome_frame_);
  HWND renderer_window = NULL;
  ScopedComPtr<IOleWindow> ole_window;
  ole_window.QueryFrom(chrome_frame_);
  EXPECT_TRUE(ole_window.get());

  if (ole_window) {
    HWND activex_window = NULL;
    ole_window->GetWindow(&activex_window);
    EXPECT_TRUE(IsWindow(activex_window));

    // chrome tab window is the first (and the only) child of activex
    HWND chrome_tab_window = GetWindow(activex_window, GW_CHILD);
    EXPECT_TRUE(IsWindow(chrome_tab_window));
    renderer_window = GetWindow(chrome_tab_window, GW_CHILD);
  }

  DCHECK(IsWindow(renderer_window));
  return renderer_window;
}

HRESULT WebBrowserEventSink::SetWebBrowser(IWebBrowser2* web_browser2) {
  DCHECK(web_browser2_.get() == NULL);
  web_browser2_ = web_browser2;
  web_browser2_->put_Visible(VARIANT_TRUE);
  HRESULT hr = DispEventAdvise(web_browser2_, &DIID_DWebBrowserEvents2);
  return hr;
}

}  // namespace chrome_frame_test
