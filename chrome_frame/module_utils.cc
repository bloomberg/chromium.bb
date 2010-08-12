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
  wnd_class.hCursor = LoadCursor(NULL, IDC_ARROW);
  wnd_class.lpszClassName = kBeaconWindowClassName;

  HMODULE this_module = reinterpret_cast<HMODULE>(&__ImageBase);
  wnd_class.lpfnWndProc = reinterpret_cast<WNDPROC>(this_module);

  atom_ = RegisterClassEx(&wnd_class);
  return (atom_ != 0);
}

void DllRedirector::UnregisterAsFirstCFModule() {
  if (atom_) {
    UnregisterClass(MAKEINTATOM(atom_), NULL);
    atom_ = NULL;
  }
}

HMODULE DllRedirector::GetFirstCFModule() {
  WNDCLASSEX wnd_class = {0};
  HMODULE oldest_module = NULL;
  if (GetClassInfoEx(GetModuleHandle(NULL), kBeaconWindowClassName,
                     &wnd_class)) {
    oldest_module = reinterpret_cast<HMODULE>(wnd_class.lpfnWndProc);
    // Handle older versions that store module pointer in a class info.
    // TODO(amit): Remove this in future versions.
    if (reinterpret_cast<HMODULE>(DefWindowProc) == oldest_module) {
      WNDCLASSEX wnd_class = {0};
      HWND hwnd = CreateWindow(kBeaconWindowClassName, L"temp_window",
                               WS_POPUP, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
      DCHECK(IsWindow(hwnd));
      if (hwnd) {
        oldest_module = reinterpret_cast<HMODULE>(GetClassLongPtr(hwnd, 0));
        DestroyWindow(hwnd);
      }
    }
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
