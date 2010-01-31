// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_IMPL_H_
#define CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_IMPL_H_
#include "chrome_frame/crash_reporting/vectored_handler.h"

#if defined(_M_IX86)
#ifndef EXCEPTION_CHAIN_END
#define EXCEPTION_CHAIN_END ((struct _EXCEPTION_REGISTRATION_RECORD*)-1)
typedef struct _EXCEPTION_REGISTRATION_RECORD {
  struct _EXCEPTION_REGISTRATION_RECORD* Next;
  PVOID Handler;
} EXCEPTION_REGISTRATION_RECORD;
#endif  // EXCEPTION_CHAIN_END
#else
#error only x86 is supported for now.
#endif

// VEH handler flags settings.
// These are grabbed from winnt.h for PocketPC.
// Only EXCEPTION_NONCONTINUABLE in defined in "regular" winnt.h
// #define EXCEPTION_NONCONTINUABLE 0x1    // Noncontinuable exception
#define EXCEPTION_UNWINDING 0x2         // Unwind is in progress
#define EXCEPTION_EXIT_UNWIND 0x4       // Exit unwind is in progress
#define EXCEPTION_STACK_INVALID 0x8     // Stack out of limits or unaligned
#define EXCEPTION_NESTED_CALL 0x10      // Nested exception handler call
#define EXCEPTION_TARGET_UNWIND 0x20    // Target unwind in progress
#define EXCEPTION_COLLIDED_UNWIND 0x40  // Collided exception handler call

#define EXCEPTION_UNWIND (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND | \
    EXCEPTION_TARGET_UNWIND | EXCEPTION_COLLIDED_UNWIND)

#define IS_UNWINDING(Flag)  (((Flag) & EXCEPTION_UNWIND) != 0)
#define IS_DISPATCHING(Flag)  (((Flag) & EXCEPTION_UNWIND) == 0)
#define IS_TARGET_UNWIND(Flag)  ((Flag) & EXCEPTION_TARGET_UNWIND)
// End of grabbed section

template <typename E>
VectoredHandlerT<E>::VectoredHandlerT(E* api) : exceptions_seen_(0), api_(api) {
}

template <typename E>
VectoredHandlerT<E>::~VectoredHandlerT() {
}

template <typename E>
LONG VectoredHandlerT<E>::Handler(EXCEPTION_POINTERS* exceptionInfo) {
  // TODO(stoyan): Consider reentrancy
  const DWORD exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;

  // Not interested in non-error exceptions. In this category falls exceptions
  // like:
  // 0x40010006 - OutputDebugStringA. Seen when no debugger is attached
  //              (otherwise debugger swallows the exception and prints
  //              the string).
  // 0x406D1388 - DebuggerProbe. Used by debug CRT - for example see source
  //              code of isatty(). Used to name a thread as well.
  // RPC_E_DISCONNECTED and Co. - COM IPC non-fatal warnings
  // STATUS_BREAKPOINT and Co. - Debugger related breakpoints
  if ((exceptionCode & ERROR_SEVERITY_ERROR) != ERROR_SEVERITY_ERROR) {
    return ExceptionContinueSearch;
  }

  // Ignore custom exception codes.
  // MSXML likes to raise 0xE0000001 while parsing.
  // Note the C++ SEH (0xE06D7363) also fails in that range.
  if (exceptionCode & APPLICATION_ERROR_MASK) {
    return ExceptionContinueSearch;
  }

  exceptions_seen_++;

  // If the exception code is STATUS_STACK_OVERFLOW then proceed as usual -
  // we want to report it. Otherwise check whether guard page in stack is gone -
  // i.e. stack overflow has been already observed and most probably we are
  // seeing the follow-up STATUS_ACCESS_VIOLATION(s). See bug 32441.
  if (exceptionCode != STATUS_STACK_OVERFLOW && api_->CheckForStackOverflow()) {
    return ExceptionContinueSearch;
  }

  // Check whether exception address is inbetween
  // [IsBadReadPtr, IsBadReadPtr + 0xXX]
  if (api_->ShouldIgnoreException(exceptionInfo)) {
    return ExceptionContinueSearch;
  }

  const DWORD exceptionFlags = exceptionInfo->ExceptionRecord->ExceptionFlags;
  // I don't think VEH is called on unwind. Just to be safe.
  if (IS_DISPATCHING(exceptionFlags)) {
    if (ModuleHasInstalledSEHFilter())
      return ExceptionContinueSearch;

    if (api_->IsOurModule(exceptionInfo->ExceptionRecord->ExceptionAddress)) {
      api_->WriteDump(exceptionInfo);
      return ExceptionContinueSearch;
    }

    // See whether our module is somewhere in the call stack.
    void* back_trace[api_->max_back_trace] = {0};
    DWORD captured = api_->RtlCaptureStackBackTrace(0, api_->max_back_trace,
                                                    &back_trace[0], NULL);
    for (DWORD i = 0; i < captured; ++i) {
      if (api_->IsOurModule(back_trace[i])) {
        api_->WriteDump(exceptionInfo);
        return ExceptionContinueSearch;
      }
    }
  }

  return ExceptionContinueSearch;
}

