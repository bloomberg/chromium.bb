// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "chrome/common/chrome_constants.h"

namespace browser_sync {

void ChromeReportUnrecoverableError() {
  // TODO(lipalani): Add this for other platforms as well.
#if defined(OS_WIN)
  // Get the breakpad pointer from chrome.exe
  typedef void (__cdecl *DumpProcessFunction)();
  DumpProcessFunction DumpProcess = reinterpret_cast<DumpProcessFunction>(
      ::GetProcAddress(::GetModuleHandle(
                       chrome::kBrowserProcessExecutableName),
                       "DumpProcessWithoutCrash"));
  if (DumpProcess)
    DumpProcess();
#endif  // OS_WIN

}

}  // namespace browser_sync
