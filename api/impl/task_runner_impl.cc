// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "api/impl/task_runner_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {
TaskRunnerImpl::TaskRunnerImpl(platform::ClockNowFunctionPtr now_function)
    : now_function_(now_function), is_running_(false) {}

TaskRunnerImpl::~TaskRunnerImpl() = default;

void TaskRunnerImpl::PostTask(Task task) {
  std::lock_guard<std::mutex> lock(task_mutex_);
  tasks_.push_back(std::move(task));
  run_loop_wakeup_.notify_one();
}

void TaskRunnerImpl::PostTaskWithDelay(Task task, Clock::duration delay) {
  std::lock_guard<std::mutex> lock(task_mutex_);
  delayed_tasks_.emplace(std::move(task), now_function_() + delay);
  run_loop_wakeup_.notify_one();
}

void TaskRunnerImpl::RunUntilStopped() {
  const bool was_running = is_running_.exchange(true);
  OSP_CHECK(!was_running);

  RunTasksUntilStopped();
}

void TaskRunnerImpl::RequestStopSoon() {
  const bool was_running = is_running_.exchange(false);

  if (was_running) {
    std::lock_guard<std::mutex> lock(task_mutex_);
    run_loop_wakeup_.notify_one();
  }
}

void TaskRunnerImpl::RunUntilIdleForTesting() {
  ScheduleDelayedTasks();
  RunCurrentTasksForTesting();
}

void TaskRunnerImpl::RunCurrentTasksForTesting() {
  std::deque<Task> current_tasks;
  {
    // Unlike in the RunCurrentTasksBlocking method, here we just immediately
    // take the lock and drain the tasks_ queue. This allows tests to avoid
    // having to do any multithreading to interact with the queue.
    std::unique_lock<std::mutex> lock(task_mutex_);
    tasks_.swap(current_tasks);
  }

  for (Task& task : current_tasks) {
    task();
  }
}

void TaskRunnerImpl::RunCurrentTasksBlocking() {
  std::deque<Task> current_tasks;
  {
    // Wait for the lock. If there are no current tasks, we will wait until
    // a delayed task is ready or a task gets added to the queue.
    auto lock = WaitForWorkAndAcquireLock();
    if (!lock) {
      return;
    }

    tasks_.swap(current_tasks);
  }

  for (Task& task : current_tasks) {
    task();
  }
}

void TaskRunnerImpl::RunTasksUntilStopped() {
  while (is_running_) {
    ScheduleDelayedTasks();
    RunCurrentTasksBlocking();
  }
}

void TaskRunnerImpl::ScheduleDelayedTasks() {
  std::lock_guard<std::mutex> lock(task_mutex_);

  // Getting the time can be expensive on some platforms, so only get it once.
  const auto current_time = now_function_();
  while (!delayed_tasks_.empty() &&
         (delayed_tasks_.top().runnable_after < current_time)) {
    tasks_.push_back(std::move(delayed_tasks_.top().task));
    delayed_tasks_.pop();
  }
}

std::unique_lock<std::mutex> TaskRunnerImpl::WaitForWorkAndAcquireLock() {
  // These checks are redundant, as they are a subset of predicates in the
  // below wait predicate. However, this is more readable and a slight
  // optimization, as we don't need to take a lock if we aren't running.
  if (!is_running_) {
    return {};
  }

  std::unique_lock<std::mutex> lock(task_mutex_);
  if (!tasks_.empty()) {
    return lock;
  }

  // Pass a wait predicate to avoid lost or spurious wakeups.
  const auto wait_predicate = [this] {
    // Either we got woken up because we aren't running
    // (probably just to end the thread), or we are running and have tasks to
    // do or schedule.
    return !this->is_running_ || !this->tasks_.empty() ||
           !this->delayed_tasks_.empty();
  };

  // TODO(jophba): find a predicate method that is compatible with our
  // fake clock, for easier use with testing.
  // We don't have any work to do currently, but know we have some in the pipe.
  if (!delayed_tasks_.empty()) {
    run_loop_wakeup_.wait_until(lock, delayed_tasks_.top().runnable_after,
                                wait_predicate);

    // We don't have any work queued.
  } else if (tasks_.empty()) {
    run_loop_wakeup_.wait(lock, wait_predicate);
  }

  return lock;
}
}  // namespace platform
}  // namespace openscreen
