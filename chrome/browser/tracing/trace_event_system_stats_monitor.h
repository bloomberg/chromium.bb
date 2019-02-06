// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_TRACE_EVENT_SYSTEM_STATS_MONITOR_H_
#define CHROME_BROWSER_TRACING_TRACE_EVENT_SYSTEM_STATS_MONITOR_H_

#include <string>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_metrics.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_log.h"

namespace tracing {

// Watches for chrome://tracing to be enabled or disabled. When tracing is
// enabled, also enables system events profiling. This class is the preferred
// way to turn system tracing on and off.
class TraceEventSystemStatsMonitor
    : public base::trace_event::TraceLog::EnabledStateObserver {
 public:
  TraceEventSystemStatsMonitor();
  ~TraceEventSystemStatsMonitor() override;

  // base::trace_event::TraceLog::EnabledStateChangedObserver overrides:
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

  // Retrieves system profiling at the current time.
  void DumpSystemStats();

  bool IsTimerRunningForTesting() const;

  void StartProfilingForTesting() { StartProfiling(); }
  void StopProfilingForTesting() { StopProfiling(); }

 private:
  void StartProfiling();

  void StopProfiling();

  // Ensures the observer starts and stops tracing on the primary thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Timer to schedule system profile dumps.
  base::RepeatingTimer dump_timer_;

  base::WeakPtrFactory<TraceEventSystemStatsMonitor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TraceEventSystemStatsMonitor);
};

}  // namespace tracing

#endif  // CHROME_BROWSER_TRACING_TRACE_EVENT_SYSTEM_STATS_MONITOR_H_
