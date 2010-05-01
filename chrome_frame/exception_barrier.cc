// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to make it easy to tag exception propagation boundaries and
// get crash reports of exceptions that pass over same.
#include "chrome_frame/exception_barrier.h"

#include "chrome_frame/crash_reporting/vectored_handler-impl.h"
#include "chrome_frame/crash_reporting/crash_report.h"

enum {
  // Flag set by exception handling machinery when unwinding
  EH_UNWINDING = 0x00000002
};

bool ExceptionBarrierConfig::s_enabled_ = false;

ExceptionBarrierCustomHandler::CustomExceptionHandler
    ExceptionBarrierCustomHandler::s_custom_handler_ = NULL;

// This function must be extern "C" to match up with the SAFESEH
// declaration in our corresponding ASM file
extern "C" EXCEPTION_DISPOSITION __cdecl
ExceptionBarrierHandler(struct _EXCEPTION_RECORD* exception_record,
                        void* establisher_frame,
                        struct _CONTEXT* context,
                        void* reserved) {
  // When the exception is really propagating through us, we'd like to be
  // called before the state of the program has been modified by the stack
  // unwinding. In the absence of an exception handler, the unhandled
  // exception filter gets called between the first chance and the second
  // chance exceptions, so Windows pops either the JIT debugger or WER UI.
  // This is not desirable in most of the cases.
  EXCEPTION_POINTERS ptrs = { exception_record, context };

  if (ExceptionBarrierConfig::enabled() &&
      IS_DISPATCHING(exception_record->ExceptionFlags)) {
    WriteMinidumpForException(&ptrs);
  }

  return ExceptionContinueSearch;
}

extern "C" EXCEPTION_DISPOSITION __cdecl
ExceptionBarrierReportOnlyModuleHandler(
    struct _EXCEPTION_RECORD* exception_record,
    void*  establisher_frame,
    struct _CONTEXT* context,
    void*  reserved) {
  EXCEPTION_POINTERS ptrs = { exception_record, context };

  if (ExceptionBarrierConfig::enabled() &&
      IS_DISPATCHING(exception_record->ExceptionFlags)) {
    CrashHandlerTraits traits;
    traits.Init(0, 0, &WriteMinidumpForException);
    if (traits.IsOurModule(exception_record->ExceptionAddress)) {
      traits.WriteDump(&ptrs);
    }
  }

  return ExceptionContinueSearch;
}

extern "C" EXCEPTION_DISPOSITION __cdecl
ExceptionBarrierCallCustomHandler(struct _EXCEPTION_RECORD* exception_record,
                                  void* establisher_frame,
                                  struct _CONTEXT* context,
                                  void* reserved) {
  if (ExceptionBarrierConfig::enabled() &&
      IS_DISPATCHING(exception_record->ExceptionFlags)) {
    ExceptionBarrierCustomHandler::CustomExceptionHandler handler =
        ExceptionBarrierCustomHandler::custom_handler();
    if (handler) {
      EXCEPTION_POINTERS ptrs = { exception_record, context };
      handler(&ptrs);
    }
  }

  return ExceptionContinueSearch;
}
