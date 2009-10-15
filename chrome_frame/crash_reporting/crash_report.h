// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// crash_report.h : Declarations for crash reporting.

#ifndef CHROME_FRAME_CRASH_REPORTING_CRASH_REPORT_H_
#define CHROME_FRAME_CRASH_REPORTING_CRASH_REPORT_H_

#include <string>

#include "breakpad/src/client/windows/handler/exception_handler.h"

bool InitializeVectoredCrashReporting(
    bool full_dump,
    const wchar_t* user_sid,
    const std::wstring& dump_path,
    google_breakpad::CustomClientInfo* client_info);
bool ShutdownVectoredCrashReporting();

#endif  // CHROME_FRAME_CRASH_REPORTING_CRASH_REPORT_H_
