// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/webthread_impl_for_scheduler.h"

#include "content/renderer/scheduler/renderer_scheduler.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace content {

WebThreadImplForScheduler::WebThreadImplForScheduler(
    RendererScheduler* scheduler)
    : task_runner_(scheduler->DefaultTaskRunner()),
      scheduler_(scheduler),
      thread_id_(base::PlatformThread::CurrentId()) {
}

WebThreadImplForScheduler::~WebThreadImplForScheduler() {
}

blink::PlatformThreadId WebThreadImplForScheduler::threadId() const {
  return thread_id_;
}

base::MessageLoop* WebThreadImplForScheduler::MessageLoop() const {
  DCHECK(isCurrentThread());
  return base::MessageLoop::current();
}

base::SingleThreadTaskRunner* WebThreadImplForScheduler::TaskRunner() const {
  return task_runner_.get();
}

void WebThreadImplForScheduler::AddTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  scheduler_->AddTaskObserver(observer);
}

void WebThreadImplForScheduler::RemoveTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  scheduler_->RemoveTaskObserver(observer);
}

}  // namespace content
