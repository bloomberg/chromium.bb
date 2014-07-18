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

// Call this as early as possible in the startup process to record a
// timestamp.
void RecordMainEntryPointTime();

// Call this when the executable is loaded and main() is entered. Can be
// different from |RecordMainEntryPointTime| when the startup process is
// contained in a separate dll, such as with chrome.exe / chrome.dll on Windows.
void RecordExeMainEntryTime();

#if defined(OS_ANDROID)
// On Android the entry point time is the time at which the Java code starts.
// This is recorded on the Java side, and then passed to the C++ side once the
// C++ library is loaded and running.
void RecordSavedMainEntryPointTime(const base::Time& entry_point_time);
#endif // OS_ANDROID

// Called just before the message loop is about to start. Entry point to record
// startup stats.
// |is_first_run| - is the current launch part of a first run.
void OnBrowserStartupComplete(bool is_first_run);

// Called when the initial page load has finished in order to record startup
// stats.
void OnInitialPageLoadComplete();

// Returns the time of main entry recorded from RecordMainEntryPointTime.
// Returns NULL if that method has not yet been called.
// This method is expected to be called from the UI thread.
const base::Time* MainEntryPointTime();

// Scoper that records the time period before it's destructed in a histogram
// with the given name. The histogram is only recorded for slow chrome startups.
// Useful for trying to figure out what parts of Chrome cause slow startup.
class ScopedSlowStartupUMA {
 public:
  explicit ScopedSlowStartupUMA(const std::string& histogram_name)
      : start_time_(base::TimeTicks::Now()),
        histogram_name_(histogram_name) {}

  ~ScopedSlowStartupUMA();

 private:
  const base::TimeTicks start_time_;
  const std::string histogram_name_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSlowStartupUMA);
};

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_STARTUP_METRIC_UTILS_H_
