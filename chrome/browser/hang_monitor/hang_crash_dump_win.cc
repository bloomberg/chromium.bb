// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hang_monitor/hang_crash_dump_win.h"

#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "components/crash/content/app/crash_export_thunks.h"
#include "content/public/common/result_codes.h"

namespace {

// How long do we wait for the terminated thread or process to die (in ms)
static const int kTerminateTimeoutMS = 2000;

// How long do we wait for the crash to be generated (in ms).
static const int kGenerateDumpTimeoutMS = 10000;

}  // namespace

void CrashDumpAndTerminateHungChildProcess(HANDLE hprocess) {
  // Before terminating the process we try collecting a dump. Which
  // a transient thread in the child process will do for us.
  HANDLE remote_thread = InjectDumpForHungInput_ExportThunk(hprocess);
  DCHECK(remote_thread) << "Failed creating remote thread: error "
                        << GetLastError();
  if (remote_thread) {
    WaitForSingleObject(remote_thread, kGenerateDumpTimeoutMS);
    CloseHandle(remote_thread);
  }

  TerminateProcess(hprocess, content::RESULT_CODE_HUNG);
  WaitForSingleObject(hprocess, kTerminateTimeoutMS);
}
