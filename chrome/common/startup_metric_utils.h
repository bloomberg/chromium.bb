// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_STARTUP_METRIC_UTILS_H_
#define CHROME_COMMON_STARTUP_METRIC_UTILS_H_

#include "base/time.h"

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

// Return the time recorded by RecordMainEntryPointTime().
const base::Time MainEntryStartTime();

}  // namespace startup_metric_utils

#endif  // CHROME_COMMON_STARTUP_METRIC_UTILS_H_
