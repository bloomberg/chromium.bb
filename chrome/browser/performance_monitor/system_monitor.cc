// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/system_monitor.h"

#include <algorithm>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "chrome/browser/performance_monitor/system_monitor_helper_win.h"
#elif defined(OS_POSIX)
#include "chrome/browser/performance_monitor/system_monitor_helper_posix.h"
#endif

namespace performance_monitor {

namespace {

using MetricRefreshFrequencies =
    SystemMonitor::SystemObserver::MetricRefreshFrequencies;

// The global instance.
SystemMonitor* g_system_metrics_monitor = nullptr;

}  // namespace

SystemMonitor::SystemMonitor(std::unique_ptr<SystemMonitorHelper> helper)
    : blocking_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      async_helper_(helper.release(),
                    base::OnTaskRunnerDeleter(blocking_task_runner_)),
      weak_factory_(this) {
  DCHECK(!g_system_metrics_monitor);
  g_system_metrics_monitor = this;
}

SystemMonitor::~SystemMonitor() {
  DCHECK_EQ(this, g_system_metrics_monitor);
  g_system_metrics_monitor = nullptr;
}

// static
std::unique_ptr<SystemMonitor> SystemMonitor::Create() {
  DCHECK(!g_system_metrics_monitor);
#if defined(OS_WIN)
  SystemMonitor* monitor =
      new SystemMonitor(base::WrapUnique(new win::SystemMonitorHelperWin()));
#elif defined(OS_POSIX)
  SystemMonitor* monitor =
      new SystemMonitor(base::WrapUnique(new SystemMonitorHelperPosix()));
#else
#error Unsupported platform
#endif
  return base::WrapUnique(monitor);
}

// static
SystemMonitor* SystemMonitor::Get() {
  return g_system_metrics_monitor;
}

void SystemMonitor::SystemObserver::OnFreePhysicalMemoryMbSample(
    int free_phys_memory_mb) {
  NOTREACHED();
}

void SystemMonitor::AddOrUpdateObserver(
    SystemMonitor::SystemObserver* observer,
    MetricRefreshFrequencies metrics_and_frequencies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
  observer_metrics_[observer] = std::move(metrics_and_frequencies);
  UpdateObservedMetricsSet();
}

void SystemMonitor::RemoveObserver(SystemMonitor::SystemObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
  observer_metrics_.erase(observer);
  UpdateObservedMetricsSet();
}

SystemMonitor::MetricsRefresh::MetricsRefresh() = default;
SystemMonitor::MetricsRefresh::MetricsRefresh(MetricsRefresh&&) = default;

void SystemMonitor::UpdateObservedMetricsSet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Reset the current metric frequencies.
  metrics_refresh_frequencies_ = MetricRefreshFrequencies();

  // Iterates over the |observer_metrics_| list to find the highest refresh
  // frequency for each metric.
  for (const auto& iter : observer_metrics_) {
    metrics_refresh_frequencies_.free_phys_memory_mb_frequency =
        std::max(metrics_refresh_frequencies_.free_phys_memory_mb_frequency,
                 iter.second.free_phys_memory_mb_frequency);
  }

  // Check if there's still some metrics being tracked.
  bool should_refresh_metrics =
      (metrics_refresh_frequencies_.free_phys_memory_mb_frequency >
       SamplingFrequency::kNoSampling);

  if (should_refresh_metrics) {
    base::TimeDelta refresh_frequency =
        async_helper_->GetRefreshInterval(metrics_refresh_frequencies_);
    if (!refresh_timer_.IsRunning() ||
        refresh_frequency != refresh_timer_.GetCurrentDelay()) {
      refresh_timer_.Start(FROM_HERE, refresh_frequency,
                           base::BindRepeating(&SystemMonitor::RefreshCallback,
                                               base::Unretained(this)));
    }
    // Sanity check.
    DCHECK_EQ(refresh_timer_.GetCurrentDelay(), refresh_frequency);
  } else {
    refresh_timer_.Stop();
  }

  // Start or stop the timer if needed.
  if (refresh_timer_.IsRunning() && !should_refresh_metrics) {
    refresh_timer_.Stop();
  } else if (should_refresh_metrics) {
    base::TimeDelta refresh_frequency =
        async_helper_->GetRefreshInterval(metrics_refresh_frequencies_);
    // Stop the timer if the refresh frequency has changed.
    if (refresh_timer_.IsRunning() &&
        refresh_frequency != refresh_timer_.GetCurrentDelay()) {
      refresh_timer_.Stop();
    }
    if (!refresh_timer_.IsRunning()) {
      refresh_timer_.Start(FROM_HERE, refresh_frequency,
                           base::BindRepeating(&SystemMonitor::RefreshCallback,
                                               base::Unretained(this)));
    }
  }
}

void SystemMonitor::RefreshCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&SystemMonitorHelper::RefreshMetrics,
                     base::Unretained(async_helper_.get()),
                     metrics_refresh_frequencies_, base::TimeTicks::Now()),
      base::BindOnce(&SystemMonitor::NotifyObservers,
                     weak_factory_.GetWeakPtr()));

  refresh_timer_.Start(FROM_HERE, refresh_timer_.GetCurrentDelay(),
                       base::BindRepeating(&SystemMonitor::RefreshCallback,
                                           base::Unretained(this)));
}

void SystemMonitor::NotifyObservers(const MetricsRefresh& metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(SamplingFrequency::kNoSampling,
            metrics.free_phys_memory_mb.refresh_reason);

  // Iterate over the observers and notify them if a metric has been refreshed
  // at the requested frequency.
  for (auto& observer : observers_) {
    const auto& iter = observer_metrics_.find(&observer);
    DCHECK(iter != observer_metrics_.end());
    if (metrics.free_phys_memory_mb.refresh_reason <=
        iter->second.free_phys_memory_mb_frequency) {
      iter->first->OnFreePhysicalMemoryMbSample(
          metrics.free_phys_memory_mb.metric_value);
    }
  }
}

}  // namespace performance_monitor
