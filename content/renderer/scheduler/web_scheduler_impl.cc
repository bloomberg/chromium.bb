// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/web_scheduler_impl.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "content/renderer/scheduler/renderer_scheduler.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace content {

WebSchedulerImpl::WebSchedulerImpl(RendererScheduler* renderer_scheduler)
    : renderer_scheduler_(renderer_scheduler),
      idle_task_runner_(renderer_scheduler_->IdleTaskRunner()),
      loading_task_runner_(renderer_scheduler_->LoadingTaskRunner()),
      timer_task_runner_(renderer_scheduler_->DefaultTaskRunner()) {
}

WebSchedulerImpl::~WebSchedulerImpl() {
}

bool WebSchedulerImpl::shouldYieldForHighPriorityWork() {
  return renderer_scheduler_->ShouldYieldForHighPriorityWork();
}

bool WebSchedulerImpl::canExceedIdleDeadlineIfRequired() {
  return renderer_scheduler_->CanExceedIdleDeadlineIfRequired();
}

void WebSchedulerImpl::runIdleTask(
    scoped_ptr<blink::WebScheduler::IdleTask> task,
    base::TimeTicks deadline) {
  task->run((deadline - base::TimeTicks()).InSecondsF());
}

void WebSchedulerImpl::runTask(scoped_ptr<blink::WebThread::Task> task) {
  task->run();
}

void WebSchedulerImpl::postIdleTask(const blink::WebTraceLocation& web_location,
                                    blink::WebScheduler::IdleTask* task) {
  DCHECK(idle_task_runner_);
  scoped_ptr<blink::WebScheduler::IdleTask> scoped_task(task);
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  idle_task_runner_->PostIdleTask(
      location,
      base::Bind(&WebSchedulerImpl::runIdleTask, base::Passed(&scoped_task)));
}

void WebSchedulerImpl::postNonNestableIdleTask(
    const blink::WebTraceLocation& web_location,
    blink::WebScheduler::IdleTask* task) {
  DCHECK(idle_task_runner_);
  scoped_ptr<blink::WebScheduler::IdleTask> scoped_task(task);
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  idle_task_runner_->PostNonNestableIdleTask(
      location,
      base::Bind(&WebSchedulerImpl::runIdleTask, base::Passed(&scoped_task)));
}

void WebSchedulerImpl::postIdleTaskAfterWakeup(
    const blink::WebTraceLocation& web_location,
    blink::WebScheduler::IdleTask* task) {
  DCHECK(idle_task_runner_);
  scoped_ptr<blink::WebScheduler::IdleTask> scoped_task(task);
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  idle_task_runner_->PostIdleTaskAfterWakeup(
      location,
      base::Bind(&WebSchedulerImpl::runIdleTask, base::Passed(&scoped_task)));
}

void WebSchedulerImpl::postLoadingTask(
    const blink::WebTraceLocation& web_location, blink::WebThread::Task* task) {
  DCHECK(loading_task_runner_);
  scoped_ptr<blink::WebThread::Task> scoped_task(task);
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  loading_task_runner_->PostTask(
      location,
      base::Bind(&WebSchedulerImpl::runTask, base::Passed(&scoped_task)));
}

void WebSchedulerImpl::postTimerTask(
    const blink::WebTraceLocation& web_location,
    blink::WebThread::Task* task,
    long long delayMs) {
  DCHECK(timer_task_runner_);
  scoped_ptr<blink::WebThread::Task> scoped_task(task);
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  timer_task_runner_->PostDelayedTask(
      location,
      base::Bind(&WebSchedulerImpl::runTask, base::Passed(&scoped_task)),
      base::TimeDelta::FromMilliseconds(delayMs));
}

void WebSchedulerImpl::shutdown() {
  // TODO(rmcilroy): Delete this method once the blink side is removed.
}

}  // namespace content
