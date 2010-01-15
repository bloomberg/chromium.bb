
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// crash_report.cc : Implementation crash reporting.
#include "chrome_frame/crash_reporting/crash_report.h"

#include "base/basictypes.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "chrome_frame/crash_reporting/vectored_handler.h"

// TODO(joshia): factor out common code with chrome used for crash reporting
const wchar_t kGoogleUpdatePipeName[] = L"\\\\.\\pipe\\GoogleCrashServices\\";
google_breakpad::ExceptionHandler* g_breakpad = NULL;

Win32VEHTraits::CodeBlock Win32VEHTraits::IgnoreExceptions[kIgnoreEntries] = {
  { "kernel32.dll", "IsBadReadPtr", 0, 100, NULL },
  { "kernel32.dll", "IsBadWritePtr", 0, 100, NULL },
  { "kernel32.dll", "IsBadStringPtrA", 0, 100, NULL },
  { "kernel32.dll", "IsBadStringPtrW", 0, 100, NULL },
};

std::wstring GetCrashServerPipeName(const std::wstring& user_sid) {
  std::wstring pipe_name = kGoogleUpdatePipeName;
  pipe_name += user_sid;
  return pipe_name;
}

bool InitializeVectoredCrashReportingWithPipeName(
    bool full_dump,
    const wchar_t* pipe_name,
    const std::wstring& dump_path,
    google_breakpad::CustomClientInfo* client_info) {
  if (g_breakpad)
    return true;

  if (dump_path.empty()) {
    return false;
  }

  MINIDUMP_TYPE dump_type = full_dump ? MiniDumpWithFullMemory : MiniDumpNormal;
  g_breakpad = new google_breakpad::ExceptionHandler(
      dump_path, NULL, NULL, NULL,
      google_breakpad::ExceptionHandler::HANDLER_INVALID_PARAMETER |
      google_breakpad::ExceptionHandler::HANDLER_PURECALL, dump_type,
      pipe_name, client_info);

  if (g_breakpad) {
    // Find current module boundaries.
    const void* start = &__ImageBase;
    const char* s = reinterpret_cast<const char*>(start);
    const IMAGE_NT_HEADERS32* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>
        (s + __ImageBase.e_lfanew);
    const void* end = s + nt->OptionalHeader.SizeOfImage;
    VectoredHandler::Register(start, end);
  }

  return g_breakpad != NULL;
}

bool InitializeVectoredCrashReporting(
    bool full_dump,
    const wchar_t* user_sid,
    const std::wstring& dump_path,
    google_breakpad::CustomClientInfo* client_info) {
  DCHECK(user_sid);
  DCHECK(client_info);

  std::wstring pipe_name = GetCrashServerPipeName(user_sid);

  return InitializeVectoredCrashReportingWithPipeName(full_dump,
                                                      pipe_name.c_str(),
                                                      dump_path,
                                                      client_info);
}

bool ShutdownVectoredCrashReporting() {
  VectoredHandler::Unregister();
  delete g_breakpad;
  g_breakpad = NULL;
  return true;
}
