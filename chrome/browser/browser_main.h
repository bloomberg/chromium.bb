// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_MAIN_H_
#define CHROME_BROWSER_BROWSER_MAIN_H_

struct MainFunctionParams;
class MetricsService;

// Perform platform-specific work that needs to be done before the main
// message loop is created, initialized, and entered.
void WillInitializeMainMessageLoop(const MainFunctionParams& parameters);

// Perform platform-specific work that needs to be done after the main event
// loop has ended.
void DidEndMainMessageLoop();

// Records the conditions that can prevent Breakpad from generating and
// sending crash reports.  The presence of a Breakpad handler (after
// attempting to initialize crash reporting) and the presence of a debugger
// are registered with the UMA metrics service.
void RecordBreakpadStatusUMA(MetricsService* metrics);

// Displays a warning message if some minimum level of OS support is not
// present on the current platform.
void WarnAboutMinimumSystemRequirements();

#endif  // CHROME_BROWSER_BROWSER_MAIN_H_
