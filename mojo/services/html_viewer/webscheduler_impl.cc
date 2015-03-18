// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of WebThread in terms of base::MessageLoop and
// base::Thread

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/tracked_objects.h"
#include "mojo/services/html_viewer/webscheduler_impl.h"

namespace html_viewer {

WebSchedulerImpl::WebSchedulerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {
}

WebSchedulerImpl::~WebSchedulerImpl() {
}

void WebSchedulerImpl::postIdleTask(const blink::WebTraceLocation& web_location,
                                    blink::WebScheduler::IdleTask* task) {
  scoped_ptr<blink::WebScheduler::IdleTask> scoped_task(task);
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  task_runner_->PostTask(location, base::Bind(&WebSchedulerImpl::RunIdleTask,
                                              base::Passed(&scoped_task)));
}

void WebSchedulerImpl::postLoadingTask(
    const blink::WebTraceLocation& web_location,
    blink::WebThread::Task* task) {
  scoped_ptr<blink::WebThread::Task> scoped_task(task);
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  task_runner_->PostTask(location, base::Bind(&WebSchedulerImpl::RunTask,
                                              base::Passed(&scoped_task)));
}

void WebSchedulerImpl::postTimerTask(
    const blink::WebTraceLocation& web_location,
    blink::WebThread::Task* task,
    long long delayMs) {
  scoped_ptr<blink::WebThread::Task> scoped_task(task);
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  task_runner_->PostDelayedTask(
      location,
      base::Bind(&WebSchedulerImpl::RunTask, base::Passed(&scoped_task)),
      base::TimeDelta::FromMilliseconds(delayMs));
}

void WebSchedulerImpl::shutdown() {
  task_runner_ = nullptr;
}

// static
void WebSchedulerImpl::RunIdleTask(
    scoped_ptr<blink::WebScheduler::IdleTask> task) {
  // TODO(davemoore) Implement idle scheduling.
  task->run(0);
}

// static
void WebSchedulerImpl::RunTask(scoped_ptr<blink::WebThread::Task> task) {
  task->run();
}

}  // namespace html_viewer
