// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/test/renderer_scheduler_test_support.h"

#include "components/scheduler/renderer/renderer_scheduler_impl.h"

namespace scheduler {

void RunIdleTasksForTesting(
    RendererScheduler* scheduler,
    const base::Closure& callback) {
  RendererSchedulerImpl* scheduler_impl = static_cast<RendererSchedulerImpl*>(
      scheduler);
  scheduler_impl->RunIdleTasksForTesting(callback);
}

} // namespace scheduler
