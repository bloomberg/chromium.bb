// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_collector.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace metrics {

CallStackProfileCollector::CallStackProfileCollector(
    CallStackProfileParams::Process expected_process)
    : expected_process_(expected_process) {}

CallStackProfileCollector::~CallStackProfileCollector() {}

// static
void CallStackProfileCollector::Create(
    CallStackProfileParams::Process expected_process,
    const service_manager::BindSourceInfo& source_info,
    mojom::CallStackProfileCollectorRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<CallStackProfileCollector>(expected_process),
      std::move(request));
}

void CallStackProfileCollector::Collect(
    const CallStackProfileParams& params,
    base::TimeTicks start_timestamp,
    std::vector<CallStackProfile> profiles) {
  if (params.process != expected_process_)
    return;

  CallStackProfileMetricsProvider::ReceiveCompletedProfiles(
      params, start_timestamp, std::move(profiles));
}

}  // namespace metrics
