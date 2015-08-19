// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STARTUP_METRIC_UTILS_STARTUP_METRIC_UTILS_H_
#define COMPONENTS_STARTUP_METRIC_UTILS_STARTUP_METRIC_UTILS_H_

#include <string>

#include "base/time/time.h"

// Utility functions to support metric collection for browser startup.

namespace startup_metric_utils {

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

// Returns the time of main entry recorded from RecordMainEntryPointTime.
// Returns NULL if that method has not yet been called.
// This method is expected to be called from the UI thread.
const base::Time* MainEntryPointTime();

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_STARTUP_METRIC_UTILS_H_
