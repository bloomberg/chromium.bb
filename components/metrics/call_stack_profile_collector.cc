// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_collector.h"

#include <memory>
#include <utility>

#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace metrics {

namespace {

// Translates CallStackProfileParams's process to the corresponding execution
// context Process.
Process ToExecutionContextProcess(CallStackProfileParams::Process process) {
  switch (process) {
    case CallStackProfileParams::UNKNOWN_PROCESS:
      return UNKNOWN_PROCESS;
    case CallStackProfileParams::BROWSER_PROCESS:
      return BROWSER_PROCESS;
    case CallStackProfileParams::RENDERER_PROCESS:
      return RENDERER_PROCESS;
    case CallStackProfileParams::GPU_PROCESS:
      return GPU_PROCESS;
    case CallStackProfileParams::UTILITY_PROCESS:
      return UTILITY_PROCESS;
    case CallStackProfileParams::ZYGOTE_PROCESS:
      return ZYGOTE_PROCESS;
    case CallStackProfileParams::SANDBOX_HELPER_PROCESS:
      return SANDBOX_HELPER_PROCESS;
    case CallStackProfileParams::PPAPI_PLUGIN_PROCESS:
      return PPAPI_PLUGIN_PROCESS;
    case CallStackProfileParams::PPAPI_BROKER_PROCESS:
      return PPAPI_BROKER_PROCESS;
  }
  NOTREACHED();
  return UNKNOWN_PROCESS;
}

}  // namespace

CallStackProfileCollector::CallStackProfileCollector(
    CallStackProfileParams::Process expected_process)
    : expected_process_(expected_process) {}

CallStackProfileCollector::~CallStackProfileCollector() {}

// static
void CallStackProfileCollector::Create(
    CallStackProfileParams::Process expected_process,
    mojom::CallStackProfileCollectorRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<CallStackProfileCollector>(expected_process),
      std::move(request));
}

void CallStackProfileCollector::Collect(base::TimeTicks start_timestamp,
                                        SampledProfile profile) {
  if (profile.process() != ToExecutionContextProcess(expected_process_))
    return;

  CallStackProfileMetricsProvider::ReceiveCompletedProfile(start_timestamp,
                                                           std::move(profile));
}

}  // namespace metrics
