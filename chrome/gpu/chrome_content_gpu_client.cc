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
#include "chrome/common/stack_sampling_configuration.h"
#include "components/metrics/child_call_stack_profile_collector.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_registry.h"

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

base::LazyInstance<metrics::ChildCallStackProfileCollector>::Leaky
    g_call_stack_profile_collector = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ChromeContentGpuClient::ChromeContentGpuClient()
    : stack_sampling_profiler_(
        base::PlatformThread::CurrentId(),
        StackSamplingConfiguration::Get()->GetSamplingParamsForCurrentProcess(),
        g_call_stack_profile_collector.Get().GetProfilerCallback(
            metrics::CallStackProfileParams(
                metrics::CallStackProfileParams::GPU_PROCESS,
                metrics::CallStackProfileParams::GPU_MAIN_THREAD,
                metrics::CallStackProfileParams::PROCESS_STARTUP,
                metrics::CallStackProfileParams::MAY_SHUFFLE))) {
  if (StackSamplingConfiguration::Get()->IsProfilerEnabledForCurrentProcess())
    stack_sampling_profiler_.Start();
}

ChromeContentGpuClient::~ChromeContentGpuClient() {}

void ChromeContentGpuClient::Initialize(
    base::FieldTrialList::Observer* observer) {
  DCHECK(!field_trial_syncer_);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  // No need for field trial syncer if we're in the browser process.
  if (!command_line.HasSwitch(switches::kInProcessGPU)) {
    field_trial_syncer_.reset(
        new variations::ChildProcessFieldTrialSyncer(observer));
    field_trial_syncer_->InitFieldTrialObserving(command_line,
                                                 switches::kSingleProcess);
  }
}

void ChromeContentGpuClient::ExposeInterfacesToBrowser(
    service_manager::InterfaceRegistry* registry,
    const gpu::GpuPreferences& gpu_preferences) {
#if defined(OS_CHROMEOS)
  registry->AddInterface(
      base::Bind(&CreateGpuArcVideoService, gpu_preferences));
  registry->AddInterface(
      base::Bind(&DeprecatedCreateGpuArcVideoService, gpu_preferences));
#endif
}

void ChromeContentGpuClient::ConsumeInterfacesFromBrowser(
    service_manager::InterfaceProvider* provider) {
  metrics::mojom::CallStackProfileCollectorPtr browser_interface;
  provider->GetInterface(&browser_interface);
  g_call_stack_profile_collector.Get().SetParentProfileCollector(
      std::move(browser_interface));
}
