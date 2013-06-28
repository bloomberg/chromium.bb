// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_H_

#include <string>
#include "base/time/time.h"

namespace performance_monitor {

// IMPORTANT: To add new metrics, please
// - Place the new metric above METRIC_NUMBER_OF_METRICS.
// - Add a member to the MetricKeyChar enum in key_builder.cc.
// - Add the appropriate messages in generated_resources.grd.
// - Add the appropriate functions in
//   chrome/browser/ui/webui/performance_monitor/performance_monitor_l10n.h.
enum MetricType {
  METRIC_UNDEFINED,

  // CPU and memory usage are combined for all Chrome-related processes.
  METRIC_CPU_USAGE,
  METRIC_PRIVATE_MEMORY_USAGE,
  METRIC_SHARED_MEMORY_USAGE,

  // Timing measurements; these are all independent metrics (e.g., session
  // restore time is independent of startup time, even though they may happen
  // in sequence). Test startup time refers to any non-normal startup time, e.g.
  // when we run test suites.
  METRIC_STARTUP_TIME,
  METRIC_TEST_STARTUP_TIME,
  METRIC_SESSION_RESTORE_TIME,
  METRIC_PAGE_LOAD_TIME,

  // Total number of bytes read since PerformanceMonitor first started running.
  METRIC_NETWORK_BYTES_READ,

  METRIC_NUMBER_OF_METRICS
};

struct Metric {
 public:
  Metric();
  Metric(MetricType metric_type,
         const base::Time& metric_time,
         const double metric_value);
  Metric(MetricType metric_type,
         const std::string& metric_time,
         const std::string& metric_value);
  ~Metric();

  // Check the value in the metric to make sure that it is reasonable. Since
  // some metric-gathering methods will fail and return incorrect values, we
  // need to try to weed these out as best we can.
  bool IsValid() const;

  // This converts the double stored in value to a string format. This will
  // not perform any checking on the validity of the metric, and only makes
  // sense if the metric IsValid().
  std::string ValueAsString() const;

  MetricType type;
  base::Time time;
  double value;
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_H_
