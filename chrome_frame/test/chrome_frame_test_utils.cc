// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/chrome_frame_test_utils.h"

#include <atlbase.h>
#include <atlwin.h>
#include <iepmapi.h>

#include "base/registry.h"   // to find IE and firefox
#include "base/scoped_handle.h"
#include "base/scoped_comptr_win.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/common/chrome_switches.h"

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

    static const int hotkey_id = 0x0000baba;

    SetWindowLongPtr(GWLP_USERDATA, reinterpret_cast<ULONG_PTR>(window));
    RegisterHotKey(m_hWnd, hotkey_id, 0, VK_F22);

    MSG msg = {0};
    PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);

    INPUT hotkey = {0};
    hotkey.type = INPUT_KEYBOARD;
    hotkey.ki.wVk =  VK_F22;
    SendInput(1, &hotkey, sizeof(hotkey));

    while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (WM_HOTKEY == msg.message)
        break;
    }

    UnregisterHotKey(m_hWnd, hotkey_id);
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

void SendVirtualKey(int16 key) {
  INPUT input = { INPUT_KEYBOARD };
  input.ki.wVk = key;
  SendInput(1, &input, sizeof(input));
  input.ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(1, &input, sizeof(input));
}

void SendChar(char c) {
  SendVirtualKey(VkKeyScanA(c));
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
    CommandLine cmdline(path);
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

}  // namespace chrome_frame_test
