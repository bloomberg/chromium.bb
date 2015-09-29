// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/ppapi/webthread_impl_for_ppapi.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "components/scheduler/base/task_queue.h"
#include "components/scheduler/child/scheduler_task_runner_delegate_impl.h"
#include "components/scheduler/child/web_scheduler_impl.h"
#include "components/scheduler/child/web_task_runner_impl.h"
#include "components/scheduler/child/worker_scheduler_impl.h"

namespace scheduler {

WebThreadImplForPPAPI::WebThreadImplForPPAPI()
    : thread_id_(base::PlatformThread::CurrentId()),
      task_runner_delegate_(SchedulerTaskRunnerDelegateImpl::Create(
          base::MessageLoop::current())),
      worker_scheduler_(WorkerScheduler::Create(task_runner_delegate_)) {
  worker_scheduler_->Init();
  task_runner_ = worker_scheduler_->DefaultTaskRunner();
  idle_task_runner_ = worker_scheduler_->IdleTaskRunner();
  web_scheduler_.reset(new WebSchedulerImpl(
      worker_scheduler_.get(), worker_scheduler_->IdleTaskRunner(),
      worker_scheduler_->DefaultTaskRunner(),
      worker_scheduler_->DefaultTaskRunner()));
  web_task_runner_ = make_scoped_ptr(new WebTaskRunnerImpl(task_runner_));
}

WebThreadImplForPPAPI::~WebThreadImplForPPAPI() {}

blink::PlatformThreadId WebThreadImplForPPAPI::threadId() const {
  return thread_id_;
}

blink::WebScheduler* WebThreadImplForPPAPI::scheduler() const {
  return web_scheduler_.get();
}

base::SingleThreadTaskRunner* WebThreadImplForPPAPI::TaskRunner() const {
  return task_runner_.get();
}

SingleThreadIdleTaskRunner* WebThreadImplForPPAPI::IdleTaskRunner() const {
  return idle_task_runner_.get();
}

blink::WebTaskRunner* WebThreadImplForPPAPI::taskRunner() {
  return web_task_runner_.get();
}

void WebThreadImplForPPAPI::AddTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  worker_scheduler_->AddTaskObserver(observer);
}

void WebThreadImplForPPAPI::RemoveTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  worker_scheduler_->RemoveTaskObserver(observer);
}

}  // namespace scheduler
