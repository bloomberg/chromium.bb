// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class to make it easy to tag exception propagation boundaries and
// get crash reports of exceptions that pass over same.
#ifndef CHROME_FRAME_EXCEPTION_BARRIER_H_
#define CHROME_FRAME_EXCEPTION_BARRIER_H_

#include <windows.h>

/// This is the type dictated for an exception handler by the platform ABI
/// @see _except_handler in excpt.h
typedef EXCEPTION_DISPOSITION (__cdecl *ExceptionHandlerFunc)(
                                struct _EXCEPTION_RECORD *exception_record,
                                void * establisher_frame,
                                struct _CONTEXT *context,
                                void * reserved);

/// The type of an exception record in the exception handler chain
struct EXCEPTION_REGISTRATION {
  EXCEPTION_REGISTRATION *prev;
  ExceptionHandlerFunc  handler;
};

/// This is our raw exception handler, it must be declared extern "C" to
/// match up with the SAFESEH declaration in our corresponding ASM file
extern "C" EXCEPTION_DISPOSITION __cdecl
ExceptionBarrierHandler(struct _EXCEPTION_RECORD *exception_record,
                        void * establisher_frame,
                        struct _CONTEXT *context,
                        void * reserved);

/// An exception barrier is used to report exceptions that pass through
/// a boundary where exceptions shouldn't pass, such as e.g. COM interface
/// boundaries.
/// This is handy for any kind of plugin code, where if the exception passes
/// through unhindered, it'll either be swallowed by an SEH exception handler
/// above us on the stack, or be reported as an unhandled exception for
/// the application hosting the plugin code.
///
/// To use this class, simply instantiate an ExceptionBarrier just inside
/// the code boundary, like this:
/// @code
/// HRESULT SomeObject::SomeCOMMethod(...) {
///   ExceptionBarrier report_crashes;
///
///   ... other code here ...
/// }
/// @endcode
class ExceptionBarrier {
 public:
  /// Register the barrier in the SEH chain
  ExceptionBarrier();

  /// And unregister on destruction
  ~ExceptionBarrier();

  /// Signature of the handler function which gets notified when
  /// an exception propagates through a barrier.
  typedef void (CALLBACK *ExceptionHandler)(EXCEPTION_POINTERS *ptrs);

  /// @name Accessors
  /// @{
  static void set_handler(ExceptionHandler handler) { s_handler_ = handler; }
  static ExceptionHandler handler() { return s_handler_; }
  /// @}

 private:
  /// Our SEH frame
  EXCEPTION_REGISTRATION registration_;

  /// The function that gets invoked if an exception
  /// propagates through a barrier
  static ExceptionHandler s_handler_;
};

/// @name These are implemented in the associated .asm file
/// @{
extern "C" void WINAPI RegisterExceptionRecord(
                          EXCEPTION_REGISTRATION *registration,
                          ExceptionHandlerFunc func);
extern "C" void WINAPI UnregisterExceptionRecord(
                          EXCEPTION_REGISTRATION *registration);
/// @}


inline ExceptionBarrier::ExceptionBarrier() {
  RegisterExceptionRecord(&registration_, ExceptionBarrierHandler);
}

inline ExceptionBarrier::~ExceptionBarrier() {
  UnregisterExceptionRecord(&registration_);
}

#endif  // CHROME_FRAME_EXCEPTION_BARRIER_H_
