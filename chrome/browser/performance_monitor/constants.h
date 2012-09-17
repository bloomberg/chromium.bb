// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_

#include "base/basictypes.h"

namespace performance_monitor {

// Constants which are used by the PerformanceMonitor and its related classes.
// The constants should be documented alongside the definition of their values
// in the .cc file.

extern const char kMetricNotFoundError[];
extern const char kProcessChromeAggregate[];
extern const int kDefaultGatherIntervalInSeconds;

// State tokens
extern const char kStateChromeVersion[];
extern const char kStateProfilePrefix[];

// Unit values (for use in metric, and on the UI side).

// Memory measurements
extern const int64 kBytesPerKilobyte;
extern const int64 kBytesPerMegabyte;
extern const int64 kBytesPerGigabyte;
extern const int64 kBytesPerTerabyte;

// Time measurements - Most of these are imported from base/time.h
// These units are used for display (and it's related calculations), not for
// any mathematical analysis. Thus we can estimate for values without an exact
// conversion.
extern const int64 kMicrosecondsPerMonth;
extern const int64 kMicrosecondsPerYear;

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_
