// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_H_

#include <string>
#include "base/time.h"

namespace performance_monitor {

// IMPORTANT: To add new metrics, please
// - Place the new metric above METRIC_NUMBER_OF_METRICS.
// - Add a member to the MetricKeyChar enum in key_builder.cc.
// - Add the appropriate messages in generated_resources.grd.
// - Add the appropriate functions in
//   chrome/browser/ui/webui/performance_monitor/performance_monitor_l10n.h.
enum MetricType {
METRIC_UNDEFINED,
METRIC_CPU_USAGE,
METRIC_PRIVATE_MEMORY_USAGE,
METRIC_SHARED_MEMORY_USAGE,
METRIC_STARTUP_TIME,
METRIC_TEST_STARTUP_TIME,
METRIC_SESSION_RESTORE_TIME,
METRIC_PAGE_LOAD_TIME,
METRIC_NETWORK_BYTES_READ,
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
