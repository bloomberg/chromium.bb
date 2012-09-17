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
// Put concisely, AggregateMetric() does sample rate conversion from irregular
// metric data points to a sample period of |resolution| beginning at |start|.
// Each sampling window starts and ends at an integer multiple away from
// |start| and data points are omitted if there are no points to resample.
// Returns a pointer to a new MetricVector, or NULL if |metrics| is empty.
scoped_ptr<Database::MetricVector> AggregateMetric(
    const Database::MetricVector* metrics,
    const base::Time& start,
    const base::TimeDelta& resolution);

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_UI_UTIL_H_
