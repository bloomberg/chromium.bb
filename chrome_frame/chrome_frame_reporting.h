// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A wrapper around common crash reporting code to manage reporting for Chrome
// Frame.

#ifndef CHROME_FRAME_CHROME_FRAME_REPORTING_H_
#define CHROME_FRAME_CHROME_FRAME_REPORTING_H_

#include "chrome_frame/crash_reporting/crash_report.h"

extern const wchar_t kSystemPrincipalSid[];

// Intialize crash reporting for Chrome Frame. Specific parameters here include
// using the temp directory for dumps, determining if the install is system
// wide or user specific, and customized client info.
bool InitializeCrashReporting();

// Shut down crash reporting for Chrome Frame.
bool ShutdownCrashReporting();

#endif  // CHROME_FRAME_CHROME_FRAME_REPORTING_H_
