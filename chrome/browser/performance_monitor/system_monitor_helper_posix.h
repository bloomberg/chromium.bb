// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_HELPER_POSIX_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_HELPER_POSIX_H_

#include "chrome/browser/performance_monitor/system_monitor.h"

namespace performance_monitor {

// Posix implementation of the SystemMonitorHelper class. Do not use
// this directly, instead access the system metrics via the SystemMonitor
// interface.
class SystemMonitorHelperPosix : public SystemMonitorHelper {
 public:
  ~SystemMonitorHelperPosix() override = default;

 protected:
  friend class ::performance_monitor::SystemMonitor;

  // Protected constructor so this can only be created by SystemMonitor.
  SystemMonitorHelperPosix() = default;

  // SystemMonitorHelper:
  base::TimeDelta GetRefreshInterval(
      const SystemMonitor::SystemObserver::MetricRefreshFrequencies&
          metrics_and_frequencies) override;
  MetricsRefresh RefreshMetrics(
      const SystemMonitor::SystemObserver::MetricRefreshFrequencies
          metrics_and_frequencies,
      const base::TimeTicks& refresh_time) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemMonitorHelperPosix);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_HELPER_POSIX_H_
