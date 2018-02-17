// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/thread_profiler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "chrome/common/stack_sampling_configuration.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/metrics/child_call_stack_profile_collector.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

// Only used by child processes.
base::LazyInstance<metrics::ChildCallStackProfileCollector>::Leaky
    g_child_call_stack_profile_collector = LAZY_INSTANCE_INITIALIZER;

// The profiler object is stored in a SequenceLocalStorageSlot on child threads
// to give it the same lifetime as the threads.
base::LazyInstance<
    base::SequenceLocalStorageSlot<std::unique_ptr<ThreadProfiler>>>::Leaky
    g_child_thread_profiler_sequence_local_storage = LAZY_INSTANCE_INITIALIZER;

base::StackSamplingProfiler::SamplingParams GetSamplingParams() {
  base::StackSamplingProfiler::SamplingParams params;
  params.initial_delay = base::TimeDelta::FromMilliseconds(0);
  params.bursts = 1;
  params.samples_per_burst = 300;
  params.sampling_interval = base::TimeDelta::FromMilliseconds(100);
  return params;
}

metrics::CallStackProfileParams::Process GetProcess() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty())
    return metrics::CallStackProfileParams::BROWSER_PROCESS;
  if (process_type == switches::kRendererProcess)
    return metrics::CallStackProfileParams::RENDERER_PROCESS;
  if (process_type == switches::kGpuProcess)
    return metrics::CallStackProfileParams::GPU_PROCESS;
  if (process_type == switches::kUtilityProcess)
    return metrics::CallStackProfileParams::UTILITY_PROCESS;
  if (process_type == switches::kZygoteProcess)
    return metrics::CallStackProfileParams::ZYGOTE_PROCESS;
  if (process_type == switches::kPpapiPluginProcess)
    return metrics::CallStackProfileParams::PPAPI_PLUGIN_PROCESS;
  if (process_type == switches::kPpapiBrokerProcess)
    return metrics::CallStackProfileParams::PPAPI_BROKER_PROCESS;
  return metrics::CallStackProfileParams::UNKNOWN_PROCESS;
}

}  // namespace

ThreadProfiler::~ThreadProfiler() {}

// static
std::unique_ptr<ThreadProfiler> ThreadProfiler::CreateAndStartOnMainThread(
    metrics::CallStackProfileParams::Thread thread) {
  return std::unique_ptr<ThreadProfiler>(new ThreadProfiler(thread));
}

// static
void ThreadProfiler::StartOnChildThread(
    metrics::CallStackProfileParams::Thread thread) {
  auto profiler = std::unique_ptr<ThreadProfiler>(new ThreadProfiler(thread));
  g_child_thread_profiler_sequence_local_storage.Get().Set(std::move(profiler));
}

void ThreadProfiler::SetServiceManagerConnectorForChildProcess(
    service_manager::Connector* connector) {
  if (!StackSamplingConfiguration::Get()->IsProfilerEnabledForCurrentProcess())
    return;

  DCHECK_NE(metrics::CallStackProfileParams::BROWSER_PROCESS, GetProcess());

  metrics::mojom::CallStackProfileCollectorPtr browser_interface;
  connector->BindInterface(content::mojom::kBrowserServiceName,
                           &browser_interface);
  g_child_call_stack_profile_collector.Get().SetParentProfileCollector(
      std::move(browser_interface));
}

ThreadProfiler::ThreadProfiler(metrics::CallStackProfileParams::Thread thread)
    : startup_profile_params_(GetProcess(),
                              thread,
                              metrics::CallStackProfileParams::PROCESS_STARTUP,
                              metrics::CallStackProfileParams::MAY_SHUFFLE) {
  if (!StackSamplingConfiguration::Get()->IsProfilerEnabledForCurrentProcess())
    return;

  startup_profiler_ = std::make_unique<base::StackSamplingProfiler>(
      base::PlatformThread::CurrentId(), GetSamplingParams(),
      BindRepeating(&ThreadProfiler::ReceiveStartupProfiles,
                    GetReceiverCallback(&startup_profile_params_)));
  startup_profiler_->Start();
}

base::StackSamplingProfiler::CompletedCallback
ThreadProfiler::GetReceiverCallback(
    metrics::CallStackProfileParams* profile_params) {
  profile_params->start_timestamp = base::TimeTicks::Now();
  // TODO(wittman): Simplify the approach to getting the profiler callback
  // across CallStackProfileMetricsProvider and
  // ChildCallStackProfileCollector. Ultimately both should expose functions
  // like
  //
  //   void ReceiveCompletedProfiles(
  //       const metrics::CallStackProfileParams& profile_params,
  //       base::TimeTicks profile_start_time,
  //       base::StackSamplingProfiler::CallStackProfiles profiles);
  //
  // and this function should bind the passed profile_params and
  // base::TimeTicks::Now() to those functions.
  if (GetProcess() == metrics::CallStackProfileParams::BROWSER_PROCESS) {
    return metrics::CallStackProfileMetricsProvider::
        GetProfilerCallbackForBrowserProcess(profile_params);
  }
  return g_child_call_stack_profile_collector.Get()
      .ChildCallStackProfileCollector::GetProfilerCallback(*profile_params);
}

// static
base::Optional<base::StackSamplingProfiler::SamplingParams>
ThreadProfiler::ReceiveStartupProfiles(
    const base::StackSamplingProfiler::CompletedCallback& receiver_callback,
    base::StackSamplingProfiler::CallStackProfiles profiles) {
  receiver_callback.Run(std::move(profiles));
  return base::Optional<base::StackSamplingProfiler::SamplingParams>();
}
