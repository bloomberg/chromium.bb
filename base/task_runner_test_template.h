// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class defines tests that implementations of TaskRunner should
// pass in order to be conformant.  Here's how you use it to test your
// implementation.
//
// Say your class is called MyTaskRunner.  Then you need to define a
// class called MyTaskRunnerTestDelegate in my_task_runner_unittest.cc
// like this:
//
//   class MyTaskRunnerTestDelegate {
//    public:
//     // Tasks posted to the task runner after this and before
//     // StopTaskRunner() is called is called should run successfully.
//     void StartTaskRunner() {
//       ...
//     }
//
//     // Should return the task runner implementation.  Only called
//     // after StartTaskRunner and before StopTaskRunner.
//     scoped_refptr<MyTaskRunner> GetTaskRunner() {
//       ...
//     }
//
//     // Stop the task runner and make sure all tasks posted before
//     // this is called are run.
//     void StopTaskRunner() {
//       ...
//     }
//
//     // Returns whether or not the task runner obeys non-zero delays.
//     bool TaskRunnerHandlesNonZeroDelays() const {
//       return true;
//     }
//   };
//
// The TaskRunnerTest test harness will have a member variable of
// this delegate type and will call its functions in the various
// tests.
//
// Then you simply #include this file as well as gtest.h and add the
// following statement to my_task_runner_unittest.cc:
//
//   INSTANTIATE_TYPED_TEST_CASE_P(
//       MyTaskRunner, TaskRunnerTest, MyTaskRunnerTestDelegate);
//
// Easy!

#ifndef BASE_TASK_RUNNER_TEST_TEMPLATE_H_
#define BASE_TASK_RUNNER_TEST_TEMPLATE_H_
#pragma once

#include <cstddef>
#include <map>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "base/tracked_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Utility class used in the tests below.
class TaskTracker : public RefCountedThreadSafe<TaskTracker> {
 public:
  TaskTracker();

  void RunTask(int i);

  std::map<int, int> GetTaskRunCounts() const;

 private:
  friend class RefCountedThreadSafe<TaskTracker>;

  ~TaskTracker();

  mutable Lock task_run_counts_lock_;
  std::map<int, int> task_run_counts_;

  DISALLOW_COPY_AND_ASSIGN(TaskTracker);
};

template <typename TaskRunnerTestDelegate>
class TaskRunnerTest : public testing::Test {
 protected:
  TaskRunnerTest() : task_tracker_(new TaskTracker()) {}

  const scoped_refptr<TaskTracker> task_tracker_;
  TaskRunnerTestDelegate delegate_;
};

TYPED_TEST_CASE_P(TaskRunnerTest);

// We can't really test much, since TaskRunner provides very few
// guarantees.

// Post a bunch of tasks to the task runner.  They should all
// complete.
TYPED_TEST_P(TaskRunnerTest, Basic) {
  std::map<int, int> expected_task_run_counts;

  this->delegate_.StartTaskRunner();
  scoped_refptr<TaskRunner> task_runner = this->delegate_.GetTaskRunner();
  // Post each ith task i+1 times.
  for (int i = 0; i < 20; ++i) {
    Closure task = Bind(&TaskTracker::RunTask, this->task_tracker_, i);
    for (int j = 0; j < i + 1; ++j) {
      task_runner->PostTask(FROM_HERE, task);
      ++expected_task_run_counts[i];
    }
  }
  this->delegate_.StopTaskRunner();

  EXPECT_EQ(expected_task_run_counts,
            this->task_tracker_->GetTaskRunCounts());
}

// Post a bunch of delayed tasks to the task runner.  They should all
// complete.
TYPED_TEST_P(TaskRunnerTest, Delayed) {
  if (!this->delegate_.TaskRunnerHandlesNonZeroDelays()) {
    DLOG(INFO) << "This TaskRunner doesn't handle non-zero delays; skipping";
    return;
  }

  std::map<int, int> expected_task_run_counts;

  this->delegate_.StartTaskRunner();
  scoped_refptr<TaskRunner> task_runner = this->delegate_.GetTaskRunner();
  // Post each ith task i+1 times with delays from 0-i.
  for (int i = 0; i < 20; ++i) {
    Closure task = Bind(&TaskTracker::RunTask, this->task_tracker_, i);
    for (int j = 0; j < i + 1; ++j) {
      task_runner->PostDelayedTask(FROM_HERE, task, j);
      ++expected_task_run_counts[i];
    }
  }
  this->delegate_.StopTaskRunner();

  EXPECT_EQ(expected_task_run_counts,
            this->task_tracker_->GetTaskRunCounts());
}

// TODO(akalin): Add test to verify RunsTaskOnCurrentThread() returns
// true for tasks runs on the TaskRunner and returns false on a
// separate PlatformThread.

REGISTER_TYPED_TEST_CASE_P(TaskRunnerTest, Basic, Delayed);

}  // namespace base

#endif  //#define BASE_TASK_RUNNER_TEST_TEMPLATE_H_
