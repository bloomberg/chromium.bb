// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/web_frame_scheduler_impl.h"

#include "components/scheduler/base/real_time_domain.h"
#include "components/scheduler/base/virtual_time_domain.h"
#include "components/scheduler/child/web_task_runner_impl.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"
#include "components/scheduler/renderer/web_view_scheduler_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace scheduler {

WebFrameSchedulerImpl::WebFrameSchedulerImpl(
    RendererSchedulerImpl* renderer_scheduler,
    WebViewSchedulerImpl* parent_web_view_scheduler)
    : renderer_scheduler_(renderer_scheduler),
      parent_web_view_scheduler_(parent_web_view_scheduler),
      visible_(true),
      page_in_background_(false) {}

WebFrameSchedulerImpl::~WebFrameSchedulerImpl() {
  if (loading_task_queue_.get())
    loading_task_queue_->UnregisterTaskQueue();

  if (timer_task_queue_.get()) {
    renderer_scheduler_->throttling_helper()->Unthrottle(
        timer_task_queue_.get());
    timer_task_queue_->UnregisterTaskQueue();
  }

  if (parent_web_view_scheduler_)
    parent_web_view_scheduler_->Unregister(this);
}

void WebFrameSchedulerImpl::DetachFromWebViewScheduler() {
  parent_web_view_scheduler_ = nullptr;
}

void WebFrameSchedulerImpl::setFrameVisible(bool visible) {
  visible_ = visible;
  // TODO(alexclarke): Do something with this flag.
}

blink::WebTaskRunner* WebFrameSchedulerImpl::loadingTaskRunner() {
  if (!loading_web_task_runner_) {
    loading_task_queue_ =
        renderer_scheduler_->NewLoadingTaskRunner("frame_loading_tq");
    loading_web_task_runner_.reset(new WebTaskRunnerImpl(loading_task_queue_));
  }
  return loading_web_task_runner_.get();
}

blink::WebTaskRunner* WebFrameSchedulerImpl::timerTaskRunner() {
  if (!timer_web_task_runner_) {
    timer_task_queue_ =
        renderer_scheduler_->NewTimerTaskRunner("frame_timer_tq");
    if (page_in_background_) {
      renderer_scheduler_->throttling_helper()->Throttle(
          timer_task_queue_.get());
    }
    timer_web_task_runner_.reset(new WebTaskRunnerImpl(timer_task_queue_));
  }
  return timer_web_task_runner_.get();
}

void WebFrameSchedulerImpl::setFrameOrigin(
    const blink::WebSecurityOrigin& origin) {
  origin_ = origin;
  // TODO(skyostil): Associate the task queues with this origin.
}

void WebFrameSchedulerImpl::SetPageInBackground(bool page_in_background) {
  if (page_in_background_ == page_in_background)
    return;

  page_in_background_ = page_in_background;

  if (!timer_web_task_runner_)
    return;

  if (page_in_background_) {
    renderer_scheduler_->throttling_helper()->Throttle(timer_task_queue_.get());
  } else {
    renderer_scheduler_->throttling_helper()->Unthrottle(
        timer_task_queue_.get());
  }
}

}  // namespace scheduler
