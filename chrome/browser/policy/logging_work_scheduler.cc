// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/logging_work_scheduler.h"

#include <algorithm>

#include "base/message_loop.h"
#include "content/browser/browser_thread.h"

namespace policy {

// Objects of this class are used in the priority-queue of future tasks
// in EventLogger.
class EventLogger::Task {
 public:
  Task();
  Task(int64 trigger_time,
       int64 secondary_key,
       linked_ptr<base::Closure> callback);
  ~Task();

  // Returns true if |this| should be executed before |rhs|.
  // Used for sorting by the priority queue.
  bool operator< (const Task& rhs) const;

  int64 trigger_time() const;

  // Returns a copy of the callback object of this task, and resets the
  // original callback object. (LoggingTaskScheduler owns a linked_ptr to
  // its task's callback objects and it only allows firing new tasks if the
  // previous task's callback object has been reset.)
  base::Closure GetAndResetCallback();

 private:
  // The virtual time when this task will trigger.
  // Smaller times win.
  int64 trigger_time_;
  // Used for sorting tasks that have the same trigger_time.
  // Bigger keys win.
  int64 secondary_key_;

  linked_ptr<base::Closure> callback_;
};

EventLogger::Task::Task() {
}

EventLogger::Task::Task(int64 trigger_time,
                        int64 secondary_key,
                        linked_ptr<base::Closure> callback)
    : trigger_time_(trigger_time),
      secondary_key_(secondary_key),
      callback_(callback) {
}

EventLogger::Task::~Task() {
}

bool EventLogger::Task::operator< (const Task& rhs) const {
  if (trigger_time_ == rhs.trigger_time_) {
    return secondary_key_ < rhs.secondary_key_;
  } else {
    return trigger_time_ > rhs.trigger_time_;
  }
}

int64 EventLogger::Task::trigger_time() const {
  return trigger_time_;
}

base::Closure EventLogger::Task::GetAndResetCallback() {
  // Make a copy.
  base::Closure callback = *(callback_.get());
  // Reset the original callback, which is shared with LoggingTaskScheduler.
  callback_->Reset();
  return callback;
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
  Task next = scheduled_tasks_.top();
  scheduled_tasks_.pop();
  base::Closure callback = next.GetAndResetCallback();
  int64 trigger_time = next.trigger_time();
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
  if (!callback_.get()) return;
  callback_->Reset();  // Erase the callback to the delayed work.
  callback_.reset(NULL);  // Erase the pointer to the callback.
}

}  // namespace policy
