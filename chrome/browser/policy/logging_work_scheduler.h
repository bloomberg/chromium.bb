// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_LOGGING_WORK_SCHEDULER_H_
#define CHROME_BROWSER_POLICY_LOGGING_WORK_SCHEDULER_H_
#pragma once

#include <queue>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/task.h"
#include "chrome/browser/policy/delayed_work_scheduler.h"

// Utilities for testing users of DelayedWorkScheduler. There are no
// thread-safety guarantees for the classes in this file. They expect to
// only be called from the UI thread and issue callbacks on that very same
// thread.
//
// Usage examples:
//
// Making CloudPolicyController and/or DeviceTokenFetcher run without real-time
// delays in tests:
//
//   DeviceTokenFetcher fetcher(..., new DummyDelayedWorkScheduler);
//
// Running CloudPolicyController and/or DeviceTokenFetcher in a simulated
// environment, in which the time of any of their actions can be recorded,
// but without having to wait for the real-time delays:
//
//   EventLogger logger;
//   DeviceTokenFetcher fetcher(..., new LoggingEventScheduler(&logger));
//   CloudPolicyController controller(..., new LoggingEventScheduler(&logger));
//
// Start the policy subsystem, and use logger.RegisterEvent() in case of
// any interesting events. The time of all these events will be recorded
// by |logger|. After that, the results can be extracted easily:
//
//   std::vector<int64> logged_events;
//   logger.Swap(&logged_events);
//
// Each element of |logged_events| corresponds to a logger event, and stores
// the virtual time when it was logged. Events are in ascending order.

namespace policy {

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
  ~EventLogger();

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
  class Task;

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
  // An EventLogger may be shared by more than one schedulers, therefore
  // no ownership is taken.
  explicit LoggingWorkScheduler(EventLogger* logger);
  virtual ~LoggingWorkScheduler();

  virtual void PostDelayedWork(const base::Closure& callback, int64 delay)
      OVERRIDE;
  virtual void CancelDelayedWork() OVERRIDE;

 private:
  EventLogger* logger_;
  linked_ptr<base::Closure> callback_;

  DISALLOW_COPY_AND_ASSIGN(LoggingWorkScheduler);
};

// This implementation of DelayedWorkScheduler always schedules the tasks
// with zero delay.
class DummyWorkScheduler : public DelayedWorkScheduler {
 public:
  DummyWorkScheduler();
  virtual ~DummyWorkScheduler();

  virtual void PostDelayedWork(const base::Closure& callback, int64 delay)
      OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyWorkScheduler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_LOGGING_WORK_SCHEDULER_H_
