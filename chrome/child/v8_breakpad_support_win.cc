// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/child/v8_breakpad_support_win.h"

#include <windows.h>

#include "base/logging.h"
#include "chrome/common/chrome_constants.h"
#include "gin/public/debug.h"

namespace v8_breakpad_support {

void SetUp() {
#ifdef _WIN64
  // Get the breakpad pointer from chrome.exe
  gin::Debug::CodeRangeCreatedCallback create_callback =
      reinterpret_cast<gin::Debug::CodeRangeCreatedCallback>(
          ::GetProcAddress(::GetModuleHandle(chrome::kChromeElfDllName),
                           "RegisterNonABICompliantCodeRange"));
  gin::Debug::CodeRangeDeletedCallback delete_callback =
      reinterpret_cast<gin::Debug::CodeRangeDeletedCallback>(
          ::GetProcAddress(::GetModuleHandle(chrome::kChromeElfDllName),
                           "UnregisterNonABICompliantCodeRange"));
  // When running e.g. browser_tests.exe, these might be NULL.
  if (create_callback && delete_callback) {
    gin::Debug::SetCodeRangeCreatedCallback(create_callback);
    gin::Debug::SetCodeRangeDeletedCallback(delete_callback);
  }
#endif
}

}  // namespace v8_breakpad_support
