// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/webthread_impl_for_worker_scheduler.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "components/scheduler/child/scheduler_message_loop_delegate.h"
#include "components/scheduler/child/web_scheduler_impl.h"
#include "components/scheduler/child/worker_scheduler_impl.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace scheduler {

WebThreadImplForWorkerScheduler::WebThreadImplForWorkerScheduler(
    const char* name)
    : thread_(new base::Thread(name)) {
  thread_->Start();

  base::WaitableEvent completion(false, false);
  thread_->message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebThreadImplForWorkerScheduler::InitOnThread,
                            base::Unretained(this), &completion));
  completion.Wait();
}

WebThreadImplForWorkerScheduler::~WebThreadImplForWorkerScheduler() {
  base::WaitableEvent completion(false, false);
  // We need to post the shutdown task on the scheduler's task queue or tasks
  // posted on the worker scheduler may not get run when the thread is deleted.
  TaskRunner()->PostTask(
      FROM_HERE, base::Bind(&WebThreadImplForWorkerScheduler::ShutDownOnThread,
                            base::Unretained(this), &completion));
  completion.Wait();

  thread_->Stop();
}

void WebThreadImplForWorkerScheduler::InitOnThread(
    base::WaitableEvent* completion) {
  worker_scheduler_ = WorkerScheduler::Create(thread_->message_loop());
  worker_scheduler_->Init();
  task_runner_ = worker_scheduler_->DefaultTaskRunner();
  idle_task_runner_ = worker_scheduler_->IdleTaskRunner();
  web_scheduler_.reset(new WebSchedulerImpl(
      worker_scheduler_.get(), worker_scheduler_->IdleTaskRunner(),
      worker_scheduler_->DefaultTaskRunner(),
      worker_scheduler_->DefaultTaskRunner()));
  completion->Signal();
}

void WebThreadImplForWorkerScheduler::ShutDownOnThread(
    base::WaitableEvent* completion) {
  task_runner_ = nullptr;
  idle_task_runner_ = nullptr;
  web_scheduler_.reset();
  worker_scheduler_.reset();
  completion->Signal();
}

blink::PlatformThreadId WebThreadImplForWorkerScheduler::threadId() const {
  return thread_->thread_id();
}

blink::WebScheduler* WebThreadImplForWorkerScheduler::scheduler() const {
  return web_scheduler_.get();
}

base::MessageLoop* WebThreadImplForWorkerScheduler::MessageLoop() const {
  // As per WebThreadImpl::MessageLoop()
  return nullptr;
}

base::SingleThreadTaskRunner* WebThreadImplForWorkerScheduler::TaskRunner()
    const {
  return task_runner_.get();
}

SingleThreadIdleTaskRunner* WebThreadImplForWorkerScheduler::IdleTaskRunner()
    const {
  return idle_task_runner_.get();
}

void WebThreadImplForWorkerScheduler::AddTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  worker_scheduler_->AddTaskObserver(observer);
}

void WebThreadImplForWorkerScheduler::RemoveTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  worker_scheduler_->RemoveTaskObserver(observer);
}

}  // namespace scheduler
