// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_DETAILS_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_DETAILS_H_

#include <string>

namespace performance_monitor {

// Keep this enum synced with kMetricDetailsList in the cc file.
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

struct MetricDetails {
  const char* const name;
  const char* const description;
  const char* const units;
  const double tick_size;
};

const MetricDetails* GetMetricDetails(MetricType event_type);

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_DETAILS_H_
