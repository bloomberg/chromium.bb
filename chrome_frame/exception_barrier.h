// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to make it easy to tag exception propagation boundaries and
// get crash reports of exceptions that pass over same.
//
// An exception barrier is used to report exceptions that pass through
// a boundary where exceptions shouldn't pass, such as e.g. COM interface
// boundaries.
// This is handy for any kind of plugin code, where if the exception passes
// through unhindered, it'll either be swallowed by an SEH exception handler
// above us on the stack, or be reported as an unhandled exception for
// the application hosting the plugin code.
//
// IMPORTANT NOTE: This class has crash_reporting disabled by default. To
// enable crash reporting call:
//
// @code
// ExceptionBarrierBase::set_crash_handling(true)
// @endcode
//
// somewhere in your initialization code.
//
// Then, to use this class, simply instantiate an ExceptionBarrier just inside
// the code boundary, like this:
// @code
// HRESULT SomeObject::SomeCOMMethod(...) {
//   ExceptionBarrier report_crashes;
//
//   ... other code here ...
// }
// @endcode
//
// There are three ExceptionBarrier types defined here:
//  1) ExceptionBarrier which reports all crashes it sees.
//  2) ExceptionBarrierReportOnlyModule which reports only crashes occurring
//     in this module.
//  3) ExceptionBarrierCustomHandler which calls the handler set by a call
//     to ExceptionBarrierCallCustomHandler::set_custom_handler(). Note that
//     there is one custom handler for all ExceptionBarrierCallCustomHandler
//     instances. If set_custom_handler() is never called, this places an
//     SEH in the chain that just returns ExceptionContinueSearch.

#ifndef CHROME_FRAME_EXCEPTION_BARRIER_H_
#define CHROME_FRAME_EXCEPTION_BARRIER_H_

#include <windows.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;

// This is the type dictated for an exception handler by the platform ABI
// @see _except_handler in excpt.h
typedef EXCEPTION_DISPOSITION (__cdecl* ExceptionHandlerFunc)(
                                struct _EXCEPTION_RECORD* exception_record,
                                void*  establisher_frame,
                                struct _CONTEXT* context,
                                void*  reserved);

// The type of an exception record in the exception handler chain
struct EXCEPTION_REGISTRATION {
  EXCEPTION_REGISTRATION* prev;
  ExceptionHandlerFunc  handler;
};

// This is our raw exception handler, it must be declared extern "C" to
// match up with the SAFESEH declaration in our corresponding ASM file.
extern "C" EXCEPTION_DISPOSITION __cdecl
ExceptionBarrierHandler(struct _EXCEPTION_RECORD* exception_record,
                        void* establisher_frame,
                        struct _CONTEXT* context,
                        void* reserved);

// An alternate raw exception handler that reports crashes only for the current
// module. It must be declared extern "C" to match up with the SAFESEH
// declaration in our corresponding ASM file.
extern "C" EXCEPTION_DISPOSITION __cdecl
ExceptionBarrierReportOnlyModuleHandler(
    struct _EXCEPTION_RECORD* exception_record,
    void* establisher_frame,
    struct _CONTEXT* context,
    void* reserved);

// An alternate raw exception handler that calls out to a custom handler.
// It must be declared extern "C" to match up with the SAFESEH declaration in
// our corresponding ASM file.
extern "C" EXCEPTION_DISPOSITION __cdecl
ExceptionBarrierCallCustomHandler(
    struct _EXCEPTION_RECORD* exception_record,
    void* establisher_frame,
    struct _CONTEXT* context,
    void* reserved);


// @name These are implemented in the associated .asm file
// @{
extern "C" void WINAPI RegisterExceptionRecord(
                          EXCEPTION_REGISTRATION* registration,
                          ExceptionHandlerFunc func);
extern "C" void WINAPI UnregisterExceptionRecord(
                          EXCEPTION_REGISTRATION* registration);
// @}


// Traits classes for ExceptionBarrierT.
class EBTraitsBase {
 public:
  static void UnregisterException(EXCEPTION_REGISTRATION* registration) {
    UnregisterExceptionRecord(registration);
  }
};

class EBReportAllTraits : public EBTraitsBase {
 public:
  static void RegisterException(EXCEPTION_REGISTRATION* registration) {
    RegisterExceptionRecord(registration, ExceptionBarrierHandler);
  }
};

class EBReportOnlyThisModuleTraits : public EBTraitsBase {
 public:
  static void RegisterException(EXCEPTION_REGISTRATION* registration) {
    RegisterExceptionRecord(registration,
                            ExceptionBarrierReportOnlyModuleHandler);
  }
};

class EBCustomHandlerTraits : public EBTraitsBase {
 public:
  static void RegisterException(EXCEPTION_REGISTRATION* registration) {
    RegisterExceptionRecord(registration,
                            ExceptionBarrierCallCustomHandler);
  }
};

class ExceptionBarrierConfig {
 public:
  // Used to globally enable or disable crash handling by ExceptionBarrierBase
  // instances.
  static void set_enabled(bool enabled) {
    s_enabled_ = enabled;
  }
  static bool enabled() { return s_enabled_; }

  // Whether crash reports are enabled.
  static bool s_enabled_;
};

template <typename RegistrarTraits>
class ExceptionBarrierT {
 public:
  // Register the barrier in the SEH chain
  ExceptionBarrierT() {
    RegistrarTraits::RegisterException(&registration_);
  }

  // Unregister on destruction
  virtual ~ExceptionBarrierT() {
    RegistrarTraits::UnregisterException(&registration_);
  }

 protected:
  // Our SEH frame
  EXCEPTION_REGISTRATION registration_;
};

// This class allows for setting a custom exception handler function. The
// handler is shared among all instances. This class is intended to enable
// testing of the SEH registration.
template <typename RegistrarTraits>
class ExceptionBarrierCustomHandlerT :
    public ExceptionBarrierT<typename RegistrarTraits> {
 public:
  // Signature of the handler function which gets notified when
  // an exception propagates through a barrier.
  typedef void (CALLBACK* CustomExceptionHandler)(EXCEPTION_POINTERS* ptrs);

  // Used to set a global custom handler used by all
  // ExceptionBarrierCustomHandler instances.
  static void set_custom_handler(CustomExceptionHandler handler) {
    s_custom_handler_ = handler;
  }
  static CustomExceptionHandler custom_handler() { return s_custom_handler_; }

 private:
  static CustomExceptionHandler s_custom_handler_;
};

// Convenience typedefs for the ExceptionBarrierT specializations.
typedef ExceptionBarrierT<EBReportAllTraits> ExceptionBarrier;
typedef ExceptionBarrierT<EBReportOnlyThisModuleTraits>
    ExceptionBarrierReportOnlyModule;
typedef ExceptionBarrierCustomHandlerT<EBCustomHandlerTraits>
    ExceptionBarrierCustomHandler;

#endif  // CHROME_FRAME_EXCEPTION_BARRIER_H_
