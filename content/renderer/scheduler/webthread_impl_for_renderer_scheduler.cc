// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/webthread_impl_for_renderer_scheduler.h"

#include "content/child/scheduler/web_scheduler_impl.h"
#include "content/renderer/scheduler/renderer_scheduler.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace content {

WebThreadImplForRendererScheduler::WebThreadImplForRendererScheduler(
    RendererScheduler* scheduler)
    : web_scheduler_(new WebSchedulerImpl(scheduler,
                                          scheduler->IdleTaskRunner(),
                                          scheduler->LoadingTaskRunner(),
                                          scheduler->TimerTaskRunner())),
      task_runner_(scheduler->DefaultTaskRunner()),
      idle_task_runner_(scheduler->IdleTaskRunner()),
      scheduler_(scheduler),
      thread_id_(base::PlatformThread::CurrentId()) {
}

WebThreadImplForRendererScheduler::~WebThreadImplForRendererScheduler() {
}

blink::PlatformThreadId WebThreadImplForRendererScheduler::threadId() const {
  return thread_id_;
}

blink::WebScheduler* WebThreadImplForRendererScheduler::scheduler() const {
  return web_scheduler_.get();
}

base::MessageLoop* WebThreadImplForRendererScheduler::MessageLoop() const {
  DCHECK(isCurrentThread());
  return base::MessageLoop::current();
}

base::SingleThreadTaskRunner* WebThreadImplForRendererScheduler::TaskRunner()
    const {
  return task_runner_.get();
}

SingleThreadIdleTaskRunner* WebThreadImplForRendererScheduler::IdleTaskRunner()
    const {
  return idle_task_runner_.get();
}

void WebThreadImplForRendererScheduler::AddTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  scheduler_->AddTaskObserver(observer);
}

void WebThreadImplForRendererScheduler::RemoveTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  scheduler_->RemoveTaskObserver(observer);
}

}  // namespace content
