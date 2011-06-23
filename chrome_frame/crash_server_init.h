// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CRASH_SERVER_INIT_H_
#define CHROME_FRAME_CRASH_SERVER_INIT_H_

#include "breakpad/src/client/windows/handler/exception_handler.h"

// Possible names for Pipes:
// Headless (testing) mode: "NamedPipe\ChromeCrashServices"
// System-wide install: "NamedPipe\GoogleCrashServices\S-1-5-18"
// Per-user install: "NamedPipe\GoogleCrashServices\<user SID>"
extern const wchar_t kChromePipeName[];
extern const wchar_t kGoogleUpdatePipeName[];
extern const wchar_t kSystemPrincipalSid[];

extern const MINIDUMP_TYPE kLargerDumpType;

enum CrashReportingMode {
  HEADLESS,  // Used for testing, uses crash_service.exe for dumps.
  NORMAL     // Regular mode, uses GoogleCrashService.exe for dumps.
};

// Returns a pointer to a static instance of a CustomClientInfo structure
// containing Chrome Frame specific data.
google_breakpad::CustomClientInfo* GetCustomInfo();

// Initializes breakpad crash reporting and returns a pointer to a newly
// constructed ExceptionHandler object. It is the responsibility of the caller
// to delete this object which will shut down the crash reporting machinery.
google_breakpad::ExceptionHandler* InitializeCrashReporting(
    CrashReportingMode mode);

#endif  // CHROME_FRAME_CRASH_SERVER_INIT_H_
