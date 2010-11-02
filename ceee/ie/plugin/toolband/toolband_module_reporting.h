// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A wrapper around common crash reporting code to manage reporting for CEEE
// plugin.

#ifndef CEEE_IE_PLUGIN_TOOLBAND_TOOLBAND_MODULE_REPORTING_H_
#define CEEE_IE_PLUGIN_TOOLBAND_TOOLBAND_MODULE_REPORTING_H_

#include "chrome_frame/crash_reporting/crash_report.h"

extern const wchar_t kSystemPrincipalSid[];

// Intialize crash reporting for the Toolband Plugin. Specific parameters
// here include using the temp directory for dumps, using the system-wide
// install ID, and customized client info.
bool InitializeCrashReporting();

// Shut down crash reporting for CEEE plug-in.
bool ShutdownCrashReporting();

#endif  // CEEE_IE_PLUGIN_TOOLBAND_TOOLBAND_MODULE_REPORTING_H_
