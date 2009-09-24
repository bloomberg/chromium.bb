// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// crash_report.h : Declarations for crash reporting.

#ifndef CHROME_FRAME_CRASH_REPORT_H_
#define CHROME_FRAME_CRASH_REPORT_H_

bool InitializeCrashReporting(bool use_crash_service, bool full_dump);
bool ShutdownCrashReporting();

#endif  // CHROME_FRAME_CRASH_REPORT_H_
