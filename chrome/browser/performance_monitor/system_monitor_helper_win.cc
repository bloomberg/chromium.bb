// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/system_monitor_helper_win.h"

#include <windows.h>

namespace performance_monitor {
namespace win {

namespace {

// The refresh period of the free physical memory metric when being tracked at
// the regular frequency.
constexpr base::TimeDelta kRefreshIntervalPhysMemoryMbRegFreq =
    base::TimeDelta::FromSeconds(2);

const DWORDLONG kMBBytes = 1024 * 1024;

// Returns the amount of physical memory available on the system.
base::Optional<int> GetFreePhysMemoryMb() {
  MEMORYSTATUSEX mem_status;
  mem_status.dwLength = sizeof(mem_status);
  if (!::GlobalMemoryStatusEx(&mem_status))
    return base::nullopt;

  return (mem_status.ullAvailPhys / kMBBytes);
}

}  // namespace

SystemMonitorHelperWin::SystemMonitorHelperWin() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

base::TimeDelta SystemMonitorHelperWin::GetRefreshInterval(
    const SystemMonitor::SystemObserver::MetricRefreshFrequencies&
        metrics_and_frequencies) {
  if (metrics_and_frequencies.free_phys_memory_mb_frequency ==
      SystemMonitor::SamplingFrequency::kDefaultFrequency) {
    return kRefreshIntervalPhysMemoryMbRegFreq;
  }
  return base::TimeDelta::Max();
}

SystemMonitorHelperWin::MetricsRefresh SystemMonitorHelperWin::RefreshMetrics(
    const SystemMonitor::SystemObserver::MetricRefreshFrequencies
        metrics_and_frequencies,
    const base::TimeTicks& refresh_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SystemMonitorHelper::MetricsRefresh metrics;

  if ((refresh_time - last_phys_memory_refresh_default_freq_) >=
      kRefreshIntervalPhysMemoryMbRegFreq) {
    last_phys_memory_refresh_default_freq_ = refresh_time;
    auto free_phys_memory = GetFreePhysMemoryMb();
    if (free_phys_memory) {
      metrics.free_phys_memory_mb =
          SystemMonitorHelper::MetricAndRefreshReason<int>(
              free_phys_memory.value(),
              SystemMonitor::SamplingFrequency::kDefaultFrequency);
    }
  }

  return metrics;
}

}  // namespace win
}  // namespace performance_monitor
