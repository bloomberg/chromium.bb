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
  DWORD size = GetClassNameW(window_to_check, buffer, buf_size);
  // If the window name is any longer than this, it isn't the one we want.
  if (size < (buf_size - 1)) {
    if (!lstrcmpiW(window_class, buffer)) {
     window_matches = true;
    }
  }
  return window_matches;
}

bool IsNamedWindow(HWND window, const wchar_t* window_name) {
  bool window_matches = false;
  const int buf_size = MAX_PATH;
  wchar_t buffer[buf_size] = {0};
  DWORD size = GetWindowText(window, buffer, buf_size);
  if (size < (buf_size - 1)) {
    if (!lstrcmpiW(window_name, buffer)) {
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

namespace {
struct FindWindowParams {
  HWND parent_;
  const wchar_t* class_name_;
  const wchar_t* window_name_;
  HWND window_found_;
  DWORD thread_id_;
  DWORD process_id_;
  FindWindowParams(HWND parent,
                   const wchar_t* class_name,
                   const wchar_t* window_name,
                   DWORD thread_id,
                   DWORD process_id)
    : parent_(parent),
      class_name_(class_name),
      window_name_(window_name),
      window_found_(NULL),
      thread_id_(thread_id),
      process_id_(process_id) {
  }
};

// Checks a window against a set of parameters defined in params. If the
// window matches, fills in params->window_found_ with the HWND of the window
// and returns true. Returns false otherwise.
bool WindowMatches(HWND window, FindWindowParams* params) {
  bool found = false;
  DWORD process_id = 0;
  DWORD thread_id = GetWindowThreadProcessId(window, &process_id);

  // First check that the PID and TID match if we're interested.
  if (params->process_id_ == 0 || params->process_id_ == process_id) {
    if (params->thread_id_ == 0 || params->thread_id_ == thread_id) {
      // Then check that we match on class and window names, again only if
      // we're interested.
      if ((params->class_name_ == NULL ||
           IsWindowOfClass(window, params->class_name_)) &&
          (params->window_name_ == NULL) ||
           IsNamedWindow(window, params->window_name_)) {
        found = true;
        params->window_found_ = window;
      }
    }
  }
  return found;
}

}  // namespace

BOOL CALLBACK WndEnumProc(HWND window, LPARAM lparam) {
  FindWindowParams* params = reinterpret_cast<FindWindowParams *>(lparam);
  if (!params) {
    return FALSE;
  }

  if (WindowMatches(window, params)) {
    // We found a match on a top level window. Return false to stop enumerating.
    return FALSE;
  } else {
    // If criteria not satisfied, let us try child windows.
    HWND child_window =  RecurseFindWindow(window,
                                           params->class_name_,
                                           params->window_name_,
                                           params->thread_id_,
                                           params->process_id_);
    if (child_window != NULL) {
      // We found the window we are looking for.
      params->window_found_ = child_window;
      return FALSE;
    }
    return TRUE;
  }
}

HWND RecurseFindWindow(HWND parent,
                       const wchar_t* class_name,
                       const wchar_t* window_name,
                       DWORD thread_id_to_match,
                       DWORD process_id_to_match) {
  if ((class_name == NULL) && (window_name == NULL)) {
    return NULL;
  }
  FindWindowParams params(parent, class_name, window_name,
                          thread_id_to_match, process_id_to_match);
  EnumChildWindows(parent, WndEnumProc, reinterpret_cast<LPARAM>(&params));
  return params.window_found_;
}
