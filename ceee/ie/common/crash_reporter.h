// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Declaration of common IE crash reporter.

#ifndef CEEE_IE_COMMON_CRASH_REPORTER_H_
#define CEEE_IE_COMMON_CRASH_REPORTER_H_

#include "base/scoped_ptr.h"
#include "breakpad/src/client/windows/handler/exception_handler.h"

// A wrapper around Breakpad's ExceptionHandler class for crash reporting using
// Omaha's crash reporting service.
class CrashReporter {
 public:
  explicit CrashReporter(const wchar_t* component_name);
  virtual ~CrashReporter();

  // Initialize the ExceptionHandler to begin catching and reporting unhandled
  // exceptions.
  //
  // @param full_dump Whether or not to do a full memory dump.
  void InitializeCrashReporting(bool full_dump);

  // Halt crash reporting, stopping the ExceptionHandler from catching any
  // further unhandled exceptions.
  void ShutdownCrashReporting();

  // TODO(stevet@google.com): Provide a way to trigger a dump send,
  // which can be used for debugging.
 private:
  google_breakpad::CustomClientInfo client_info_;
  scoped_array<google_breakpad::CustomInfoEntry> client_info_entries_;

  // Valid after a call to InitializeCrashReporting and invalidated after a call
  // to ShutdownCrashReporting.
  google_breakpad::ExceptionHandler* exception_handler_;
};

#endif  // CEEE_IE_COMMON_CRASH_REPORTER_H_
