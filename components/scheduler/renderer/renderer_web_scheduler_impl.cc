// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/renderer_web_scheduler_impl.h"

#include "components/scheduler/child/task_queue.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "components/scheduler/renderer/web_frame_host_scheduler_impl.h"

namespace scheduler {

RendererWebSchedulerImpl::RendererWebSchedulerImpl(
    RendererScheduler* renderer_scheduler)
    : WebSchedulerImpl(renderer_scheduler,
                       renderer_scheduler->IdleTaskRunner(),
                       renderer_scheduler->LoadingTaskRunner(),
                       renderer_scheduler->TimerTaskRunner()),
      renderer_scheduler_(renderer_scheduler) {
}

RendererWebSchedulerImpl::~RendererWebSchedulerImpl() {
}

void RendererWebSchedulerImpl::suspendTimerQueue() {
  renderer_scheduler_->SuspendTimerQueue();
}

void RendererWebSchedulerImpl::resumeTimerQueue() {
  renderer_scheduler_->ResumeTimerQueue();
}

blink::WebFrameHostScheduler*
RendererWebSchedulerImpl::createFrameHostScheduler() {
  return new WebFrameHostSchedulerImpl(renderer_scheduler_);
}

}  // namespace scheduler
