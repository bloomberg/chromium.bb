// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// chrome_frame_helper_main.cc : The .exe that bootstraps the
// chrome_frame_helper.dll.
//
// This is a small exe that loads the hook dll to set the event hooks and then
// waits in a message loop. It is intended to be shut down by looking for a
// window with the class and title
// kChromeFrameHelperWindowClassName and kChromeFrameHelperWindowName and then
// sending that window a WM_CLOSE message.
//

#include <crtdbg.h>
#include <objbase.h>
#include <stdlib.h>
#include <windows.h>

#include "chrome_frame/chrome_frame_helper_util.h"
#include "chrome_frame/crash_server_init.h"
#include "chrome_frame/registry_watcher.h"

namespace {

// Window class and window names.
const wchar_t kChromeFrameHelperWindowClassName[] =
    L"ChromeFrameHelperWindowClass";
const wchar_t kChromeFrameHelperWindowName[] =
    L"ChromeFrameHelperWindowName";

const wchar_t kChromeFrameClientStateKey[] =
    L"Software\\Google\\Update\\ClientState\\"
    L"{8BA986DA-5100-405E-AA35-86F34A02ACBF}";
const wchar_t kChromeFrameUninstallCmdExeValue[] = L"UninstallString";
const wchar_t kChromeFrameUninstallCmdArgsValue[] = L"UninstallArguments";

const wchar_t kBHORegistrationPath[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer"
    L"\\Browser Helper Objects";

const wchar_t kStartupArg[] = L"--startup";
const wchar_t kForceUninstall[] = L"--force-uninstall";

HWND g_hwnd = NULL;
const UINT kRegistryWatcherChangeMessage = WM_USER + 42;

}  // namespace

// Small helper class that assists in loading the DLL that contains code
// to register our event hook callback. Automatically removes the hook and
// unloads the DLL on destruction.
class HookDllLoader {
 public:
  HookDllLoader() : dll_(NULL), start_proc_(NULL), stop_proc_(NULL) {}
  ~HookDllLoader() {
    if (dll_) {
      Unload();
    }
  }

  bool Load() {
    dll_ = LoadLibrary(L"chrome_frame_helper.dll");
    if (dll_) {
      start_proc_ = GetProcAddress(dll_, "StartUserModeBrowserInjection");
      stop_proc_ = GetProcAddress(dll_, "StopUserModeBrowserInjection");
    }

    bool result = true;
    if (!start_proc_ || !stop_proc_) {
      _ASSERTE(L"failed to load hook dll.");
      result = false;
    } else {
      if (FAILED(start_proc_())) {
        _ASSERTE(L"failed to initialize hook dll.");
        result = false;
      }
    }
    return result;
  }

  void Unload() {
    if (stop_proc_) {
      stop_proc_();
    }
    if (dll_) {
      FreeLibrary(dll_);
    }
  }

