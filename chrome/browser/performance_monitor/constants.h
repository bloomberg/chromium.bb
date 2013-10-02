// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_

#include "base/basictypes.h"
#include "base/time/time.h"

namespace performance_monitor {

// Constants which are used by the PerformanceMonitor and its related classes.
// The constants should be documented alongside the definition of their values
// in the .cc file.

extern const char kMetricNotFoundError[];
extern const char kProcessChromeAggregate[];

// State tokens
extern const char kStateChromeVersion[];
extern const char kStateProfilePrefix[];

// The interval the watched processes are sampled for performance metrics.
const int kSampleIntervalInSeconds = 10;
// The default interval at which PerformanceMonitor performs its timed
// collections; this can be overridden by using the kPerformanceMonitorGathering
// switch with an associated (positive integer) value.
const int kDefaultGatherIntervalInSeconds = 120;

// Unit values (for use in metric, and on the UI side).

// Memory measurements
const int64 kBytesPerKilobyte = 1 << 10;
const int64 kBytesPerMegabyte = kBytesPerKilobyte * (1 << 10);
const int64 kBytesPerGigabyte = kBytesPerMegabyte * (1 << 10);
const int64 kBytesPerTerabyte = kBytesPerGigabyte * (1 << 10);

// Time measurements - Most of these are imported from base/time/time.h
// These units are used for display (and it's related calculations), not for
// any mathematical analysis. Thus we can estimate for values without an exact
// conversion.
const int64 kMicrosecondsPerMonth = base::Time::kMicrosecondsPerDay * 30;
const int64 kMicrosecondsPerYear = base::Time::kMicrosecondsPerDay * 365;

// Performance alert thresholds

// If a process is consistently above this CPU utilization percentage over time,
// we consider it as high and may take action.
const float kHighCPUUtilizationThreshold = 90.0f;
}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_
