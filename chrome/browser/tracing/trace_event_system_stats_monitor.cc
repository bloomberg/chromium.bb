// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/trace_event_system_stats_monitor.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/process/process_metrics.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"

namespace tracing {

namespace {

// Length of time interval between stat profiles.
static const int kSamplingIntervalMilliseconds = 2000;

// Converts system memory profiling stats in |input| to trace event compatible
// JSON and appends to |output|.
void AppendSystemProfileAsTraceFormat(const base::SystemMetrics& system_metrics,
                                      std::string* output) {
  std::string tmp;
  base::JSONWriter::Write(*system_metrics.ToValue(), &tmp);
  *output += tmp;
}

/////////////////////////////////////////////////////////////////////////////
// Holds profiled system stats until the tracing system needs to serialize it.
class SystemStatsHolder : public base::trace_event::ConvertableToTraceFormat {
 public:
  SystemStatsHolder() = default;
  ~SystemStatsHolder() override = default;

  // Fills system_metrics_ with profiled system memory and disk stats.
  // Uses the previous stats to compute rates if this is not the first profile.
  void GetSystemProfilingStats();

  // base::trace_event::ConvertableToTraceFormat overrides:
  void AppendAsTraceFormat(std::string* out) const override {
    AppendSystemProfileAsTraceFormat(system_stats_, out);
  }

 private:
  base::SystemMetrics system_stats_;

  DISALLOW_COPY_AND_ASSIGN(SystemStatsHolder);
};

void SystemStatsHolder::GetSystemProfilingStats() {
  system_stats_ = base::SystemMetrics::Sample();
}

void DumpSystemStatsImpl(TraceEventSystemStatsMonitor* stats_monitor) {
  std::unique_ptr<SystemStatsHolder> dump_holder =
      std::make_unique<SystemStatsHolder>();
  dump_holder->GetSystemProfilingStats();

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("system_stats"),
      "base::TraceEventSystemStatsMonitor::SystemStats", stats_monitor,
      std::move(dump_holder));
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////

TraceEventSystemStatsMonitor::TraceEventSystemStatsMonitor()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()), weak_factory_(this) {
  // Force the "system_stats" category to show up in the trace viewer.
  base::trace_event::TraceLog::GetCategoryGroupEnabled(
      TRACE_DISABLED_BY_DEFAULT("system_stats"));

  // Allow this to be instantiated on unsupported platforms, but don't run.
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(this);
}

TraceEventSystemStatsMonitor::~TraceEventSystemStatsMonitor() {
  if (dump_timer_.IsRunning())
    StopProfiling();
  base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
}

void TraceEventSystemStatsMonitor::OnTraceLogEnabled() {
  // Check to see if system tracing is enabled.
  bool enabled;

  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("system_stats"),
                                     &enabled);
  if (!enabled)
    return;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TraceEventSystemStatsMonitor::StartProfiling,
                                weak_factory_.GetWeakPtr()));
}

void TraceEventSystemStatsMonitor::OnTraceLogDisabled() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TraceEventSystemStatsMonitor::StopProfiling,
                                weak_factory_.GetWeakPtr()));
}

void TraceEventSystemStatsMonitor::StartProfiling() {
  // Watch for the tracing framework sending enabling more than once.
  if (dump_timer_.IsRunning())
    return;

  dump_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kSamplingIntervalMilliseconds),
      base::BindRepeating(&TraceEventSystemStatsMonitor::DumpSystemStats,
                          weak_factory_.GetWeakPtr()));
}

void TraceEventSystemStatsMonitor::DumpSystemStats() {
  // Calls to |DumpSystemStatsImpl| might be blocking.
  //
  // TODO(sebmarchand): Ideally the timer that calls this function should use a
  // thread with the MayBlock trait to avoid having to use a trampoline here.
  // This isn't currently possible due to https://crbug.com/896990.
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&DumpSystemStatsImpl, base::Unretained(this)));
}

void TraceEventSystemStatsMonitor::StopProfiling() {
  dump_timer_.Stop();
}

bool TraceEventSystemStatsMonitor::IsTimerRunningForTesting() const {
  return dump_timer_.IsRunning();
}

}  // namespace tracing
