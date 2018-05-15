// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hang_monitor/hang_crash_dump.h"

#include <windows.h>

#include "base/logging.h"
#include "components/crash/content/app/crash_export_thunks.h"

namespace {

// How long do we wait for the crash to be generated (in ms).
static const int kGenerateDumpTimeoutMS = 10000;

}  // namespace

void CrashDumpHungChildProcess(base::ProcessHandle handle) {
  HANDLE remote_thread = InjectDumpForHungInput_ExportThunk(handle);
  DCHECK(remote_thread) << "Failed creating remote thread: error "
                        << GetLastError();
  if (remote_thread) {
    WaitForSingleObject(remote_thread, kGenerateDumpTimeoutMS);
    CloseHandle(remote_thread);
  }
}
