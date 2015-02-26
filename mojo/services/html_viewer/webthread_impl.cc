// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of WebThread in terms of base::MessageLoop and
// base::Thread

#include "mojo/services/html_viewer/webthread_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/pending_task.h"
#include "base/threading/platform_thread.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace html_viewer {

WebThreadBase::WebThreadBase() {}
WebThreadBase::~WebThreadBase() {}

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

void WebThreadBase::addTaskObserver(TaskObserver* observer) {
  CHECK(isCurrentThread());
  std::pair<TaskObserverMap::iterator, bool> result = task_observer_map_.insert(
      std::make_pair(observer, static_cast<TaskObserverAdapter*>(NULL)));
  if (result.second)
    result.first->second = new TaskObserverAdapter(observer);
  base::MessageLoop::current()->AddTaskObserver(result.first->second);
}

void WebThreadBase::removeTaskObserver(TaskObserver* observer) {
  CHECK(isCurrentThread());
  TaskObserverMap::iterator iter = task_observer_map_.find(observer);
  if (iter == task_observer_map_.end())
    return;
  base::MessageLoop::current()->RemoveTaskObserver(iter->second);
  delete iter->second;
  task_observer_map_.erase(iter);
}

WebThreadImpl::WebThreadImpl(const char* name)
    : thread_(new base::Thread(name)) {
  thread_->Start();
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
static void RunWebThreadTask(scoped_ptr<blink::WebThread::Task> task) {
    task->run();
}

void WebThreadImpl::postTask(const blink::WebTraceLocation& location,
                             Task* task) {
  postDelayedTask(location, task, 0);
}

void WebThreadImpl::postDelayedTask(const blink::WebTraceLocation& web_location,
                                    Task* task,
                                    long long delay_ms) {
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  thread_->message_loop()->PostDelayedTask(
      location,
      base::Bind(RunWebThreadTask, base::Passed(make_scoped_ptr(task))),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

void WebThreadImpl::enterRunLoop() {
  CHECK(isCurrentThread());
  CHECK(!thread_->message_loop()->is_running());  // We don't support nesting.
  thread_->message_loop()->Run();
}

void WebThreadImpl::exitRunLoop() {
  CHECK(isCurrentThread());
  CHECK(thread_->message_loop()->is_running());
  thread_->message_loop()->Quit();
}

bool WebThreadImpl::isCurrentThread() const {
  return thread_->thread_id() == base::PlatformThread::CurrentId();
}

blink::PlatformThreadId WebThreadImpl::threadId() const {
  return thread_->thread_id();
}

WebThreadImpl::~WebThreadImpl() {
  thread_->Stop();
}

WebThreadImplForMessageLoop::WebThreadImplForMessageLoop(
    base::MessageLoopProxy* message_loop)
    : message_loop_(message_loop) {}

void WebThreadImplForMessageLoop::postTask(
    const blink::WebTraceLocation& location,
    Task* task) {
  postDelayedTask(location, task, 0);
}

void WebThreadImplForMessageLoop::postDelayedTask(
    const blink::WebTraceLocation& web_location,
    Task* task,
    long long delay_ms) {
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  message_loop_->PostDelayedTask(
      location,
      base::Bind(RunWebThreadTask, base::Passed(make_scoped_ptr(task))),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

void WebThreadImplForMessageLoop::enterRunLoop() {
  CHECK(isCurrentThread());
  // We don't support nesting.
  CHECK(!base::MessageLoop::current()->is_running());
  base::MessageLoop::current()->Run();
}

void WebThreadImplForMessageLoop::exitRunLoop() {
  CHECK(isCurrentThread());
  CHECK(base::MessageLoop::current()->is_running());
  base::MessageLoop::current()->Quit();
}

bool WebThreadImplForMessageLoop::isCurrentThread() const {
  return message_loop_->BelongsToCurrentThread();
}

blink::PlatformThreadId WebThreadImplForMessageLoop::threadId() const {
  return thread_id_;
}

WebThreadImplForMessageLoop::~WebThreadImplForMessageLoop() {}

}  // namespace html_viewer
