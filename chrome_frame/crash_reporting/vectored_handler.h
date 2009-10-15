// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_H_
#define CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_H_

// Base class for VectoredHandlerT just to hold some members (independent of
// template parameter)
class VectoredHandlerBase {
 public:
  // For RtlCaptureStackBackTrace MSDN says:
  // Windows Server 2003 and Windows XP:  The sum of the FramesToSkip and
  // FramesToCapture parameters must be less than 64.
  // In practice (on XPSP2) it has to be less than 63, hence leaving us with
  // max back trace of 62.
  static const DWORD max_back_trace = 62;
  static unsigned long g_exceptions_seen;
 protected:
  static void* g_handler;
};

DECLSPEC_SELECTANY void* VectoredHandlerBase::g_handler;
DECLSPEC_SELECTANY unsigned long VectoredHandlerBase::g_exceptions_seen;

// The E class is supposed to provide external/API functions. Using template
// make testability easier. It shall confirm the following concept/archetype:
// void* Register(PVECTORED_EXCEPTION_HANDLER,
//                const void* module_start, const void* module_end)
// Registers Vectored Exception Handler, non-unittest implementation shall call
// ::AddVectoredExceptionHandler Win32 API
// ULONG Unregister(void*) - ::RemoveVectoredExceptionHandler Win32 API
// int IsOurModule(const void* address) -
// void WriteDump(EXCEPTION_POINTERS*) -
// WORD RtlCaptureStackBackTrace(..) - same as Win32 API
// EXCEPTION_REGISTRATION_RECORD* RtlpGetExceptionList() - same as Win32 API
// You may want to derive own External class by deriving from
// VEHExternalBase helper (see below).
// Create dump policy:
// 1. Scan SEH chain, if there is a handler/filter that belongs to our
//    module - assume we expect this one and hence do nothing here.
// 2. If the address of the exception is in our module - create dump.
// 3. If our module is in somewhere in callstack - create dump.
template <class E>
class VectoredHandlerT : public VectoredHandlerBase {
 public:
  static void* Register(const void* module_start, const void* module_end) {
    g_exceptions_seen = 0;
    g_handler = E::Register(&VectoredHandler, module_start, module_end);
    return g_handler;
  }

  static ULONG Unregister() {
    if (g_handler)
      return E::Unregister(g_handler);
    return 0;
  }

  static LONG WINAPI VectoredHandler(EXCEPTION_POINTERS* exceptionInfo);
 private:
  static BOOL ModuleHasInstalledSEHFilter();
};

// Handy class supposed to act as a base class for classes used as template
// parameter of VectoredHandlerT<E>
class VEHTraitsBase {
 public:
  static const void* g_module_start;
  static const void* g_module_end;

  static inline int IsOurModule(const void* address) {
    return (g_module_start <= address && address < g_module_end);
  }

  static inline void SetModule(const void* module_start,
                               const void* module_end) {
    g_module_start = module_start;
    g_module_end = module_end;
  }
};

DECLSPEC_SELECTANY const void* VEHTraitsBase::g_module_start;
DECLSPEC_SELECTANY const void* VEHTraitsBase::g_module_end;

class Win32VEHTraits;
typedef class VectoredHandlerT<Win32VEHTraits> VectoredHandler;
#endif  // CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_H_
