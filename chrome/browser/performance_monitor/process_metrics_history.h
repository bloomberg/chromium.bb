// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_METRICS_HISTORY_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_METRICS_HISTORY_H_

#include "base/memory/linked_ptr.h"
#include "base/process/process_handle.h"
#include "content/public/browser/child_process_data.h"

namespace base {
class ProcessMetrics;
}

namespace performance_monitor {

class ProcessMetricsHistory {
 public:
  ProcessMetricsHistory();
  ~ProcessMetricsHistory();

  // Configure this to monitor a specific process.
  void Initialize(const content::ChildProcessData& process_data,
                  int initial_update_sequence);

  // End of a measurement cycle; check for performance issues and reset
  // accumulated statistics.
  void EndOfCycle();

  // Gather metrics for the process and accumulate with past data.
  void SampleMetrics();

  // Used to mark when this object was last updated, so we can cull
  // dead ones.
  void set_last_update_sequence(int new_update_sequence) {
    last_update_sequence_ = new_update_sequence;
  }

  int last_update_sequence() const { return last_update_sequence_; }

  // Average CPU over all the past sampling points since last reset.
  double GetAverageCPUUsage() const {
    return accumulated_cpu_usage_ / sample_count_;
  }

  // Average mem usage over all the past sampling points since last reset.
  void GetAverageMemoryBytes(size_t* private_bytes,
                             size_t* shared_bytes) const {
    *private_bytes = accumulated_private_bytes_ / sample_count_;
    *shared_bytes = accumulated_shared_bytes_ / sample_count_;
  }

 private:
  void ResetCounters();
  void RunPerformanceTriggers();

  // May not be fully populated. e.g. no |id| and no |name| for browser and
  // renderer processes.
  content::ChildProcessData process_data_;
  linked_ptr<base::ProcessMetrics> process_metrics_;
  int last_update_sequence_;

  double accumulated_cpu_usage_;
  double min_cpu_usage_;
  size_t accumulated_private_bytes_;
  size_t accumulated_shared_bytes_;
  int sample_count_;

  DISALLOW_ASSIGN(ProcessMetricsHistory);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_PROCESS_METRICS_HISTORY_H_
