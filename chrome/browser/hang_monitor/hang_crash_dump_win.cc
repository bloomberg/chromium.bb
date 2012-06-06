// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hang_monitor/hang_crash_dump_win.h"

#include "chrome/common/chrome_constants.h"
#include "content/public/common/result_codes.h"

namespace {

// This function will be called via an injected thread in the hung child
// process. Calling DumpProcessWithoutCrash causes breakpad to capture a dump of
// the process.
DWORD WINAPI HungChildProcessDumpAndExit(void*) {
  typedef void (__cdecl *DumpProcessFunction)();
  DumpProcessFunction request_dump = reinterpret_cast<DumpProcessFunction>(
      GetProcAddress(GetModuleHandle(chrome::kBrowserProcessExecutableName),
                     "DumpProcessWithoutCrash"));
  if (request_dump)
    request_dump();
  return 0;
}

// How long do we wait for the terminated thread or process to die (in ms)
static const int kTerminateTimeoutMS = 2000;

// How long do we wait for the crash to be generated (in ms).
static const int kGenerateDumpTimeoutMS = 10000;

}  // namespace

void CrashDumpAndTerminateHungChildProcess(HANDLE hprocess) {
  // Before terminating the process we try collecting a dump. Which
  // a transient thread in the child process will do for us.
  HANDLE remote_thread = CreateRemoteThread(
      hprocess, NULL, 0, &HungChildProcessDumpAndExit, 0, 0, NULL);
  if (remote_thread) {
    WaitForSingleObject(remote_thread, kGenerateDumpTimeoutMS);
    CloseHandle(remote_thread);
  }

  TerminateProcess(hprocess, content::RESULT_CODE_HUNG);
  WaitForSingleObject(hprocess, kTerminateTimeoutMS);
}
