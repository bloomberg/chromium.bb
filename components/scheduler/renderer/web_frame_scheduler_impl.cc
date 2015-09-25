// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/web_frame_scheduler_impl.h"

#include "components/scheduler/child/web_task_runner_impl.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "components/scheduler/renderer/web_frame_host_scheduler_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace scheduler {

WebFrameSchedulerImpl::WebFrameSchedulerImpl(
    RendererScheduler* render_scheduler,
    WebFrameHostSchedulerImpl* parent_frame_host_scheduler)
    : render_scheduler_(render_scheduler),
      parent_frame_host_scheduler_(parent_frame_host_scheduler),
      visible_(true) {}

WebFrameSchedulerImpl::~WebFrameSchedulerImpl() {
  if (loading_task_queue_.get())
    loading_task_queue_->UnregisterTaskQueue();

  if (timer_task_queue_.get())
    timer_task_queue_->UnregisterTaskQueue();

  parent_frame_host_scheduler_->Unregister(this);
}

void WebFrameSchedulerImpl::setFrameVisible(bool visible) {
  visible_ = visible;
  // TODO(alexclarke): Do something with this flag.
}

blink::WebTaskRunner* WebFrameSchedulerImpl::loadingTaskRunner() {
  if (!loading_web_task_runner_) {
    loading_task_queue_ =
        render_scheduler_->NewLoadingTaskRunner("frame_loading_tq");
    loading_web_task_runner_.reset(new WebTaskRunnerImpl(loading_task_queue_));
  }
  return loading_web_task_runner_.get();
}

blink::WebTaskRunner* WebFrameSchedulerImpl::timerTaskRunner() {
  if (!timer_web_task_runner_) {
    timer_task_queue_ = render_scheduler_->NewTimerTaskRunner("frame_timer_tq");
    timer_web_task_runner_.reset(new WebTaskRunnerImpl(timer_task_queue_));
  }
  return timer_web_task_runner_.get();
}

void WebFrameSchedulerImpl::setFrameOrigin(
    const blink::WebSecurityOrigin* origin) {
  DCHECK(origin);
  origin_ = *origin;
  // TODO(skyostil): Associate the task queues with this origin.
}

}  // namespace scheduler
