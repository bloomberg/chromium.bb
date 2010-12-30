// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_WIN_HWND_UTIL_H_
#define APP_WIN_HWND_UTIL_H_
#pragma once

#include <windows.h>

#include "base/string16.h"

namespace app {
namespace win {

// A version of the GetClassNameW API that returns the class name in an
// string16. An empty result indicates a failure to get the class name.
string16 GetClassName(HWND hwnd);

// Useful for subclassing a HWND.  Returns the previous window procedure.
WNDPROC SetWindowProc(HWND hwnd, WNDPROC wndproc);

// Pointer-friendly wrappers around Get/SetWindowLong(..., GWLP_USERDATA, ...)
// Returns the previously set value.
void* SetWindowUserData(HWND hwnd, void* user_data);
void* GetWindowUserData(HWND hwnd);

}  // namespace win
}  // namespace app

#endif  // APP_WIN_HWND_UTIL_H_
