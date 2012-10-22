// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"

#include "base/rand_util.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "chrome/common/chrome_constants.h"

namespace browser_sync {

static const double kErrorUploadRatio = 0.15;
void ChromeReportUnrecoverableError() {
  // TODO(lipalani): Add this for other platforms as well.
#if defined(OS_WIN)
  // We only want to upload |kErrorUploadRatio| ratio of errors.
  if (kErrorUploadRatio <= 0.0)
    return; // We are not allowed to upload errors.
  double random_number = base::RandDouble();
  if (random_number > kErrorUploadRatio)
    return;

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
