// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_

namespace performance_monitor {

// Constants which are used by the PerformanceMonitor and its related classes.
// The constants should be documented alongside the definition of their values
// in the .cc file.

extern const char kMetricNotFoundError[];
extern const char kProcessChromeAggregate[];
extern const int kGatherIntervalInMinutes;

// State tokens
extern const char kStateChromeVersion[];
extern const char kStateProfilePrefix[];

// Metric details
extern const char kMetricCPUUsageName[];
extern const char kMetricCPUUsageDescription[];
extern const char kMetricCPUUsageUnits[];
extern const double kMetricCPUUsageTickSize;

extern const char kMetricPrivateMemoryUsageName[];
extern const char kMetricPrivateMemoryUsageDescription[];
extern const char kMetricPrivateMemoryUsageUnits[];
extern const double kMetricPrivateMemoryUsageTickSize;

extern const char kMetricSharedMemoryUsageName[];
extern const char kMetricSharedMemoryUsageDescription[];
extern const char kMetricSharedMemoryUsageUnits[];
extern const double kMetricSharedMemoryUsageTickSize;

extern const char kMetricStartupTimeName[];
extern const char kMetricStartupTimeDescription[];
extern const char kMetricStartupTimeUnits[];
extern const double kMetricStartupTimeTickSize;

extern const char kMetricTestStartupTimeName[];
extern const char kMetricTestStartupTimeDescription[];
extern const char kMetricTestStartupTimeUnits[];
extern const double kMetricTestStartupTimeTickSize;

extern const char kMetricSessionRestoreTimeName[];
extern const char kMetricSessionRestoreTimeDescription[];
extern const char kMetricSessionRestoreTimeUnits[];
extern const double kMetricSessionRestoreTimeTickSize;

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_
