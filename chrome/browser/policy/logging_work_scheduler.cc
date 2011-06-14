// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/logging_work_scheduler.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "content/browser/browser_thread.h"

namespace policy {

bool EventLogger::Task::operator< (const Task& rhs) const {
  if (trigger_time == rhs.trigger_time) {
    return secondary_key < rhs.secondary_key;
  } else {
    return trigger_time > rhs.trigger_time;
  }
}

EventLogger::Task::Task() {
}

EventLogger::Task::Task(
    int64 trigger_time, int64 secondar_key, linked_ptr<base::Closure> callback)
        : trigger_time(trigger_time),
          secondary_key(secondary_key),
          callback(callback) {
}

EventLogger::EventLogger()
    : step_task_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      current_time_(0),
      task_counter_(0) {
}

EventLogger::~EventLogger() {
}

void EventLogger::PostDelayedWork(
    linked_ptr<base::Closure> callback,
    int64 delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int64 trigger_time = current_time_ + delay;
  task_counter_++;
  scheduled_tasks_.push(Task(trigger_time, task_counter_, callback));
  if (!step_task_) {
    step_task_ = method_factory_.NewRunnableMethod(&EventLogger::Step);
    MessageLoop::current()->PostTask(FROM_HERE, step_task_);
  }
}

void EventLogger::RegisterEvent() {
  events_.push_back(current_time_);
}

void EventLogger::Swap(std::vector<int64>* events) {
  std::swap(events_, *events);
}

// static
// Counts the number of elements in a sorted integer vector that are
// >= |start| but < |start| + |length|.
int EventLogger::CountEvents(const std::vector<int64>& events,
                             int64 start, int64 length) {
  // The first interesting event.
  std::vector<int64>::const_iterator begin_it =
      std::lower_bound(events.begin(), events.end(), start);
  // The event after the last interesting event.
  std::vector<int64>::const_iterator end_it =
      std::lower_bound(begin_it, events.end(), start + length);
  return end_it - begin_it;
}

void EventLogger::Step() {
  DCHECK(step_task_);
  step_task_ = NULL;
  if (scheduled_tasks_.empty())
    return;
  // Take the next scheduled task from the queue.
  const Task& next = scheduled_tasks_.top();
  base::Closure callback = *(next.callback.get());
  int64 trigger_time = next.trigger_time;
  // Mark as cancelled, because LoggingTaskScheduler still owns a linked_ptr to
  // this task and it uses it to avoid owning more than one pending tasks.
  next.callback->Reset();
  scheduled_tasks_.pop();
  DCHECK(trigger_time >= current_time_);
  // Execute the next scheduled task if it was not cancelled.
  if (!callback.is_null()) {
    current_time_ = trigger_time;
    callback.Run(); // Note: new items may be added to scheduled_tasks_ here.
  }
  // Trigger calling this method if there are remaining tasks.
  if (!step_task_ && !scheduled_tasks_.empty()) {
    step_task_ = method_factory_.NewRunnableMethod(&EventLogger::Step);
    MessageLoop::current()->PostTask(FROM_HERE, step_task_);
  }
}

DummyWorkScheduler::DummyWorkScheduler() {
}

DummyWorkScheduler::~DummyWorkScheduler() {
  CancelDelayedWork();
}

void DummyWorkScheduler::PostDelayedWork(
    const base::Closure& callback, int64 delay) {
  DelayedWorkScheduler::PostDelayedWork(callback, 0);
}

LoggingWorkScheduler::LoggingWorkScheduler(EventLogger* logger)
    : logger_(logger) {
}

LoggingWorkScheduler::~LoggingWorkScheduler() {
  if (callback_.get())
    callback_->Reset();
}

void LoggingWorkScheduler::PostDelayedWork(
    const base::Closure& callback, int64 delay) {
  DCHECK(!callback_.get() || callback_->is_null());
  callback_.reset(new base::Closure(callback));
  logger_->PostDelayedWork(callback_, delay);
}

void LoggingWorkScheduler::CancelDelayedWork() {
  DCHECK(callback_.get());
  callback_->Reset();
}

}  // namespace policy
