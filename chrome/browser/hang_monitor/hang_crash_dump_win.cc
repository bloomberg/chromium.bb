// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hang_monitor/hang_crash_dump_win.h"

#include "base/logging.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/common/result_codes.h"
#include "ppapi/shared_impl/ppapi_message_tracker.h"

namespace {

// How long do we wait for the terminated thread or process to die (in ms)
static const int kTerminateTimeoutMS = 2000;

// How long do we wait for the crash to be generated (in ms).
static const int kGenerateDumpTimeoutMS = 10000;

DWORD WINAPI DumpIfHandlingPepper(void*) {
  typedef void (__cdecl *DumpFunction)();
  if (ppapi::PpapiMessageTracker::GetInstance()->IsHandlingMessage()) {
    DumpFunction request_dump = reinterpret_cast<DumpFunction>(GetProcAddress(
        GetModuleHandle(chrome::kBrowserProcessExecutableName),
        "DumpProcessWithoutCrash"));
    DCHECK(request_dump) << "Failed loading DumpProcessWithoutCrash: error " <<
        GetLastError();
    if (request_dump)
      request_dump();
  }
  return 0;
}

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

void CrashDumpIfProcessHandlingPepper(HANDLE hprocess) {
  // Unlike CrashDumpAndTerminateHungChildProcess() which creates a remote
  // thread using function pointer relative to chrome.exe, here we create a
  // remote thread using function pointer relative to chrome.dll. The reason is
  // that there are separate PpapiMessageTracker singletons for chrome.dll and
  // chrome.exe (in non-component build). We cannot access the information
  // collected by PpapiMessageTracker of chrome.dll in chrome.exe.
  //
  // This is less safe, because chrome.dll may be loaded at different addresses
  // in different processes. We could cause crash in that case. However, it
  // should be rare and we are only doing this temporarily for debugging on the
  // Canary channel.
  HANDLE remote_thread = CreateRemoteThread(hprocess, NULL, 0,
                                            DumpIfHandlingPepper, 0, 0, NULL);
  DCHECK(remote_thread) << "Failed creating remote thread: error " <<
      GetLastError();
  if (remote_thread) {
    WaitForSingleObject(remote_thread, kGenerateDumpTimeoutMS);
    CloseHandle(remote_thread);
  }
}
