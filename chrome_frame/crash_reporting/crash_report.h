// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// crash_report.h : Declarations for crash reporting.

#ifndef CHROME_FRAME_CRASH_REPORTING_CRASH_REPORT_H_
#define CHROME_FRAME_CRASH_REPORTING_CRASH_REPORT_H_

#include <string>

#include "base/logging.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "chrome_frame/crash_reporting/vectored_handler-impl.h"

extern google_breakpad::ExceptionHandler* g_breakpad;
extern "C" IMAGE_DOS_HEADER __ImageBase;

bool InitializeVectoredCrashReporting(
    bool full_dump,
    const wchar_t* user_sid,
    const std::wstring& dump_path,
    google_breakpad::CustomClientInfo* client_info);

bool InitializeVectoredCrashReportingWithPipeName(
    bool full_dump,
    const wchar_t* pipe_name,
    const std::wstring& dump_path,
    google_breakpad::CustomClientInfo* client_info);

bool ShutdownVectoredCrashReporting();

namespace {
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
    InitializeIgnoredBlocks();
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

  static bool ShouldIgnoreException(const EXCEPTION_POINTERS* exceptionInfo) {
    const void* address = exceptionInfo->ExceptionRecord->ExceptionAddress;
    for (int i = 0; i < kIgnoreEntries; i++) {
      const CodeBlock& code_block = IgnoreExceptions[i];
      if (code_block.code &&
         (CodeOffset(code_block.code, code_block.begin_offset) <= address) &&
         (address < CodeOffset(code_block.code, code_block.end_offset))) {
        return true;
      }
    }

    return false;
  }

  static inline void InitializeIgnoredBlocks() {
    // Initialize ignored exception list
    for (int i = 0; i < kIgnoreEntries; i++) {
      CodeBlock& code_block = IgnoreExceptions[i];
      if (!code_block.code) {
        HMODULE module = GetModuleHandleA(code_block.module);
        DCHECK(module) << "GetModuleHandle error: " << GetLastError();
        code_block.code = GetProcAddress(module, code_block.function);
        DCHECK(code_block.code) << "GetProcAddress error: "<< GetLastError();
      }
    }
  }

  static inline const void* CodeOffset(const void* code, int offset) {
    return reinterpret_cast<const char*>(code) + offset;
  }

 private:
  // Block of code to be ignored for exceptions
  struct CodeBlock {
    char* module;
    char* function;
    int begin_offset;
    int end_offset;
    const void* code;
  };

  static const int kIgnoreEntries = 4;
  static CodeBlock IgnoreExceptions[kIgnoreEntries];
};


#endif  // CHROME_FRAME_CRASH_REPORTING_CRASH_REPORT_H_
