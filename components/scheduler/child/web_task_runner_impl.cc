// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/web_task_runner_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace scheduler {

WebTaskRunnerImpl::WebTaskRunnerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {}

WebTaskRunnerImpl::~WebTaskRunnerImpl() {}

void WebTaskRunnerImpl::postTask(const blink::WebTraceLocation& web_location,
                                 blink::WebTaskRunner::Task* task) {
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  task_runner_->PostTask(location, base::Bind(&blink::WebTaskRunner::Task::run,
                                              base::Owned(task)));
}

void WebTaskRunnerImpl::postDelayedTask(
    const blink::WebTraceLocation& web_location,
    blink::WebTaskRunner::Task* task,
    double delayMs) {
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  task_runner_->PostDelayedTask(
      location, base::Bind(&blink::WebTaskRunner::Task::run, base::Owned(task)),
      base::TimeDelta::FromMillisecondsD(delayMs));
}

}  // namespace scheduler
