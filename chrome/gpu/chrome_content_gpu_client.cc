// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/chrome_content_gpu_client.h"

#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/metrics/child_call_stack_profile_collector.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/interface_registry.h"

#if defined(OS_CHROMEOS)
#include "chrome/gpu/gpu_arc_video_service.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
void DeprecatedCreateGpuArcVideoService(
    const gpu::GpuPreferences& gpu_preferences,
    ::arc::mojom::VideoAcceleratorServiceClientRequest client_request) {
  chromeos::arc::GpuArcVideoService::DeprecatedConnect(
      base::MakeUnique<chromeos::arc::GpuArcVideoService>(gpu_preferences),
      std::move(client_request));
}

void CreateGpuArcVideoService(
    const gpu::GpuPreferences& gpu_preferences,
    ::arc::mojom::VideoAcceleratorServiceRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<chromeos::arc::GpuArcVideoService>(gpu_preferences),
      std::move(request));
}
#endif

// Returns appropriate parameters for stack sampling on startup.
base::StackSamplingProfiler::SamplingParams GetStartupSamplingParams() {
  base::StackSamplingProfiler::SamplingParams params;
  params.initial_delay = base::TimeDelta::FromMilliseconds(0);
  params.bursts = 1;
  params.samples_per_burst = 300;
  params.sampling_interval = base::TimeDelta::FromMilliseconds(100);
  return params;
}

base::LazyInstance<metrics::ChildCallStackProfileCollector>::Leaky
    g_call_stack_profile_collector = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ChromeContentGpuClient::ChromeContentGpuClient()
    : stack_sampling_profiler_(
        base::PlatformThread::CurrentId(),
        GetStartupSamplingParams(),
        g_call_stack_profile_collector.Get().GetProfilerCallback(
            metrics::CallStackProfileParams(
                metrics::CallStackProfileParams::GPU_PROCESS,
                metrics::CallStackProfileParams::GPU_MAIN_THREAD,
                metrics::CallStackProfileParams::PROCESS_STARTUP,
                metrics::CallStackProfileParams::MAY_SHUFFLE))) {
}

ChromeContentGpuClient::~ChromeContentGpuClient() {}

void ChromeContentGpuClient::Initialize(
    base::FieldTrialList::Observer* observer) {
  DCHECK(!field_trial_syncer_);
  field_trial_syncer_.reset(
      new chrome_variations::ChildProcessFieldTrialSyncer(observer));
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  field_trial_syncer_->InitFieldTrialObserving(command_line);
}

void ChromeContentGpuClient::ExposeInterfacesToBrowser(
    shell::InterfaceRegistry* registry,
    const gpu::GpuPreferences& gpu_preferences) {
#if defined(OS_CHROMEOS)
  registry->AddInterface(
      base::Bind(&CreateGpuArcVideoService, gpu_preferences));
  registry->AddInterface(
      base::Bind(&DeprecatedCreateGpuArcVideoService, gpu_preferences));
#endif
}

void ChromeContentGpuClient::ConsumeInterfacesFromBrowser(
    shell::InterfaceProvider* provider) {
  metrics::mojom::CallStackProfileCollectorPtr browser_interface;
  provider->GetInterface(&browser_interface);
  g_call_stack_profile_collector.Get().SetParentProfileCollector(
      std::move(browser_interface));
}
