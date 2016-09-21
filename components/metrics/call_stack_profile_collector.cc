// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_collector.h"

#include "base/memory/ptr_util.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace metrics {

CallStackProfileCollector::CallStackProfileCollector() {}

CallStackProfileCollector::~CallStackProfileCollector() {}

// static
void CallStackProfileCollector::Create(
    mojom::CallStackProfileCollectorRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<CallStackProfileCollector>(),
                          std::move(request));
}

void CallStackProfileCollector::Collect(
    const CallStackProfileParams& params,
    base::TimeTicks start_timestamp,
    const std::vector<CallStackProfile>& profiles) {
  CallStackProfileMetricsProvider::ReceiveCompletedProfiles(params,
                                                            start_timestamp,
                                                            profiles);
}

}  // namespace metrics
