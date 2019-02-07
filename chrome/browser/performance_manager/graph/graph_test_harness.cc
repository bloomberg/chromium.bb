// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/graph_test_harness.h"

#include "base/bind.h"
#include "base/run_loop.h"

namespace performance_manager {

GraphTestHarness::GraphTestHarness()
    : task_env_(base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
                base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED),
      service_keepalive_(static_cast<service_manager::ServiceBinding*>(nullptr),
                         base::nullopt /* idle_timeout */),
      provider_(&service_keepalive_, &coordination_unit_graph_) {}

GraphTestHarness::~GraphTestHarness() = default;

void GraphTestHarness::TearDown() {
  base::RunLoop().RunUntilIdle();
}

}  // namespace performance_manager
