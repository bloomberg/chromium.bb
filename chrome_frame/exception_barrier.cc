// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to make it easy to tag exception propagation boundaries and
// get crash reports of exceptions that pass over same.
#include "chrome_frame/exception_barrier.h"

enum {
  // Flag set by exception handling machinery when unwinding
  EH_UNWINDING = 0x00000002
};

ExceptionBarrier::ExceptionHandler ExceptionBarrier::s_handler_ = NULL;

// This function must be extern "C" to match up with the SAFESEH
// declaration in our corresponding ASM file
extern "C" EXCEPTION_DISPOSITION __cdecl
ExceptionBarrierHandler(struct _EXCEPTION_RECORD* exception_record,
                        void* establisher_frame,
                        struct _CONTEXT* context,
                        void* reserved) {
  establisher_frame;  // unreferenced formal parameter
  reserved;
  if (!(exception_record->ExceptionFlags & EH_UNWINDING)) {
    // When the exception is really propagating through us, we'd like to be
    // called before the state of the program has been modified by the stack
    // unwinding. In the absence of an exception handler, the unhandled
    // exception filter gets called between the first chance and the second
    // chance exceptions, so Windows pops either the JIT debugger or WER UI.
    // This is not desirable in most of the cases.
    ExceptionBarrier::ExceptionHandler handler = ExceptionBarrier::handler();
    if (handler) {
      EXCEPTION_POINTERS ptrs = { exception_record, context };

      handler(&ptrs);
    }
  }

  return ExceptionContinueSearch;
}
