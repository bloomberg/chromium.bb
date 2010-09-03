// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

// Assume this implies headless mode and use kChromePipeName if it shows
// up in the command line.
extern const wchar_t kFullMemoryCrashReport[];

extern const MINIDUMP_TYPE kLargerDumpType;

// Returns a pointer to a static instance of a CustomClientInfo structure
// containing Chrome Frame specific data.
google_breakpad::CustomClientInfo* GetCustomInfo();

// Initializes breakpad crash reporting and returns a pointer to a newly
// constructed ExceptionHandler object. It is the responsibility of the caller
// to delete this object which will shut down the crash reporting machinery.
google_breakpad::ExceptionHandler* InitializeCrashReporting(
    const wchar_t* cmd_line);

#endif  // CHROME_FRAME_CRASH_SERVER_INIT_H_
