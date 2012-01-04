// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DoneTask : public Task {
 public:
  DoneTask(int* run_count, bool* was_deleted)
      : run_count_(run_count), was_deleted_(was_deleted) {
  }
  virtual ~DoneTask() {
    *was_deleted_ = true;
  }

  virtual void Run() {
    ++(*run_count_);
  }

  int* run_count_;
  bool* was_deleted_;
};

TEST(TaskTest, TestScopedTaskRunnerExitScope) {
  int run_count = 0;
  bool was_deleted = false;
  {
    base::ScopedTaskRunner runner(new DoneTask(&run_count, &was_deleted));
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(1, run_count);
  EXPECT_TRUE(was_deleted);
}

TEST(TaskTest, TestScopedTaskRunnerRelease) {
  int run_count = 0;
  bool was_deleted = false;
  {
    base::ScopedTaskRunner runner(new DoneTask(&run_count, &was_deleted));
    delete runner.Release();
    EXPECT_TRUE(was_deleted);
  }
  EXPECT_EQ(0, run_count);
}

TEST(TaskTest, TestScopedTaskRunnerManualRun) {
  int run_count = 0;
  Task* done_task = NULL;
  bool was_deleted = false;
  {
    base::ScopedTaskRunner runner(new DoneTask(&run_count, &was_deleted));
    done_task = runner.Release();
    EXPECT_TRUE(NULL != done_task);
    EXPECT_FALSE(was_deleted);
    EXPECT_EQ(0, run_count);
    done_task->Run();
    EXPECT_FALSE(was_deleted);
    EXPECT_EQ(1, run_count);
  }
  EXPECT_EQ(1, run_count);
  delete done_task;
  EXPECT_TRUE(was_deleted);
}

void Increment(int* value) {
  (*value)++;
}

TEST(TaskTest, TestScopedClosureRunnerExitScope) {
  int run_count = 0;
  {
    base::ScopedClosureRunner runner(base::Bind(&Increment, &run_count));
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(1, run_count);
}

TEST(TaskTest, TestScopedClosureRunnerRelease) {
  int run_count = 0;
  base::Closure c;
  {
    base::ScopedClosureRunner runner(base::Bind(&Increment, &run_count));
    c = runner.Release();
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(0, run_count);
  c.Run();
  EXPECT_EQ(1, run_count);
}

}  // namespace
