// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_IMPL_H_
#define CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_IMPL_H_
#include "chrome_frame/crash_reporting/vectored_handler.h"

#if defined(_M_IX86)
typedef struct _EXCEPTION_REGISTRATION_RECORD {
  struct _EXCEPTION_REGISTRATION_RECORD* Next;
  PVOID Handler;
} EXCEPTION_REGISTRATION_RECORD;
#define EXCEPTION_CHAIN_END ((struct _EXCEPTION_REGISTRATION_RECORD*)-1)
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

template <class E>
LONG WINAPI VectoredHandlerT<E>::VectoredHandler(
    EXCEPTION_POINTERS* exceptionInfo) {
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

  ++VectoredHandlerT<E>::g_exceptions_seen;

  // TODO(stoyan): Check whether exception address is inbetween
  // [IsBadReadPtr, IsBadReadPtr + 0xXX]

  const DWORD exceptionFlags = exceptionInfo->ExceptionRecord->ExceptionFlags;
  // I don't think VEH is called on unwind. Just to be safe.
  if (IS_DISPATCHING(exceptionFlags)) {
    if (ModuleHasInstalledSEHFilter())
      return ExceptionContinueSearch;

    if (E::IsOurModule(exceptionInfo->ExceptionRecord->ExceptionAddress)) {
      E::WriteDump(exceptionInfo);
      return ExceptionContinueSearch;
    }

    // See whether our module is somewhere in the call stack.
    void* back_trace[max_back_trace] = {0};
    // Skip RtlCaptureStackBackTrace and VectoredHandler itself.
    DWORD captured = E::RtlCaptureStackBackTrace(2, max_back_trace - 2,
                                                 &back_trace[0], NULL);
    for (DWORD i = 0; i < captured; ++i) {
     if (E::IsOurModule(back_trace[i])) {
       E::WriteDump(exceptionInfo);
       return ExceptionContinueSearch;
     }
    }
  }

  return ExceptionContinueSearch;
}

template <class E>
BOOL VectoredHandlerT<E>::ModuleHasInstalledSEHFilter() {
  EXCEPTION_REGISTRATION_RECORD* RegistrationFrame = E::RtlpGetExceptionList();
  // TODO(stoyan): Add the stack limits check and some sanity checks like
  // decreasing addresses of registration records
  while (RegistrationFrame != EXCEPTION_CHAIN_END) {
    if (E::IsOurModule(RegistrationFrame->Handler)) {
      return TRUE;
    }

    RegistrationFrame = RegistrationFrame->Next;
  }

  return FALSE;
}
#endif  // CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_IMPL_H_
