// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STARTUP_METRIC_UTILS_STARTUP_METRIC_UTILS_H_
#define COMPONENTS_STARTUP_METRIC_UTILS_STARTUP_METRIC_UTILS_H_

#include <string>

#include "base/time/time.h"

// Utility functions to support metric collection for browser startup.

namespace startup_metric_utils {

// An enumeration of startup temperatures. This must be kept in sync with the
// UMA StartupType enumeration defined in histograms.xml.
enum StartupTemperature {
  // The startup was a cold start: nearly all of the Chrome binaries and
  // resources were brought into memory using hard faults.
  COLD_STARTUP_TEMPERATURE = 0,
  // The startup was a warm start: the Chrome binaries and resources were
  // mostly already resident in memory and effectively no hard faults were
  // observed.
  WARM_STARTUP_TEMPERATURE = 1,
  // The startup type couldn't quite be classified as warm of cold, but rather
  // was somewhere in between.
  UNCERTAIN_STARTUP_TEMPERATURE = 2,
  // This must be last.
  STARTUP_TEMPERATURE_MAX
};

// Returns true if any UI other than the browser window has been displayed
// so far.  Useful to test if UI has been displayed before the first browser
// window was shown, which would invalidate any surrounding timing metrics.
bool WasNonBrowserUIDisplayed();

// Call this when displaying UI that might potentially delay the appearance
// of the initial browser window on Chrome startup.
//
// Note on usage: This function is idempotent and its overhead is low enough
// in comparison with UI display that it's OK to call it on every
// UI invocation regardless of whether the browser window has already
// been displayed or not.
void SetNonBrowserUIDisplayed();

// Call this with the creation time of the startup (initial/main) process.
void RecordStartupProcessCreationTime(const base::Time& time);

// Call this with a time recorded as early as possible in the startup process.
// On Android, the entry point time is the time at which the Java code starts.
// In Mojo, the entry point time is the time at which the shell starts.
void RecordMainEntryPointTime(const base::Time& time);

// Call this with the time when the executable is loaded and main() is entered.
// Can be different from |RecordMainEntryPointTime| when the startup process is
// contained in a separate dll, such as with chrome.exe / chrome.dll on Windows.
void RecordExeMainEntryPointTime(const base::Time& time);

// Call this with the time recorded just before the message loop is started.
// |is_first_run| - is the current launch part of a first run.
void RecordBrowserMainMessageLoopStart(const base::Time& time,
                                       bool is_first_run);

// Call this with the time when the first browser window became visible.
void RecordBrowserWindowDisplay(const base::Time& time);

// Call this with the time delta that the browser spent opening its tabs.
void RecordBrowserOpenTabsDelta(const base::TimeDelta& delta);

// Call this with the time when the first web contents loaded its main frame.
void RecordFirstWebContentsMainFrameLoad(const base::Time& time);

// Call this with the time when the first web contents had a non-empty paint.
void RecordFirstWebContentsNonEmptyPaint(const base::Time& time);

// Returns the time of main entry recorded from RecordMainEntryPointTime.
// Returns a null Time if a value has not been recorded yet.
// This method is expected to be called from the UI thread.
base::Time MainEntryPointTime();

// Returns the startup type. This is only currently supported on the Windows
// platform and will simply return UNCERTAIN_STARTUP_TYPE on other platforms.
// This is only valid after a call to RecordBrowserMainMessageLoopStart().
StartupTemperature GetStartupTemperature();

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_STARTUP_METRIC_UTILS_H_
