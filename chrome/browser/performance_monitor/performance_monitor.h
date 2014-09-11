// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_H_

#include <map>

#include "base/process/process_handle.h"
#include "base/timer/timer.h"
#include "chrome/browser/performance_monitor/process_metrics_history.h"

template <typename Type>
struct DefaultSingletonTraits;

namespace performance_monitor {

// PerformanceMonitor is a tool which periodically monitors performance metrics
// for histogram logging and possibly taking action upon noticing serious
// performance degradation.
class PerformanceMonitor {
 public:
  typedef std::map<base::ProcessHandle, ProcessMetricsHistory> MetricsMap;

  // Returns the current PerformanceMonitor instance if one exists; otherwise
  // constructs a new PerformanceMonitor.
  static PerformanceMonitor* GetInstance();

  // Start the cycle of metrics gathering.
  void StartGatherCycle();

 private:
  friend struct DefaultSingletonTraits<PerformanceMonitor>;

  PerformanceMonitor();
  virtual ~PerformanceMonitor();

  // Perform any collections that are done on a timed basis.
  void DoTimedCollections();

  // Mark the given process as alive in the current update iteration.
  // This means adding an entry to the map of watched processes if it's not
  // already present.
  void MarkProcessAsAlive(const base::ProcessHandle& handle,
                          int process_type,
                          int current_update_sequence);

  // Updates the ProcessMetrics map with the current list of processes and
  // gathers metrics from each entry.
  void GatherMetricsMapOnUIThread();
  void GatherMetricsMapOnIOThread(int current_update_sequence);

  // A map of currently running ProcessHandles to ProcessMetrics.
  MetricsMap metrics_map_;

  // The timer to signal PerformanceMonitor to perform its timed collections.
  base::RepeatingTimer<PerformanceMonitor> repeating_timer_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceMonitor);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_H_
