// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_mock_time_task_runner.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"

namespace base {

namespace {

// TickClock that always returns the then-current mock time of |task_runner| as
// the current time.
class MockTickClock : public TickClock {
 public:
  explicit MockTickClock(
      scoped_refptr<const TestMockTimeTaskRunner> task_runner);
  ~MockTickClock() override;

  // TickClock:
  TimeTicks NowTicks() override;

 private:
  scoped_refptr<const TestMockTimeTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockTickClock);
};

MockTickClock::MockTickClock(
    scoped_refptr<const TestMockTimeTaskRunner> task_runner)
    : task_runner_(task_runner) {
}

MockTickClock::~MockTickClock() {
}

TimeTicks MockTickClock::NowTicks() {
  return task_runner_->GetCurrentMockTime();
}

}  // namespace

bool TestMockTimeTaskRunner::TemporalOrder::operator()(
    const TestPendingTask& first_task,
    const TestPendingTask& second_task) const {
  return first_task.GetTimeToRun() > second_task.GetTimeToRun();
}

TestMockTimeTaskRunner::TestMockTimeTaskRunner() {
}

TestMockTimeTaskRunner::~TestMockTimeTaskRunner() {
}

void TestMockTimeTaskRunner::FastForwardBy(TimeDelta delta) {
  DCHECK(thread_checker_.CalledOnValidThread());

  OnBeforeSelectingTask();

  const base::TimeTicks original_now = now_;
  TestPendingTask task_info;
  while (DequeueNextTask(original_now, delta, &task_info)) {
    if (task_info.GetTimeToRun() - now_ > base::TimeDelta()) {
      now_ = task_info.GetTimeToRun();
      OnAfterTimePassed();
    }

    task_info.task.Run();

    OnAfterTaskRun();
    OnBeforeSelectingTask();
  }

  if (!delta.is_max() && now_ - original_now < delta) {
    now_ = original_now + delta;
    OnAfterTimePassed();
  }
}

void TestMockTimeTaskRunner::RunUntilIdle() {
  FastForwardBy(TimeDelta());
}

void TestMockTimeTaskRunner::FastForwardUntilNoTasksRemain() {
  FastForwardBy(TimeDelta::Max());
}

TimeTicks TestMockTimeTaskRunner::GetCurrentMockTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return now_;
}

scoped_ptr<TickClock> TestMockTimeTaskRunner::GetMockTickClock() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return make_scoped_ptr(new MockTickClock(this));
}

bool TestMockTimeTaskRunner::HasPendingTask() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return !tasks_.empty();
}

size_t TestMockTimeTaskRunner::GetPendingTaskCount() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return tasks_.size();
}

TimeDelta TestMockTimeTaskRunner::NextPendingTaskDelay() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return tasks_.empty() ? TimeDelta::Max() : tasks_.top().GetTimeToRun() - now_;
}

bool TestMockTimeTaskRunner::RunsTasksOnCurrentThread() const {
  return thread_checker_.CalledOnValidThread();
}

bool TestMockTimeTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const Closure& task,
    TimeDelta delay) {
  base::AutoLock scoped_lock(tasks_lock_);
  tasks_.push(
      TestPendingTask(from_here, task, now_, delay, TestPendingTask::NESTABLE));
  return true;
}

bool TestMockTimeTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const Closure& task,
    TimeDelta delay) {
  NOTREACHED();
  return false;
}

void TestMockTimeTaskRunner::OnBeforeSelectingTask() {
  // Empty default implementation.
}

void TestMockTimeTaskRunner::OnAfterTimePassed() {
  // Empty default implementation.
}

void TestMockTimeTaskRunner::OnAfterTaskRun() {
  // Empty default implementation.
}

bool TestMockTimeTaskRunner::DequeueNextTask(const base::TimeTicks& reference,
                                             const base::TimeDelta& max_delta,
                                             TestPendingTask* next_task) {
  base::AutoLock scoped_lock(tasks_lock_);
  if (!tasks_.empty() &&
      (tasks_.top().GetTimeToRun() - reference) <= max_delta) {
    *next_task = tasks_.top();
    tasks_.pop();
    return true;
  }
  return false;
}

}  // namespace base
