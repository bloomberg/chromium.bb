// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/web_frame_scheduler_impl.h"

#include "components/scheduler/base/real_time_domain.h"
#include "components/scheduler/base/virtual_time_domain.h"
#include "components/scheduler/child/web_task_runner_impl.h"
#include "components/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"
#include "components/scheduler/renderer/web_view_scheduler_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace scheduler {

WebFrameSchedulerImpl::WebFrameSchedulerImpl(
    RendererSchedulerImpl* renderer_scheduler,
    WebViewSchedulerImpl* parent_web_view_scheduler)
    : renderer_scheduler_(renderer_scheduler),
      parent_web_view_scheduler_(parent_web_view_scheduler),
      frame_visible_(true),
      page_visible_(true) {}

WebFrameSchedulerImpl::~WebFrameSchedulerImpl() {
  if (loading_task_queue_.get())
    loading_task_queue_->UnregisterTaskQueue();

  if (timer_task_queue_.get())
    timer_task_queue_->UnregisterTaskQueue();

  if (parent_web_view_scheduler_)
    parent_web_view_scheduler_->Unregister(this);
}

void WebFrameSchedulerImpl::DetachFromWebViewScheduler() {
  parent_web_view_scheduler_ = nullptr;
}

void WebFrameSchedulerImpl::setFrameVisible(bool frame_visible) {
  frame_visible_ = frame_visible;
  // TODO(alexclarke): Do something with this flag.
}

blink::WebTaskRunner* WebFrameSchedulerImpl::loadingTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!loading_web_task_runner_) {
    loading_task_queue_ =
        renderer_scheduler_->NewLoadingTaskRunner("frame_loading_tq");
    if (parent_web_view_scheduler_->virtual_time_domain()) {
      loading_task_queue_->SetTimeDomain(
          parent_web_view_scheduler_->virtual_time_domain());
      loading_task_queue_->SetPumpPolicy(
          parent_web_view_scheduler_->GetVirtualTimePumpPolicy());
    }
    loading_web_task_runner_.reset(new WebTaskRunnerImpl(loading_task_queue_));
  }
  return loading_web_task_runner_.get();
}

blink::WebTaskRunner* WebFrameSchedulerImpl::timerTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!timer_web_task_runner_) {
    timer_task_queue_ =
        renderer_scheduler_->NewTimerTaskRunner("frame_timer_tq");
    if (parent_web_view_scheduler_->virtual_time_domain()) {
      timer_task_queue_->SetTimeDomain(
          parent_web_view_scheduler_->virtual_time_domain());
      timer_task_queue_->SetPumpPolicy(
          parent_web_view_scheduler_->GetVirtualTimePumpPolicy());
    } else if (!page_visible_) {
      renderer_scheduler_->throttling_helper()->IncreaseThrottleRefCount(
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

void WebFrameSchedulerImpl::setPageVisible(bool page_visible) {
  DCHECK(parent_web_view_scheduler_);
  if (page_visible_ == page_visible)
    return;

  page_visible_ = page_visible;

  if (!timer_web_task_runner_ ||
      parent_web_view_scheduler_->virtual_time_domain()) {
    return;
  }

  if (page_visible_) {
    renderer_scheduler_->throttling_helper()->DecreaseThrottleRefCount(
        timer_task_queue_.get());
  } else {
    renderer_scheduler_->throttling_helper()->IncreaseThrottleRefCount(
        timer_task_queue_.get());
  }
}

void WebFrameSchedulerImpl::OnVirtualTimeDomainChanged() {
  DCHECK(parent_web_view_scheduler_);
  DCHECK(parent_web_view_scheduler_->virtual_time_domain());

  if (timer_task_queue_) {
    renderer_scheduler_->throttling_helper()->UnregisterTaskQueue(
        timer_task_queue_.get());
    timer_task_queue_->SetTimeDomain(
        parent_web_view_scheduler_->virtual_time_domain());
    timer_task_queue_->SetPumpPolicy(
        parent_web_view_scheduler_->GetVirtualTimePumpPolicy());
  }

  if (loading_task_queue_) {
    loading_task_queue_->SetTimeDomain(
        parent_web_view_scheduler_->virtual_time_domain());
    loading_task_queue_->SetPumpPolicy(
        parent_web_view_scheduler_->GetVirtualTimePumpPolicy());
  }
}

void WebFrameSchedulerImpl::OnVirtualTimePumpPolicyChanged() {
  if (!parent_web_view_scheduler_->virtual_time_domain())
    return;

  if (timer_task_queue_) {
    timer_task_queue_->SetPumpPolicy(
        parent_web_view_scheduler_->GetVirtualTimePumpPolicy());
  }

  if (loading_task_queue_) {
    loading_task_queue_->SetPumpPolicy(
        parent_web_view_scheduler_->GetVirtualTimePumpPolicy());
  }
}

}  // namespace scheduler
