// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hang_monitor/hang_crash_dump_win.h"

#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/common/result_codes.h"

namespace {

// How long do we wait for the terminated thread or process to die (in ms)
static const int kTerminateTimeoutMS = 2000;

// How long do we wait for the crash to be generated (in ms).
static const int kGenerateDumpTimeoutMS = 10000;

enum NoCrashKeyReason {
  kNoCrashKeyReasonVirtualAlloc = 1,
  kNoCrashKeyReasonWriteProcessMemory,
  kNoCrashKeyReasonNoKeys
};

}  // namespace

void CrashDumpAndTerminateHungChildProcess(
    HANDLE hprocess,
    const base::StringPairs& additional_child_crash_keys) {
  // Before terminating the process we try collecting a dump. Which
  // a transient thread in the child process will do for us.
  typedef HANDLE(__cdecl * DumpFunction)(HANDLE, void*);
  typedef HANDLE(__cdecl * DumpFunctionNoCrashKeys)(HANDLE, int);
  static DumpFunction request_dump = nullptr;
  static DumpFunctionNoCrashKeys request_dump_no_crash_keys = nullptr;
  if (!request_dump) {
    request_dump = reinterpret_cast<DumpFunction>(GetProcAddress(
        GetModuleHandle(chrome::kChromeElfDllName), "InjectDumpForHungInput"));
    DCHECK(request_dump) << "Failed loading DumpProcessWithoutCrash: error " <<
        GetLastError();
  }
  if (!request_dump_no_crash_keys) {
    request_dump_no_crash_keys = reinterpret_cast<DumpFunctionNoCrashKeys>(
        GetProcAddress(GetModuleHandle(chrome::kChromeElfDllName),
                       "InjectDumpForHungInputNoCrashKeys"));
    DCHECK(request_dump_no_crash_keys)
        << "Failed loading DumpProcessWithoutCrash: error " << GetLastError();
  }

  DWORD crash_key_failure = 0;
  void* remote_memory = nullptr;
  bool send_remote_memory = false;
  std::vector<std::string> keys;
  for (const auto& crash_key : additional_child_crash_keys) {
    DCHECK(base::debug::LookupCrashKey(crash_key.first));
    std::string serialized_key = crash_key.first;
    serialized_key.append(":");
    serialized_key.append(crash_key.second);
    keys.push_back(serialized_key);
  }
  std::string serialized_keys = base::JoinString(keys, ",");

  if (!serialized_keys.empty()) {
    size_t data_length = serialized_keys.length() + 1;
    remote_memory = VirtualAllocEx(hprocess, nullptr, data_length,
                                   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote_memory) {
      send_remote_memory =
          !!WriteProcessMemory(hprocess, remote_memory, serialized_keys.c_str(),
                               data_length, nullptr);
      crash_key_failure =
          MAKELPARAM(kNoCrashKeyReasonWriteProcessMemory, GetLastError());
    } else {
      crash_key_failure =
          MAKELPARAM(kNoCrashKeyReasonVirtualAlloc, GetLastError());
    }
  } else {
    crash_key_failure = MAKELPARAM(kNoCrashKeyReasonNoKeys, 0);
  }

  HANDLE remote_thread = nullptr;
  if (request_dump && send_remote_memory) {
    remote_thread = request_dump(hprocess, remote_memory);
  } else if (request_dump_no_crash_keys) {
    remote_thread = request_dump_no_crash_keys(hprocess, crash_key_failure);
  }
  DCHECK(remote_thread) << "Failed creating remote thread: error "
                        << GetLastError();
  if (remote_thread) {
    WaitForSingleObject(remote_thread, kGenerateDumpTimeoutMS);
    CloseHandle(remote_thread);
  }

  TerminateProcess(hprocess, content::RESULT_CODE_HUNG);
  WaitForSingleObject(hprocess, kTerminateTimeoutMS);
}
