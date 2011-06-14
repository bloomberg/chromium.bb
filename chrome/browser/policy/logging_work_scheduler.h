// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_LOGGING_WORK_SCHEDULER_H_
#define CHROME_BROWSER_POLICY_LOGGING_WORK_SCHEDULER_H_
#pragma once

#include <queue>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/policy/delayed_work_scheduler.h"

namespace policy {

// This implementation of DelayedWorkScheduler always schedules the tasks
// with zero delay.
class DummyWorkScheduler : public DelayedWorkScheduler {
 public:
  DummyWorkScheduler();
  virtual ~DummyWorkScheduler();

  virtual void PostDelayedWork(const base::Closure& callback, int64 delay);

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyWorkScheduler);
};

// Helper class for LoggingWorkScheduler. It essentially emulates a real
// message loop. All the submitted tasks are run with zero delay, but the
// order in which they would run with delays is preserved.
// All the task posting requests of the schedulers will be channeled through
// a common instance of EventLogger. This makes sure, that this instance can
// keep track of time in the simulation and record logged events with correct
// timestamps.
class EventLogger {
 public:
  EventLogger();
  virtual ~EventLogger();

  // Post a task to be executed |delay| milliseconds from now. The task can be
  // cancelled later by calling Reset() on the callback.
  void PostDelayedWork(linked_ptr<base::Closure> callback, int64 delay);

 // Register a new event that happened now according to the internal clock.
  void RegisterEvent();

  // Swap out the internal list of events.
  void Swap(std::vector<int64>* events);

  // Counts the events in a sorted integer array that are >= |start| but
  // < |start| + |length|.
  static int CountEvents(const std::vector<int64>& events,
                         int64 start, int64 length);

 private:
  struct Task {
    Task();
    Task(int64 trigger_time_, int64 secondary_key,
         linked_ptr<base::Closure> callback);

    // Returns true if |this| should be executed before |rhs|.
    // Used for sorting by the priority queue.
    bool operator< (const Task& rhs) const;

    // The virtual time when this task will trigger.
    // Smaller times win.
    int64 trigger_time;
    // Used for sorting tasks that have the same trigger_time.
    // Bigger keys win.
    int64 secondary_key;

    linked_ptr<base::Closure> callback;
  };

  // Updates |current_time_| and triggers the next scheduled task. This method
  // is run repeatedly on the main message loop until there are scheduled
  // tasks.
  void Step();

  // Stores the list of scheduled tasks with their respective delays and
  // schedulers.
  std::priority_queue<Task> scheduled_tasks_;

  // Machinery to put a call to |Step| at the end of the message loop.
  CancelableTask* step_task_;
  ScopedRunnableMethodFactory<EventLogger> method_factory_;

  // Ascending list of observation-times of the logged events.
  std::vector<int64> events_;
  // The current time of the simulated system.
  int64 current_time_;
  // The total number of tasks scheduled so far.
  int64 task_counter_;

  DISALLOW_COPY_AND_ASSIGN(EventLogger);
};

// Issues delayed tasks with zero effective delay, but posts them through
// an EventLogger, to make it possible to log events and reconstruct their
// execution time.
class LoggingWorkScheduler : public DelayedWorkScheduler {
 public:
  explicit LoggingWorkScheduler(EventLogger* logger);
  virtual ~LoggingWorkScheduler();

  virtual void PostDelayedWork(const base::Closure& callback, int64 delay);
  virtual void CancelDelayedWork();

 private:
  EventLogger* logger_;
  linked_ptr<base::Closure> callback_;

  DISALLOW_COPY_AND_ASSIGN(LoggingWorkScheduler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_LOGGING_WORK_SCHEDULER_H_
