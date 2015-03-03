// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of WebThread in terms of base::MessageLoop and
// base::Thread

#include "content/child/webthread_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/pending_task.h"
#include "base/threading/platform_thread.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace content {

class WebThreadBase::TaskObserverAdapter
    : public base::MessageLoop::TaskObserver {
 public:
  TaskObserverAdapter(WebThread::TaskObserver* observer)
      : observer_(observer) {}

  void WillProcessTask(const base::PendingTask& pending_task) override {
    observer_->willProcessTask();
  }

  void DidProcessTask(const base::PendingTask& pending_task) override {
    observer_->didProcessTask();
  }

private:
  WebThread::TaskObserver* observer_;
};

WebThreadBase::WebThreadBase() {
}

WebThreadBase::~WebThreadBase() {
  for (auto& observer_entry : task_observer_map_) {
    delete observer_entry.second;
  }
}

void WebThreadBase::addTaskObserver(TaskObserver* observer) {
  CHECK(isCurrentThread());
  std::pair<TaskObserverMap::iterator, bool> result = task_observer_map_.insert(
      std::make_pair(observer, static_cast<TaskObserverAdapter*>(NULL)));
  if (result.second)
    result.first->second = new TaskObserverAdapter(observer);
  AddTaskObserverInternal(result.first->second);
}

void WebThreadBase::removeTaskObserver(TaskObserver* observer) {
  CHECK(isCurrentThread());
  TaskObserverMap::iterator iter = task_observer_map_.find(observer);
  if (iter == task_observer_map_.end())
    return;
  RemoveTaskObserverInternal(iter->second);
  delete iter->second;
  task_observer_map_.erase(iter);
}

void WebThreadBase::AddTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  base::MessageLoop::current()->AddTaskObserver(observer);
}

void WebThreadBase::RemoveTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  base::MessageLoop::current()->RemoveTaskObserver(observer);
}

// RunWebThreadTask takes the ownership of |task| from base::Closure and
// deletes it on the first invocation of the closure for thread-safety.
// base::Closure made from RunWebThreadTask is copyable but Closure::Run
// should be called at most only once.
// This is because WebThread::Task can contain RefPtr to a
// thread-unsafe-reference-counted object (e.g. WorkerThreadTask can contain
// RefPtr to WebKit's StringImpl), and if we don't delete |task| here,
// it causes a race condition as follows:
// [A] In task->run(), more RefPtr's to the refcounted object can be created,
//     and the reference counter of the object can be modified via these
//     RefPtr's (as intended) on the thread where the task is executed.
// [B] However, base::Closure still retains the ownership of WebThread::Task
//     even after RunWebThreadTask is called.
//     When base::Closure is deleted, WebThread::Task is deleted and the
//     reference counter of the object is decreased by one, possibly from a
//     different thread from [A], which is a race condition.
// Taking the ownership of |task| here by using scoped_ptr and base::Passed
// removes the reference counter modification of [B] and the race condition.
// When the closure never runs at all, the corresponding WebThread::Task is
// destructed when base::Closure is deleted (like [B]). In this case, there
// are no reference counter modification like [A] (because task->run() is not
// executed), so there are no race conditions.
// See https://crbug.com/390851 for more details.
//
// static
void WebThreadBase::RunWebThreadTask(scoped_ptr<blink::WebThread::Task> task) {
  task->run();
}

void WebThreadBase::postTask(const blink::WebTraceLocation& location,
                             Task* task) {
  postDelayedTask(location, task, 0);
}

void WebThreadBase::postDelayedTask(const blink::WebTraceLocation& web_location,
                                    Task* task,
                                    long long delay_ms) {
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  TaskRunner()->PostDelayedTask(
      location,
      base::Bind(RunWebThreadTask, base::Passed(make_scoped_ptr(task))),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

void WebThreadBase::enterRunLoop() {
  CHECK(isCurrentThread());
  CHECK(MessageLoop());
  CHECK(!MessageLoop()->is_running());  // We don't support nesting.
  MessageLoop()->Run();
}

void WebThreadBase::exitRunLoop() {
  CHECK(isCurrentThread());
  CHECK(MessageLoop());
  CHECK(MessageLoop()->is_running());
  MessageLoop()->Quit();
}

bool WebThreadBase::isCurrentThread() const {
  return TaskRunner()->BelongsToCurrentThread();
}

blink::PlatformThreadId WebThreadImpl::threadId() const {
  return thread_->thread_id();
}

WebThreadImpl::WebThreadImpl(const char* name)
    : thread_(new base::Thread(name)) {
  thread_->Start();
}

WebThreadImpl::~WebThreadImpl() {
  thread_->Stop();
}

base::MessageLoop* WebThreadImpl::MessageLoop() const {
  return nullptr;
}

base::SingleThreadTaskRunner* WebThreadImpl::TaskRunner() const {
  return thread_->message_loop_proxy().get();
}

}  // namespace content
