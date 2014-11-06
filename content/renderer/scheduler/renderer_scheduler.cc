// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/renderer_scheduler.h"

#include "content/renderer/scheduler/null_renderer_scheduler.h"

namespace content {

RendererScheduler::RendererScheduler() {
}

RendererScheduler::~RendererScheduler() {
}

// static
scoped_ptr<RendererScheduler> RendererScheduler::Create() {
  // TODO(rmcilroy): Use the RendererSchedulerImpl when the scheduler is enabled
  return make_scoped_ptr(new NullRendererScheduler());
}

}  // namespace content
