// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace performance_monitor {

class SystemMonitorHelper;

// Monitors various various system metrics such as free memory, disk idle time,
// etc.
//
// Must be created and used from the UI thread.
//
// Users of this class need to subscribe as observers via the
// AddOrUpdateObserver method. They need to specify which metrics they're
// interested in and at which frequency they should be refreshed. This set of
// metrics and frequencies can then be updated at runtime.
//
// Platforms that want to use this class need to provide a platform specific
// implementation of the SystemMonitorHelper class and update
// SystemMonitor::Create.
class SystemMonitor {
 public:
  // The frequency at which a metric will be collected. Exact frequencies are
  // implementation details determined by experimentation.
  //
  // NOTE: Frequencies must be listed in increasing order in this enum.
  enum class SamplingFrequency : uint32_t {
    kNoSampling = 0,
    kDefaultFrequency = 1,
  };

  virtual ~SystemMonitor();

  // Creates and returns the application-wide SystemMonitor. Can only be called
  // if no SystemMonitor instance exists in the current process. The caller
  // owns the created instance. The current process' instance can be retrieved
  // with Get().
  static std::unique_ptr<SystemMonitor> Create();

  // Get the application-wide SystemMonitor (if not present, returns
  // nullptr).
  static SystemMonitor* Get();

  // Observer that should be notified when new samples are available.
  //
  // When being registered, an observer should declare the metrics it want to
  // track and their refresh frequency.
  class SystemObserver : public base::CheckedObserver {
   public:
    // A struct that associates metrics with their refresh frequencies.
    struct MetricRefreshFrequencies {
      SamplingFrequency free_phys_memory_mb_frequency =
          SamplingFrequency::kNoSampling;
    };

    ~SystemObserver() override = default;

    // Reports the amount of free physical memory, in MB.
    virtual void OnFreePhysicalMemoryMbSample(int free_phys_memory_mb);
  };
  using ObserverToFrequenciesMap =
      base::flat_map<SystemObserver*, SystemObserver::MetricRefreshFrequencies>;

  // Adds |observer| as an observer and updates the metrics to collect and their
  // frequencies based on |metrics_frequencies|. If this observer is already
  // in the list then this simply updates the list of metrics to collect or
  // their frequency.
  void AddOrUpdateObserver(
      SystemObserver* observer,
      SystemObserver::MetricRefreshFrequencies metrics_frequencies);

  // Removes |observer| from the observer list. After this call, the observer
  // will not receive notifications for any metric.
  void RemoveObserver(SystemObserver* observer);

  const SystemObserver::MetricRefreshFrequencies&
  metric_refresh_frequencies_for_testing() const {
    return metrics_refresh_frequencies_;
  }

  bool IsRefreshTimerRunningForTesting() { return refresh_timer_.IsRunning(); }

  void SetHelperForTesting(std::unique_ptr<SystemMonitorHelper> helper) {
    async_helper_.reset(helper.release());
  }

 protected:
  friend class SystemMonitorHelper;

  // Creates SystemMonitor. Only one SystemMonitor instance per
  // application is allowed.
  explicit SystemMonitor(std::unique_ptr<SystemMonitorHelper> helper);

  // A struct that stores a refreshed metric and the reason why it has been
  // refreshed.
  template <typename T>
  struct MetricAndRefreshReason {
    MetricAndRefreshReason() {}
    MetricAndRefreshReason(T value, SamplingFrequency reason)
        : metric_value(value), refresh_reason(reason) {}
    T metric_value = {};
    SamplingFrequency refresh_reason = SamplingFrequency::kNoSampling;
  };

  // Struct that will receive the refreshed metrics in the refresh callback.
  struct MetricsRefresh {
    MetricsRefresh();
    MetricsRefresh(MetricsRefresh&& o);
    MetricAndRefreshReason<int> free_phys_memory_mb;
  };

 private:
  // Updates |observed_metrics_| with the list of metrics that need to be
  // tracked.
  void UpdateObservedMetricsSet();

  // Function that gets called by every time the refresh callback triggers.
  void RefreshCallback();

  // Notify the observers with the refreshed metrics.
  void NotifyObservers(const MetricsRefresh& metrics);

  // The list of observers.
  base::ObserverList<SystemObserver> observers_;

  // A map that associates an observer to the metrics it's interested in.
  ObserverToFrequenciesMap observer_metrics_;

  // The current metrics that are being observed and the corresponding refresh
  // frequency.
  SystemObserver::MetricRefreshFrequencies metrics_refresh_frequencies_;

  // The timer responsible of refreshing the metrics and notifying the
  // observers.
  base::OneShotTimer refresh_timer_;

  // The task runner used to run all the blocking operations.
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // The SystemMonitorHelper instance responsible for running all asynchronous
  // operations.
  std::unique_ptr<SystemMonitorHelper, base::OnTaskRunnerDeleter> async_helper_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SystemMonitor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemMonitor);
};

// Helper class used to run all the blocking operations posted by SystemMonitor
// on a sequence with the |MayBlock()| trait.
//
// Instances of this class should only be destructed once all the posted tasks
// have been run, in practice it means that they should ideally be stored in a
// std::unique_ptr<AsyncHelper, base::OnTaskRunnerDeleter>.
class SystemMonitorHelper {
 public:
  using MetricsRefresh = SystemMonitor::MetricsRefresh;
  template <typename T>
  using MetricAndRefreshReason = SystemMonitor::MetricAndRefreshReason<T>;

  SystemMonitorHelper() = default;
  virtual ~SystemMonitorHelper() = default;

  // Returns the refresh interval that should be used by the SystemMonitor
  // refresh timer based on the current configuration of its observers. If no
  // metrics are being observed then this returns base::TimeDelta::Max().
  //
  // This can be called from any sequence.
  virtual base::TimeDelta GetRefreshInterval(
      const SystemMonitor::SystemObserver::MetricRefreshFrequencies&
          metrics_and_frequencies) = 0;

  // Refresh the metrics according to |metrics_and_frequencies|. |refresh_time|
  // indicates at which time the refresh has been requested.
  virtual MetricsRefresh RefreshMetrics(
      const SystemMonitor::SystemObserver::MetricRefreshFrequencies
          metrics_and_frequencies,
      const base::TimeTicks& refresh_time) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemMonitorHelper);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_H_
