// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hang_monitor/hang_crash_dump_win.h"

#include "base/logging.h"
#include "chrome/common/chrome_constants.h"
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
  typedef HANDLE (__cdecl *DumpFunction)(HANDLE);
  static DumpFunction request_dump = NULL;
  if (!request_dump) {
    request_dump = reinterpret_cast<DumpFunction>(GetProcAddress(
        GetModuleHandle(chrome::kBrowserProcessExecutableName),
            "InjectDumpProcessWithoutCrash"));
    DCHECK(request_dump) << "Failed loading DumpProcessWithoutCrash: error " <<
        GetLastError();
  }

  if (request_dump) {
    HANDLE remote_thread = request_dump(hprocess);
    DCHECK(remote_thread) << "Failed creating remote thread: error " <<
        GetLastError();
    if (remote_thread) {
      WaitForSingleObject(remote_thread, kGenerateDumpTimeoutMS);
      CloseHandle(remote_thread);
    }
  }

  TerminateProcess(hprocess, content::RESULT_CODE_HUNG);
  WaitForSingleObject(hprocess, kTerminateTimeoutMS);
}

void CrashDumpForHangDebugging(HANDLE hprocess) {
  if (hprocess == GetCurrentProcess()) {
    typedef void (__cdecl *DumpFunction)();
    DumpFunction request_dump = reinterpret_cast<DumpFunction>(GetProcAddress(
        GetModuleHandle(chrome::kBrowserProcessExecutableName),
        "DumpProcessWithoutCrash"));
    DCHECK(request_dump) << "Failed loading DumpProcessWithoutCrash: error " <<
        GetLastError();
    if (request_dump)
      request_dump();
  } else {
    typedef HANDLE (__cdecl *DumpFunction)(HANDLE);
    DumpFunction request_dump = reinterpret_cast<DumpFunction>(GetProcAddress(
        GetModuleHandle(chrome::kBrowserProcessExecutableName),
        "InjectDumpForHangDebugging"));
    DCHECK(request_dump) << "Failed loading InjectDumpForHangDebugging: error "
                         << GetLastError();
    if (request_dump) {
      HANDLE remote_thread = request_dump(hprocess);
      DCHECK(remote_thread) << "Failed creating remote thread: error " <<
          GetLastError();
      if (remote_thread) {
        WaitForSingleObject(remote_thread, kGenerateDumpTimeoutMS);
        CloseHandle(remote_thread);
      }
    }
  }
}