template <typename E>
bool VectoredHandlerT<E>::ModuleHasInstalledSEHFilter() {
  const EXCEPTION_REGISTRATION_RECORD* RegistrationFrame =
      api_->RtlpGetExceptionList();
  // TODO(stoyan): Add the stack limits check and some sanity checks like
  // decreasing addresses of registration records
  while (RegistrationFrame != EXCEPTION_CHAIN_END) {
    if (api_->IsOurModule(RegistrationFrame->Handler)) {
      return true;
    }

    RegistrationFrame = RegistrationFrame->Next;
  }

  return false;
}


// Here comes the default Windows 32-bit implementation.
namespace {
  __declspec(naked)
    static EXCEPTION_REGISTRATION_RECORD* InternalRtlpGetExceptionList() {
      __asm {
        mov eax, fs:0
        ret
      }
  }
  __declspec(naked)
  static char* GetStackTopLimit() {
    __asm {
      mov eax, fs:8
      ret
    }
  }
}  // end of namespace

// Class which methods simply forwards to Win32 API.
// Used as template (external interface) of VectoredHandlerT<E>.
class Win32VEHTraits {
 public:
  enum {max_back_trace = 62};

  static inline
  EXCEPTION_REGISTRATION_RECORD* RtlpGetExceptionList() {
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
      DCHECK(code_block.code) << "Win32VEHTraits::CodeBlocks not initialized!";
      if ((CodeOffset(code_block.code, code_block.begin_offset) <= address) &&
          (address < CodeOffset(code_block.code, code_block.end_offset))) {
        return true;
      }
    }

    return false;
  }

  static bool CheckForStackOverflow() {
    MEMORY_BASIC_INFORMATION mi;
    const DWORD kPageSize = 0x1000;
    void* stack_top = GetStackTopLimit() - kPageSize;
    ::VirtualQuery(stack_top, &mi, sizeof(mi));
    // The above call may result in moving the top of the stack.
    // Check once more.
    void* stack_top2 = GetStackTopLimit() - kPageSize;
    if (stack_top2 != stack_top)
      ::VirtualQuery(stack_top2, &mi, sizeof(mi));
    return !(mi.Protect & PAGE_GUARD);
  }

  static void InitializeIgnoredBlocks() {
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

 private:
  static inline const void* CodeOffset(const void* code, int offset) {
    return reinterpret_cast<const char*>(code) + offset;
  }

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

DECLSPEC_SELECTANY Win32VEHTraits::CodeBlock
Win32VEHTraits::IgnoreExceptions[kIgnoreEntries] = {
  { "kernel32.dll", "IsBadReadPtr", 0, 100, NULL },
  { "kernel32.dll", "IsBadWritePtr", 0, 100, NULL },
  { "kernel32.dll", "IsBadStringPtrA", 0, 100, NULL },
  { "kernel32.dll", "IsBadStringPtrW", 0, 100, NULL },
};

#endif  // CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_IMPL_H_
