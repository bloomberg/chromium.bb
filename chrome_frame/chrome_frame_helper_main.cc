// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include <windows.h>

#include "chrome_frame/crash_server_init.h"

// Window class and window names.
const wchar_t kChromeFrameHelperWindowClassName[] =
    L"ChromeFrameHelperWindowClass";
const wchar_t kChromeFrameHelperWindowName[] =
    L"ChromeFrameHelperWindowName";

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


LRESULT CALLBACK ChromeFrameHelperWndProc(HWND hwnd,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam) {
  switch (message) {
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

int APIENTRY wWinMain(HINSTANCE hinstance, HINSTANCE, wchar_t*, int show_cmd) {
  const wchar_t* cmd_line = ::GetCommandLine();
  google_breakpad::scoped_ptr<google_breakpad::ExceptionHandler> breakpad(
      InitializeCrashReporting(cmd_line));

  // Create a window with a known class and title just to listen for WM_CLOSE
  // messages that will shut us down.
  HWND hwnd = RegisterAndCreateWindow(hinstance);
  _ASSERTE(hwnd);

  // Load the hook dll, and set the event hooks.
  HookDllLoader loader;
  bool loaded = loader.Load();
  _ASSERTE(loaded);

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

  return 0;
}
