// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_H_

#include <array>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace performance_monitor {

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
// implementation of the |Evaluate| functions of the various
// SystemMonitor::MetricEvaluator classes defined in this file.
class SystemMonitor {
 public:
  // The frequency at which a metric will be collected. Exact frequencies are
  // implementation details determined by experimentation.
  //
  // NOTE: Frequencies must be listed in increasing order in this enum.
  enum class SamplingFrequency : uint32_t {
    kNoSampling,
    kDefaultFrequency,
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

  const base::OneShotTimer& refresh_timer_for_testing() {
    return refresh_timer_;
  }

 protected:
  // Represents a metric. Overridden for each metric tracked by this monitor.
  class MetricEvaluator {
   public:
    enum class Type : size_t {
      kFreeMemoryMb,
      kMax,
    };

    explicit MetricEvaluator(Type type);
    virtual ~MetricEvaluator();

    // Called when the metric needs to be evaluated.
    virtual void Evaluate() = 0;

    // Notify |observer| that a value is available, should only be called after
    // Evaluate().
    virtual void NotifyObserver(SystemObserver* observer) = 0;

    // Returns the metric type.
    Type type() const { return type_; }

    // Indicates if the metric has a valid value.
    bool has_value() const { return has_value_; }

   protected:
    bool has_value_ = false;

   private:
    const Type type_;

    DISALLOW_COPY_AND_ASSIGN(MetricEvaluator);
  };

  // Structure storing all the functions specific to a metric.
  struct MetricMetadata {
    MetricMetadata() = delete;
    MetricMetadata(std::unique_ptr<MetricEvaluator> (*create_function)(),
                   SamplingFrequency (*get_refresh_field_function)(
                       const SystemObserver::MetricRefreshFrequencies&));
    // A pointer to the function that creates the appropriate |MetricEvaluator|
    // instance for a given metric.
    // TODO(sebmarchand): Make this a const member.
    std::unique_ptr<MetricEvaluator> (*create_metric_evaluator_function)();
    // A pointer to the function that extract the sampling frequency for a given
    // metric from a MetricRefreshFrequencies struct.
    SamplingFrequency (*get_refresh_frequency_field_function)(
        const SystemObserver::MetricRefreshFrequencies&);
  };

  class FreePhysMemoryMetricEvaluator;
  using MetricVector = std::vector<std::unique_ptr<MetricEvaluator>>;
  using MetricSamplingFrequencyArray =
      std::array<SamplingFrequency,
                 static_cast<size_t>(MetricEvaluator::Type::kMax)>;

  // Creates SystemMonitor. Only one SystemMonitor instance per application is
  // allowed.
  SystemMonitor();

  // Returns a vector with all the metrics that should be evaluated given the
  // current list of observers.
  SystemMonitor::MetricVector GetMetricsToEvaluate() const;

  const MetricSamplingFrequencyArray&
  GetMetricSamplingFrequencyArrayForTesting() {
    return metrics_refresh_frequencies_;
  }

  MetricMetadata* GetMetricEvaluatorMetadataForTesting(
      MetricEvaluator::Type type) {
    DCHECK_LT(static_cast<size_t>(type), metric_evaluators_metadata_.size());
    return const_cast<MetricMetadata*>(
        &metric_evaluators_metadata_[static_cast<size_t>(type)]);
  }

 private:
  using MetricMetadataArray =
      const std::array<const MetricMetadata,
                       static_cast<size_t>(MetricEvaluator::Type::kMax)>;
  // Evaluate the metrics in |metric_vector|.
  static SystemMonitor::MetricVector EvaluateMetrics(
      MetricVector metric_vector);

  // Create the array of MetricMetadata used to initialize
  // |metric_evaluators_metadata_|.
  static MetricMetadataArray CreateMetricMetadataArray();

  // Updates |observed_metrics_| with the list of metrics that need to be
  // tracked. Starts or stop |refresh_timer_| if needed.
  void UpdateObservedMetrics();

  // Function that gets called by every time the refresh callback triggers.
  void RefreshCallback();

  // Notify the observers with the refreshed metrics.
  void NotifyObservers(SystemMonitor::MetricVector metrics);

  // The list of observers.
  base::ObserverList<SystemObserver> observers_;

  // A map that associates an observer to the metrics it's interested in.
  ObserverToFrequenciesMap observer_metrics_;

  // The current metrics that are being observed and the corresponding refresh
  // frequency.
  MetricSamplingFrequencyArray metrics_refresh_frequencies_ = {};

  // The timer responsible of refreshing the metrics and notifying the
  // observers.
  base::OneShotTimer refresh_timer_;

  // There should be one |MetricMetadata| for each value of
  // |MetricEvaluator::Type|.
  MetricMetadataArray metric_evaluators_metadata_;

  // The task runner used to run all the blocking operations.
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SystemMonitor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemMonitor);
};

// Implementation of the |MetricEvaluator| interface that tracks the amount of
// free physical memory.
class SystemMonitor::FreePhysMemoryMetricEvaluator
    : public SystemMonitor::MetricEvaluator {
 public:
  FreePhysMemoryMetricEvaluator();
  ~FreePhysMemoryMetricEvaluator() override = default;

  // SystemMonitor::MetricEvaluator:
  void Evaluate() override;
  void NotifyObserver(SystemObserver* observer) override;

  int value() const { return value_; }
  void set_value_for_testing(int value) { value_ = value; }

 private:
  int value_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FreePhysMemoryMetricEvaluator);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_SYSTEM_MONITOR_H_
