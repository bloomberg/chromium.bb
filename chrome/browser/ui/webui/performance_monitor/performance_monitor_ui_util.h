// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_UI_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_UI_UTIL_H_

#include "chrome/browser/performance_monitor/database.h"
#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui_constants.h"

namespace performance_monitor {

// Return the factor by which all metrics should be multiplied in order to be in
// the preferred unit (e.g., memory usage is in bytes, but we display it in
// megabytes, so we return 1/1024^2).
double GetConversionFactor(UnitDetails from, UnitDetails to);

// Metric data can be either dense or sporadic, so AggregateMetric() normalizes
// the metric data in time. |metrics| must be sorted in increasing time.
// AggregateMetrics() will perform the aggregation according to the |strategy|
// provided.
//
// Median: Use the median of sample values from the window, ignoring
//         irregularity in sample timings.
// Mean: Use the mean value within the window.
// None: Return all samples from the window.
//
// In the methods which do aggregation, sampling windows start and end at an
// integer multiple of |resolution| away from |start| and data points are
// omitted if there are no points to resample. The time associated with each
// slice is the time at the end of the slice.
//
// Returns a pointer to a new MetricVector, or NULL if |metrics| is empty.
scoped_ptr<Database::MetricVector> AggregateMetric(
    MetricType type,
    const Database::MetricVector* metrics,
    const base::Time& start,
    const base::TimeDelta& resolution,
    AggregationMethod method);

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_UI_UTIL_H_
