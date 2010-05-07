// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/module_utils.h"

#include <atlbase.h>
#include "base/logging.h"

const wchar_t kBeaconWindowClassName[] =
    L"ChromeFrameBeaconWindowClass826C5D01-E355-4b23-8AC2-40650E0B7843";

// static
ATOM DllRedirector::atom_ = 0;

bool DllRedirector::RegisterAsFirstCFModule() {
  // This would imply that this module had already registered a window class
  // which should never happen.
  if (atom_) {
    NOTREACHED();
    return true;
  }

  WNDCLASSEX wnd_class = {0};
  wnd_class.cbSize = sizeof(WNDCLASSEX);
  wnd_class.style = CS_GLOBALCLASS;
  wnd_class.lpfnWndProc = &DefWindowProc;
  wnd_class.cbClsExtra = sizeof(HMODULE);
  wnd_class.cbWndExtra = 0;
  wnd_class.hInstance = NULL;
  wnd_class.hIcon = NULL;

  wnd_class.hCursor = LoadCursor(NULL, IDC_ARROW);
  wnd_class.hbrBackground = NULL;
  wnd_class.lpszMenuName = NULL;
  wnd_class.lpszClassName = kBeaconWindowClassName;
  wnd_class.hIconSm = wnd_class.hIcon;

  ATOM atom = RegisterClassEx(&wnd_class);

  bool success = false;
  if (atom != 0) {
    HWND hwnd = CreateWindow(MAKEINTATOM(atom), L"temp_window", WS_POPUP,
                             0, 0, 0, 0, NULL, NULL, NULL, NULL);
    DCHECK(IsWindow(hwnd));
    if (hwnd) {
      HMODULE this_module = reinterpret_cast<HMODULE>(&__ImageBase);
      LONG_PTR lp = reinterpret_cast<LONG_PTR>(this_module);
      SetClassLongPtr(hwnd, 0, lp);
      // We need to check the GLE value since SetClassLongPtr returns 0 on
      // failure as well as on the first call.
      if (GetLastError() == ERROR_SUCCESS) {
        atom_ = atom;
        success = true;
      }
      DestroyWindow(hwnd);
    }
  }

  return success;
}

void DllRedirector::UnregisterAsFirstCFModule() {
  if (atom_) {
    UnregisterClass(MAKEINTATOM(atom_), NULL);
  }
}

HMODULE DllRedirector::GetFirstCFModule() {
  WNDCLASSEX wnd_class = {0};
  HMODULE oldest_module = NULL;
  HWND hwnd = CreateWindow(kBeaconWindowClassName, L"temp_window", WS_POPUP, 0,
                           0, 0, 0, NULL, NULL, NULL, NULL);
  DCHECK(IsWindow(hwnd));
  if (hwnd) {
    oldest_module = reinterpret_cast<HMODULE>(GetClassLongPtr(hwnd, 0));
    DestroyWindow(hwnd);
  }
  return oldest_module;
}

LPFNGETCLASSOBJECT DllRedirector::GetDllGetClassObjectPtr(HMODULE module) {
  LPFNGETCLASSOBJECT proc_ptr = NULL;
  HMODULE temp_handle = 0;
  // Increment the module ref count while we have an pointer to its
  // DllGetClassObject function.
  if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                        reinterpret_cast<LPCTSTR>(module),
                        &temp_handle)) {
    proc_ptr = reinterpret_cast<LPFNGETCLASSOBJECT>(
        GetProcAddress(temp_handle, "DllGetClassObject"));
    if (!proc_ptr) {
      FreeLibrary(temp_handle);
      LOG(ERROR) << "Module Scan: Couldn't get address of "
                 << "DllGetClassObject: "
                 << GetLastError();
    }
  } else {
    LOG(ERROR) << "Module Scan: Could not increment module count: "
               << GetLastError();
  }
  return proc_ptr;
}
