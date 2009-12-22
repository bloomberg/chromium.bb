// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_MAIN_H_
#define CHROME_BROWSER_BROWSER_MAIN_H_

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <vector>
class GURL;
#endif

struct MainFunctionParams;
class MetricsService;

namespace Platform {

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

#if defined(OS_MACOSX)
// On Mac, files/URLs to open at startup aren't typically provided on the
// command line, but rather by Launch Services via an Apple Event. We spin a UI
// message loop early to catch this event in the app controller and store the
// URLs.

// Has the app controller been told that it is done? (I.e., has it received
// |-applicationDidFinishLaunching:|?)
bool IsStartupComplete();

// The following functions retrieves the URLs and clears the list of URLs,
// respectively.
const std::vector<GURL>& GetStartupURLs();
void ClearStartupURLs();

#endif

}  // namespace Platform

#endif  // CHROME_BROWSER_BROWSER_MAIN_H_
