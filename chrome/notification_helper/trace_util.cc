// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <stdarg.h>
#include <stdio.h>

#include "base/stl_util.h"
#include "base/strings/string16.h"

#if !defined(NDEBUG)
// Sends string |format| to the debugger for display.
//
// This is for developers only; we don't use this in circumstances
// (like release builds) where users could see it.
void TraceImpl(const wchar_t* format, ...) {
  constexpr int kMaxLogBufferSize = 1024;
  wchar_t buffer[kMaxLogBufferSize] = {};

  va_list args = {};

  va_start(args, format);
  if (vswprintf(buffer, base::size(buffer), format, args) > 0) {
    OutputDebugString(buffer);
  } else {
    base::string16 error_string(L"Format error for string: ");
    OutputDebugString(error_string.append(format).c_str());
  }
  va_end(args);
}
#endif  // !defined(NDEBUG)
