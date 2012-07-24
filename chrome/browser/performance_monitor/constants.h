// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_

namespace performance_monitor {

// TODO(chebert): i18n on all constants.
extern const char kMetricNotFoundError[];

// Any metric that is not associated with a specific activity will use this as
// its activity.
extern const char kProcessChromeAggregate[];

// Metrics keys for statistics gathering.
extern const char kMetricCPUUsageName[];
extern const char kMetricCPUUsageDescription[];
extern const char kMetricCPUUsageUnits[];
extern const double kMetricCPUUsageTickSize;

extern const char kMetricPrivateMemoryUsageName[];
extern const char kMetricPrivateMemoryUsageDescription[];
extern const char kMetricPrivateMemoryUsageUnits[];
extern const double kMetricPrivateMemoryUsageTickSize;

extern const char kStateChromeVersion[];

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_CONSTANTS_H_
