// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/profiler/main_thread_stack_sampling_profiler.h"

#include "base/command_line.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/stack_sampling_configuration.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/metrics/child_call_stack_profile_collector.h"
#include "content/public/common/content_switches.h"

namespace {

// Returns the profiler callback appropriate for the current process.
base::StackSamplingProfiler::CompletedCallback GetProfilerCallback() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  // The browser process has an empty process type.
  if (process_type.empty()) {
    return metrics::CallStackProfileMetricsProvider::
        GetProfilerCallbackForBrowserProcessStartup();
  }

  if (process_type == switches::kGpuProcess) {
    return metrics::ChildCallStackProfileCollector::Get()->GetProfilerCallback(
        metrics::CallStackProfileParams(
            metrics::CallStackProfileParams::GPU_PROCESS,
            metrics::CallStackProfileParams::GPU_MAIN_THREAD,
            metrics::CallStackProfileParams::PROCESS_STARTUP,
            metrics::CallStackProfileParams::MAY_SHUFFLE));
  }

  // No other processes are currently supported.
  NOTREACHED();
  return base::StackSamplingProfiler::CompletedCallback();
}

}  // namespace

MainThreadStackSamplingProfiler::MainThreadStackSamplingProfiler() {
  StackSamplingConfiguration* config = StackSamplingConfiguration::Get();
  if (!config->IsProfilerEnabledForCurrentProcess())
    return;

  sampling_profiler_ = std::make_unique<base::StackSamplingProfiler>(
      base::PlatformThread::CurrentId(),
      config->GetSamplingParamsForCurrentProcess(), GetProfilerCallback());
  sampling_profiler_->Start();
}

// Note that it's important for the |sampling_profiler_| destructor to run, as
// it ensures program correctness on shutdown. Without it, the profiler thread's
// destruction can race with the profiled thread's destruction, which results in
// the sampling thread attempting to profile the sampled thread after the
// sampled thread has already been shut down.
MainThreadStackSamplingProfiler::~MainThreadStackSamplingProfiler() = default;
