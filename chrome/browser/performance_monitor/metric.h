// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_H_

#include <string>
#include "base/time.h"

namespace performance_monitor {

// IMPORTANT: This is used as an indication of the metric type within the
// performance monitor database; do not change the order! If you add new
// metric types to this list, place them above METRIC_NUMBER_OF_METRICS and add
// the appropriate constants to metric_details.
enum MetricType {
METRIC_CPU_USAGE,
METRIC_PRIVATE_MEMORY_USAGE,
METRIC_SHARED_MEMORY_USAGE,
METRIC_STARTUP_TIME,
METRIC_TEST_STARTUP_TIME,
METRIC_SESSION_RESTORE_TIME,
METRIC_PAGE_LOAD_TIME,
METRIC_NUMBER_OF_METRICS
};

struct Metric {
 public:
  Metric();
  Metric(const base::Time& metric_time, const double metric_value);
  Metric(const std::string& metric_time, const std::string& metric_value);
  ~Metric();

  base::Time time;
  double value;
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_H_
