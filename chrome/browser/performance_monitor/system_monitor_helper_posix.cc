// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/system_monitor_helper_posix.h"

namespace performance_monitor {

base::TimeDelta SystemMonitorHelperPosix::GetRefreshInterval(
    const SystemMonitor::SystemObserver::MetricRefreshFrequencies&
        metrics_and_frequencies) {
  NOTIMPLEMENTED();
  return base::TimeDelta::Max();
}

SystemMonitorHelper::MetricsRefresh SystemMonitorHelperPosix::RefreshMetrics(
    const SystemMonitor::SystemObserver::MetricRefreshFrequencies
        metrics_and_frequencies,
    const base::TimeTicks& refresh_time) {
  NOTIMPLEMENTED();
  return {};
}

}  // namespace performance_monitor
