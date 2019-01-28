// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_HELPER_WIN_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_HELPER_WIN_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/browser/performance_monitor/system_monitor.h"

namespace performance_monitor {
namespace win {

// Windows specific implementation of the SystemMonitorHelper class. Do not use
// this directly, instead access the system metrics via the SystemMonitor
// interface.
class SystemMonitorHelperWin : public SystemMonitorHelper {
 public:
  ~SystemMonitorHelperWin() override = default;

 protected:
  friend class ::performance_monitor::SystemMonitor;
  friend class SystemMonitorHelperWinTest;

  // Protected constructor so this can only be created by SystemMonitor.
  SystemMonitorHelperWin();

  // SystemMonitorHelperWin:
  base::TimeDelta GetRefreshInterval(
      const SystemMonitor::SystemObserver::MetricRefreshFrequencies&
          metrics_and_frequencies) override;
  MetricsRefresh RefreshMetrics(
      const SystemMonitor::SystemObserver::MetricRefreshFrequencies
          metrics_and_frequencies,
      const base::TimeTicks& refresh_time) override;

 private:
  // The last time the free physical memory value has been refreshed at the
  // default frequency.
  base::TimeTicks last_phys_memory_refresh_default_freq_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SystemMonitorHelperWin);
};

}  // namespace win
}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_HELPER_WIN_H_
