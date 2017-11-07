// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILING_HOST_BACKGROUND_PROFILING_TRIGGERS_H_
#define CHROME_BROWSER_PROFILING_HOST_BACKGROUND_PROFILING_TRIGGERS_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/timer/timer.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"

namespace profiling {

class ProfilingProcessHost;

// BackgroundProfilingTriggers is used on the browser process to trigger the
// collection of memory dumps and upload the results to the slow-reports
// service. BackgroundProfilingTriggers class sets a periodic timer and
// interacts with ProfilingProcessHost to trigger and upload memory dumps.
class BackgroundProfilingTriggers {
 public:
  explicit BackgroundProfilingTriggers(ProfilingProcessHost* host);
  virtual ~BackgroundProfilingTriggers();

  // Register a periodic timer calling |PerformMemoryUsageChecks|.
  void StartTimer();

 private:
  friend class FakeBackgroundProfilingTriggers;

  // Returns true if |private_footprint_kb| is large enough to trigger
  // a report for the given |content_process_type|.
  bool IsOverTriggerThreshold(int content_process_type,
                              uint32_t private_footprint_kb);

  // Check the current memory usage and send a slow-report if needed.
  void PerformMemoryUsageChecks();
  void PerformMemoryUsageChecksOnIOThread();

  // Called when the memory dump is received. Performs
  // checks on memory usage and trigger a memory report with
  // |TriggerMemoryReportForProcess| if needed.
  void OnReceivedMemoryDump(
      bool success,
      memory_instrumentation::mojom::GlobalMemoryDumpPtr ptr);

  // Virtual for testing. Called when a memory report needs to be send.
  virtual void TriggerMemoryReportForProcess(base::ProcessId pid);

  // Timer to periodically check memory consumption and upload a slow-report.
  base::RepeatingTimer timer_;
  ProfilingProcessHost* host_;

  base::WeakPtrFactory<BackgroundProfilingTriggers> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundProfilingTriggers);
};

}  // namespace profiling

#endif  // CHROME_BROWSER_PROFILING_HOST_BACKGROUND_PROFILING_TRIGGERS_H_
