// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CancelInDestructor : public base::RefCounted<CancelInDestructor> {
 public:
  CancelInDestructor() : cancelable_task_(NULL) {}

  void Start() {
    if (cancelable_task_) {
      ADD_FAILURE();
      return;
    }
    AddRef();
    cancelable_task_ = NewRunnableMethod(
        this, &CancelInDestructor::NeverIssuedCallback);
    Release();
  }

  CancelableTask* cancelable_task() {
    return cancelable_task_;
  }

 private:
  friend class base::RefCounted<CancelInDestructor>;

  ~CancelInDestructor() {
    if (cancelable_task_)
      cancelable_task_->Cancel();
  }

  void NeverIssuedCallback() { NOTREACHED(); }

  CancelableTask* cancelable_task_;
};

TEST(TaskTest, TestCancelInDestructor) {
  // Intentionally not using a scoped_refptr for cancel_in_destructor.
  CancelInDestructor* cancel_in_destructor = new CancelInDestructor();
  cancel_in_destructor->Start();
  CancelableTask* cancelable_task = cancel_in_destructor->cancelable_task();
  ASSERT_TRUE(cancelable_task);
  delete cancelable_task;
}

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
