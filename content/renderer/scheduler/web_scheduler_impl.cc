// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/web_scheduler_impl.h"

#include "base/bind.h"
#include "content/renderer/scheduler/renderer_scheduler.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace content {

WebSchedulerImpl::WebSchedulerImpl(RendererScheduler* renderer_scheduler)
    : renderer_scheduler_(renderer_scheduler),
      idle_task_runner_(renderer_scheduler_->IdleTaskRunner()) {
}

WebSchedulerImpl::~WebSchedulerImpl() {
}

bool WebSchedulerImpl::shouldYieldForHighPriorityWork() {
  return renderer_scheduler_->ShouldYieldForHighPriorityWork();
}

void WebSchedulerImpl::runIdleTask(
    scoped_ptr<blink::WebScheduler::IdleTask> task,
    base::TimeTicks deadline) {
  task->run((deadline - base::TimeTicks()).InSecondsF());
}

void WebSchedulerImpl::postIdleTask(const blink::WebTraceLocation& web_location,
                                    blink::WebScheduler::IdleTask* task) {
  scoped_ptr<blink::WebScheduler::IdleTask> scoped_task(task);
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  idle_task_runner_->PostIdleTask(
      location,
      base::Bind(&WebSchedulerImpl::runIdleTask, base::Passed(&scoped_task)));
}

void WebSchedulerImpl::shutdown() {
  return renderer_scheduler_->Shutdown();
}

}  // namespace content
