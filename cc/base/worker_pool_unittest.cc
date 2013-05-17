// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/worker_pool.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

class WorkerPoolTest : public testing::Test,
                       public WorkerPoolClient {
 public:
  WorkerPoolTest()
      : run_task_count_(0),
        on_task_completed_count_(0),
        finish_dispatching_completion_callbacks_count_(0) {
  }
  virtual ~WorkerPoolTest() {
  }

  virtual void SetUp() OVERRIDE {
    Reset();
  }

  virtual void TearDown() OVERRIDE {
    worker_pool_->Shutdown();
  }

  // Overridden from WorkerPoolClient:
  virtual void DidFinishDispatchingWorkerPoolCompletionCallbacks() OVERRIDE {
    ++finish_dispatching_completion_callbacks_count_;
  }

  void Reset() {
    worker_pool_ = WorkerPool::Create(this,
                                      1,
                                      base::TimeDelta::FromDays(1024),
                                      "test");
  }

  void RunAllTasksAndReset() {
    worker_pool_->Shutdown();
    Reset();
  }

  WorkerPool* worker_pool() {
    return worker_pool_.get();
  }

  void RunTask() {
    ++run_task_count_;
  }

  void OnTaskCompleted() {
    ++on_task_completed_count_;
  }

  unsigned run_task_count() {
    return run_task_count_;
  }

  unsigned on_task_completed_count() {
    return on_task_completed_count_;
  }

  unsigned finish_dispatching_completion_callbacks_count() {
    return finish_dispatching_completion_callbacks_count_;
  }

 private:
  scoped_ptr<WorkerPool> worker_pool_;
  unsigned run_task_count_;
  unsigned on_task_completed_count_;
  unsigned finish_dispatching_completion_callbacks_count_;
};

TEST_F(WorkerPoolTest, Basic) {
  EXPECT_EQ(0u, run_task_count());
  EXPECT_EQ(0u, on_task_completed_count());
  EXPECT_EQ(0u, finish_dispatching_completion_callbacks_count());

  worker_pool()->PostTaskAndReply(
      base::Bind(&WorkerPoolTest::RunTask, base::Unretained(this)),
      base::Bind(&WorkerPoolTest::OnTaskCompleted, base::Unretained(this)));
  RunAllTasksAndReset();

  EXPECT_EQ(1u, run_task_count());
  EXPECT_EQ(1u, on_task_completed_count());
  EXPECT_EQ(1u, finish_dispatching_completion_callbacks_count());

  worker_pool()->PostTaskAndReply(
      base::Bind(&WorkerPoolTest::RunTask, base::Unretained(this)),
      base::Bind(&WorkerPoolTest::OnTaskCompleted, base::Unretained(this)));
  worker_pool()->PostTaskAndReply(
      base::Bind(&WorkerPoolTest::RunTask, base::Unretained(this)),
      base::Bind(&WorkerPoolTest::OnTaskCompleted, base::Unretained(this)));
  RunAllTasksAndReset();

  EXPECT_EQ(3u, run_task_count());
  EXPECT_EQ(3u, on_task_completed_count());
  EXPECT_EQ(2u, finish_dispatching_completion_callbacks_count());
}

}  // namespace

}  // namespace cc