 private:
  HMODULE dll_;
  PROC start_proc_;
  PROC stop_proc_;
};

// Checks the window title and then class of hwnd. If they match with that
// of a chrome_frame_helper.exe window, then add it to the list of windows
// pointed to by lparam.
BOOL CALLBACK CloseHelperWindowsEnumProc(HWND hwnd, LPARAM lparam) {
  _ASSERTE(lparam != 0);

  wchar_t title_buffer[MAX_PATH] = {0};
  if (GetWindowText(hwnd, title_buffer, MAX_PATH)) {
    if (lstrcmpiW(title_buffer, kChromeFrameHelperWindowName) == 0) {
      wchar_t class_buffer[MAX_PATH] = {0};
      if (GetClassName(hwnd, class_buffer, MAX_PATH)) {
        if (lstrcmpiW(class_buffer, kChromeFrameHelperWindowClassName) == 0) {
          if (hwnd != reinterpret_cast<HWND>(lparam)) {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
          }
        }
      }
    }
  }

  return TRUE;
}

// Enumerates all top level windows, looking for those that look like a
// Chrome Frame helper window. It then closes all of them except for
// except_me.
void CloseAllHelperWindowsApartFrom(HWND except_me) {
  EnumWindows(CloseHelperWindowsEnumProc, reinterpret_cast<LPARAM>(except_me));
}

LRESULT CALLBACK ChromeFrameHelperWndProc(HWND hwnd,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam) {
  switch (message) {
    case WM_CREATE:
      CloseAllHelperWindowsApartFrom(hwnd);
      break;
    case kRegistryWatcherChangeMessage:
      // A system level Chrome appeared. Fall through:
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      return ::DefWindowProc(hwnd, message, wparam, lparam);
  }
  return 0;
}

HWND RegisterAndCreateWindow(HINSTANCE hinstance) {
  WNDCLASSEX wcex = {0};
  wcex.cbSize         = sizeof(WNDCLASSEX);
  wcex.lpfnWndProc    = ChromeFrameHelperWndProc;
  wcex.hInstance      = GetModuleHandle(NULL);
  wcex.hbrBackground  = reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
  wcex.lpszClassName  = kChromeFrameHelperWindowClassName;
  RegisterClassEx(&wcex);

  HWND hwnd = CreateWindow(kChromeFrameHelperWindowClassName,
      kChromeFrameHelperWindowName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0,
      CW_USEDEFAULT, 0, NULL, NULL, hinstance, NULL);

  return hwnd;
}


// This method runs the user-level Chrome Frame uninstall command. This is
// intended to allow it to delegate to an existing system-level install.
bool UninstallUserLevelChromeFrame() {
  bool success = false;
  HKEY reg_handle = NULL;
  wchar_t reg_path_buffer[MAX_PATH] = {0};
  LONG result = RegOpenKeyEx(HKEY_CURRENT_USER,
                             kChromeFrameClientStateKey,
                             0,
                             KEY_QUERY_VALUE,
                             &reg_handle);
  if (result == ERROR_SUCCESS) {
    wchar_t exe_buffer[MAX_PATH] = {0};
    wchar_t args_buffer[MAX_PATH] = {0};
    LONG exe_result = ReadValue(reg_handle,
                                kChromeFrameUninstallCmdExeValue,
                                MAX_PATH,
                                exe_buffer);
    LONG args_result = ReadValue(reg_handle,
                                 kChromeFrameUninstallCmdArgsValue,
                                 MAX_PATH,
                                 args_buffer);
    RegCloseKey(reg_handle);
    reg_handle = NULL;

    if (exe_result == ERROR_SUCCESS && args_result == ERROR_SUCCESS) {
      STARTUPINFO startup_info = {0};
      startup_info.cb = sizeof(startup_info);
      startup_info.dwFlags = STARTF_USESHOWWINDOW;
      startup_info.wShowWindow = SW_SHOW;
      PROCESS_INFORMATION process_info = {0};

      // Quote the command string in the args.
      wchar_t argument_string[MAX_PATH * 3] = {0};
      int arg_len = _snwprintf(argument_string,
                               _countof(argument_string) - 1,
                               L"\"%s\" %s %s",
                               exe_buffer,
                               args_buffer,
                               kForceUninstall);

      if (arg_len > 0 && CreateProcess(exe_buffer, argument_string,
                                       NULL, NULL, FALSE, 0, NULL, NULL,
                                       &startup_info, &process_info)) {
        // Close handles.
        CloseHandle(process_info.hThread);
        CloseHandle(process_info.hProcess);
        success = true;
      }
    }
  }

  return success;
}

void WaitCallback() {
  // Check if the Chrome Frame BHO is now in the list of registered BHOs:
  if (IsBHOLoadingPolicyRegistered()) {
    PostMessage(g_hwnd, kRegistryWatcherChangeMessage, 0, 0);
  }
}

int APIENTRY wWinMain(HINSTANCE hinstance, HINSTANCE, wchar_t*, int show_cmd) {
  google_breakpad::scoped_ptr<google_breakpad::ExceptionHandler> breakpad(
    InitializeCrashReporting(NORMAL));

  if (IsSystemLevelChromeFrameInstalled()) {
    // If we're running at startup, check for system-level Chrome Frame
    // installations. If we have one, time
    // to bail, also schedule user-level CF to be uninstalled at next logon.
    const wchar_t* cmd_line = ::GetCommandLine();
    if (cmd_line && wcsstr(cmd_line, kStartupArg) != NULL) {
      bool uninstalled = UninstallUserLevelChromeFrame();
      _ASSERTE(uninstalled);
    }
    return 0;
  }

  // Create a window with a known class and title just to listen for WM_CLOSE
  // messages that will shut us down.
  g_hwnd = RegisterAndCreateWindow(hinstance);
  _ASSERTE(IsWindow(g_hwnd));

  // Load the hook dll, and set the event hooks.
  HookDllLoader loader;
  bool loaded = loader.Load();
  _ASSERTE(loaded);

  // Start up the registry watcher
  RegistryWatcher registry_watcher(HKEY_LOCAL_MACHINE, kBHORegistrationPath,
                                   WaitCallback);
  bool watching = registry_watcher.StartWatching();
  _ASSERTE(watching);

  if (loaded) {
    MSG msg;
    BOOL ret;
    // Main message loop:
    while ((ret = GetMessage(&msg, NULL, 0, 0))) {
      if (ret == -1) {
        break;
      } else {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  }

  UnregisterClass(kChromeFrameHelperWindowClassName, hinstance);
  return 0;
}
