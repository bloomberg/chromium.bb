// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_DETAILS_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_DETAILS_H_

#include <string>

#include "chrome/browser/performance_monitor/metric.h"

namespace performance_monitor {

struct MetricDetails {
  const char* const name;
  const char* const description;
  const char* const units;
  const double tick_size;
};

const MetricDetails* GetMetricDetails(MetricType event_type);

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_DETAILS_H_
