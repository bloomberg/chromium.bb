// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for windows (as in HWND).

#include "ceee/common/window_utils.h"

#include "base/logging.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/process_utils_win.h"

namespace {

static bool CanAccessWindowProcess(HWND window) {
  bool can_access = false;

  return SUCCEEDED(process_utils_win::ProcessCompatibilityCheck::IsCompatible(
      window, &can_access)) && can_access;
}

}  // namespace

namespace window_utils {

bool IsWindowClass(HWND window, const std::wstring& window_class) {
  if (!::IsWindow(window)) {
    // No DCHECK, we may get her while a window is being destroyed.
    return false;
  }

  wchar_t class_name[MAX_PATH] = { 0 };
  int chars_copied = ::GetClassName(window, class_name, arraysize(class_name));
  LOG_IF(ERROR, chars_copied == 0) << "GetClassName, err=" << ::GetLastError();

  return (window_class == class_name);
}

HWND GetTopLevelParent(HWND window) {
  HWND top_level = window;
  while (HWND parent = ::GetParent(top_level)) {
    top_level = parent;
  }
  return top_level;
}

bool IsWindowThread(HWND window) {
  DWORD process_id = 0;
  return ::GetCurrentThreadId() == ::GetWindowThreadProcessId(window,
      &process_id) && ::GetCurrentProcessId() == process_id;
}

bool WindowHasNoThread(HWND window) {
  return ::GetWindowThreadProcessId(window, NULL) == 0;
}

// Changes made to this struct must also be made to the unittest code.
struct FindDescendentData {
  FindDescendentData(const std::wstring& window_class, bool only_visible,
                     bool check_compatibility)
      : window_class(window_class), only_visible(only_visible),
        check_process_access(check_compatibility), descendent(NULL) {
  }
  const std::wstring& window_class;
  bool only_visible;
  HWND descendent;
  bool check_process_access;
};

BOOL CALLBACK HandleDescendentWindowProc(HWND window, LPARAM param) {
  DCHECK(window && param);
  FindDescendentData* data = reinterpret_cast<FindDescendentData*>(param);
  if (data && IsWindowClass(window, data->window_class) &&
      (!data->only_visible || IsWindowVisible(window)) &&
      (!data->check_process_access || CanAccessWindowProcess(window))) {
    data->descendent = window;
    return FALSE;
  }
  return TRUE;
}

bool FindDescendentWindowEx(
    HWND ancestor, const std::wstring& window_class,
    bool only_visible, bool only_accessible, HWND* descendent) {
  FindDescendentData data(window_class, only_visible, only_accessible);
  ::EnumChildWindows(ancestor, HandleDescendentWindowProc,
                     reinterpret_cast<LPARAM>(&data));
  if (data.descendent != NULL) {
    if (descendent != NULL)
      *descendent = data.descendent;
    return true;
  } else {
    return false;
  }
}

bool FindDescendentWindow(
    HWND ancestor, const std::wstring& window_class, bool only_visible,
    HWND* descendent) {
  return FindDescendentWindowEx(ancestor, window_class,
                                only_visible, true, descendent);
}

struct FindTopLevelWindowsData {
  FindTopLevelWindowsData(const std::wstring& window_class,
                          bool check_compatibility,
                          std::set<HWND>* windows)
      : window_class(window_class),
        windows(windows),
        check_process_access(check_compatibility) {
  }
  const std::wstring& window_class;
  std::set<HWND>* windows;
  bool check_process_access;
};

BOOL CALLBACK HandleChildWindowProc(HWND window, LPARAM param) {
  DCHECK(window && param);
  FindTopLevelWindowsData* data =
      reinterpret_cast<FindTopLevelWindowsData*>(param);
  if (data != NULL && (data->window_class.empty() ||
                       IsWindowClass(window, data->window_class)) &&
      (!data->check_process_access || CanAccessWindowProcess(window))) {
    DCHECK(data->windows != NULL);
    data->windows->insert(window);
  }
  return TRUE;
}

void FindTopLevelWindowsEx(const std::wstring& window_class, bool only_visible,
                           std::set<HWND>* top_level_windows) {
  FindTopLevelWindowsData find_data(window_class, only_visible,
                                    top_level_windows);
  ::EnumWindows(HandleChildWindowProc, reinterpret_cast<LPARAM>(&find_data));
}

void FindTopLevelWindows(const std::wstring& window_class,
                         std::set<HWND>* top_level_windows) {
  FindTopLevelWindowsEx(window_class, true, top_level_windows);
}

}  // namespace window_utils
