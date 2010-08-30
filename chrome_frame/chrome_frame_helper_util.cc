// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_helper_util.h"

#include <shlwapi.h>

const wchar_t kGetBrowserMessage[] = L"GetAutomationObject";

bool UtilIsWebBrowserWindow(HWND window_to_check) {
  bool is_browser_window = false;

  if (!IsWindow(window_to_check)) {
    return is_browser_window;
  }

  static wchar_t* known_ie_window_classes[] = {
    L"IEFrame",
    L"TabWindowClass"
  };

  for (int i = 0; i < ARRAYSIZE(known_ie_window_classes); i++) {
    if (IsWindowOfClass(window_to_check, known_ie_window_classes[i])) {
     is_browser_window = true;
     break;
    }
  }

  return is_browser_window;
}

HRESULT UtilGetWebBrowserObjectFromWindow(HWND window,
                                          REFIID iid,
                                          void** web_browser_object) {
  if (NULL == web_browser_object) {
    return E_POINTER;
  }

  // Check whether this window is really a web browser window.
  if (UtilIsWebBrowserWindow(window)) {
    // IWebBroswer2 interface pointer can be retrieved from the browser
    // window by simply sending a registered message "GetAutomationObject"
    // Note that since we are sending a message to parent window make sure that
    // it is in the same thread.
    if (GetWindowThreadProcessId(window, NULL) != GetCurrentThreadId()) {
      return E_UNEXPECTED;
    }

    static const ULONG get_browser_message =
        RegisterWindowMessageW(kGetBrowserMessage);

    *web_browser_object =
        reinterpret_cast<void*>(SendMessage(window,
                                            get_browser_message,
                                            reinterpret_cast<WPARAM>(&iid),
                                            NULL));
    if (NULL != *web_browser_object) {
      return S_OK;
    }
  } else {
    return E_INVALIDARG;
  }
  return E_NOINTERFACE;
}


bool IsWindowOfClass(HWND window_to_check, const wchar_t* window_class) {
  bool window_matches = false;
  const int buf_size = MAX_PATH;
  wchar_t buffer[buf_size] = {0};
  DWORD dwRetSize = GetClassNameW(window_to_check, buffer, buf_size);
  // If the window name is any longer than this, it isn't the one we want.
  if (dwRetSize < (buf_size - 1)) {
    if (0 == lstrcmpiW(window_class, buffer)) {
     window_matches = true;
    }
  }
  return window_matches;
}


bool IsNamedProcess(const wchar_t* process_name) {
  wchar_t file_path[2048] = {0};
  GetModuleFileName(NULL, file_path, 2047);
  wchar_t* file_name = PathFindFileName(file_path);
  return (0 == lstrcmpiW(file_name, process_name));
}
