
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// crash_report.cc : Implementation crash reporting.
#include "chrome_frame/crash_reporting/crash_report.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "chrome_frame/crash_reporting/vectored_handler.h"
#include "chrome_frame/crash_reporting/vectored_handler-impl.h"
#include "chrome_frame/utils.h"

namespace {
// TODO(joshia): factor out common code with chrome used for crash reporting
const wchar_t kGoogleUpdatePipeName[] = L"\\\\.\\pipe\\GoogleCrashServices\\";
const wchar_t kChromePipeName[] = L"\\\\.\\pipe\\ChromeCrashServices";

google_breakpad::ExceptionHandler* g_breakpad = NULL;

__declspec(naked)
static EXCEPTION_REGISTRATION_RECORD* InternalRtlpGetExceptionList() {
  __asm {
    mov eax, fs:0
    ret
  }
}
}  // end of namespace

// Class which methods simply forwards to Win32 API and uses breakpad to write
// a minidump. Used as template (external interface) of VectoredHandlerT<E>.
class Win32VEHTraits : public VEHTraitsBase {
 public:
  static inline void* Register(PVECTORED_EXCEPTION_HANDLER func,
      const void* module_start, const void* module_end) {
    VEHTraitsBase::SetModule(module_start, module_end);
    return ::AddVectoredExceptionHandler(1, func);
  }

  static inline ULONG Unregister(void* handle) {
    return ::RemoveVectoredExceptionHandler(handle);
  }

  static inline bool WriteDump(EXCEPTION_POINTERS* p) {
    return g_breakpad->WriteMinidumpForException(p);
  }

  static inline EXCEPTION_REGISTRATION_RECORD* RtlpGetExceptionList() {
    return InternalRtlpGetExceptionList();
  }

  static inline WORD RtlCaptureStackBackTrace(DWORD FramesToSkip,
      DWORD FramesToCapture, void** BackTrace, DWORD* BackTraceHash) {
    return ::RtlCaptureStackBackTrace(FramesToSkip, FramesToCapture,
                                      BackTrace, BackTraceHash);
  }
};

extern "C" IMAGE_DOS_HEADER __ImageBase;

std::wstring GetCrashServerPipeName(const std::wstring& user_sid) {
  if (IsHeadlessMode())
    return kChromePipeName;

  std::wstring pipe_name = kGoogleUpdatePipeName;
  pipe_name += user_sid;
  return pipe_name;
}

bool InitializeVectoredCrashReporting(
    bool full_dump,
    const wchar_t* user_sid,
    const std::wstring& dump_path,
    google_breakpad::CustomClientInfo* client_info) {
  DCHECK(user_sid);
  DCHECK(client_info);
  if (g_breakpad)
    return true;

  std::wstring pipe_name = GetCrashServerPipeName(user_sid);

  if (dump_path.empty()) {
    return false;
  }

  MINIDUMP_TYPE dump_type = full_dump ? MiniDumpWithFullMemory : MiniDumpNormal;
  g_breakpad = new google_breakpad::ExceptionHandler(
      dump_path, NULL, NULL, NULL,
      google_breakpad::ExceptionHandler::HANDLER_INVALID_PARAMETER |
      google_breakpad::ExceptionHandler::HANDLER_PURECALL, dump_type,
      pipe_name.c_str(), client_info);

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

bool ShutdownVectoredCrashReporting() {
  VectoredHandler::Unregister();
  delete g_breakpad;
  g_breakpad = NULL;
  return true;
}
