// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_TASK_RUNNER_IMPL_H_
#define API_IMPL_TASK_RUNNER_IMPL_H_

#include <atomic>
#include <condition_variable>  // NOLINT
#include <deque>
#include <functional>
#include <memory>
#include <mutex>  // NOLINT
#include <queue>
#include <thread>  // NOLINT
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/types/optional.h"
#include "api/public/task_runner.h"
#include "platform/api/time.h"

namespace openscreen {
namespace platform {

class TaskRunnerImpl : public TaskRunner {
 public:
  explicit TaskRunnerImpl(platform::ClockNowFunctionPtr now_function);

  // TaskRunner overrides
  ~TaskRunnerImpl() override;
  void PostTask(Task task) override;
  void PostTaskWithDelay(Task task, Clock::duration delay) override;

  // Tasks will only be executed if RunUntilStopped has been called, and
  // RequestStopSoon has not. Important note: TaskRunnerImpl does NOT do any
  // threading, so calling "RunUntilStopped()" will block whatever thread you
  // are calling it on.
  void RunUntilStopped();

  // Thread-safe method for requesting the TaskRunnerImpl to stop running. This
  // sets a flag that will get checked in the run loop, typically after
  // completing the current task.
  void RequestStopSoon();

  // Execute all tasks immediately, useful for testing only. Note: this method
  // will schedule any delayed tasks that are ready to run, but does not block
  // waiting for delayed tasks to become eligible.
  void RunUntilIdleForTesting();

 private:
  struct DelayedTask {
    DelayedTask(Task task_, Clock::time_point runnable_after_)
        : task(std::move(task_)), runnable_after(runnable_after_) {}

    Task task;
    Clock::time_point runnable_after;

    bool operator>(const DelayedTask& other) const {
      return this->runnable_after > other.runnable_after;
    }
  };

  // Run all tasks already in the task queue. If the queue is empty, wait for
  // either (1) a delayed task to become available, or (2) a task to be added
  // to the queue.
  void RunCurrentTasksBlocking();

  // Run tasks already in the queue, for testing. If the queue is empty, this
  // method does not block but instead returns immediately.
  void RunCurrentTasksForTesting();

  // Loop method that runs tasks in the current thread, until the
  // RequestStopSoon method is called.
  void RunTasksUntilStopped();

  // Look at all tasks in the delayed task queue, then schedule them if the
  // minimum delay time has elapsed.
  void ScheduleDelayedTasks();

  // Takes the task_mutex_ lock, returning immediately if work is available. If
  // no work is available, this places the task running thread into a waiting
  // state until we stop running or work is available.
  std::unique_lock<std::mutex> WaitForWorkAndAcquireLock();

  const platform::ClockNowFunctionPtr now_function_;

  // Atomic so that we can perform atomic exchanges.
  std::atomic_bool is_running_;

  // This mutex is used for both tasks_ and notifying the run loop to wake
  // up when it is waiting for a task to be added to the queue in
  // run_loop_wakeup_.
  std::mutex task_mutex_;
  std::priority_queue<DelayedTask,
                      std::vector<DelayedTask>,
                      std::greater<DelayedTask>>
      delayed_tasks_ GUARDED_BY(task_mutex_);

  std::condition_variable run_loop_wakeup_;
  std::deque<Task> tasks_ GUARDED_BY(task_mutex_);
};
}  // namespace platform
}  // namespace openscreen

#endif  // API_IMPL_TASK_RUNNER_IMPL_H_
