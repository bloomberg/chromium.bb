// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/worker_pool.h"

#include <vector>

#include "cc/base/completion_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

class FakeTaskImpl : public internal::WorkerPoolTask {
 public:
  FakeTaskImpl(const base::Closure& callback,
               const base::Closure& reply,
               internal::WorkerPoolTask::TaskVector* dependencies)
      : internal::WorkerPoolTask(dependencies),
        callback_(callback),
        reply_(reply) {
  }
  FakeTaskImpl(const base::Closure& callback, const base::Closure& reply)
      : callback_(callback),
        reply_(reply) {
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnThread(unsigned thread_index) OVERRIDE {
    if (!callback_.is_null())
      callback_.Run();
  }
  virtual void DispatchCompletionCallback() OVERRIDE {
    if (!reply_.is_null())
      reply_.Run();
  }

 private:
  virtual ~FakeTaskImpl() {}

  const base::Closure callback_;
  const base::Closure reply_;
};

class FakeWorkerPool : public WorkerPool {
 public:
  FakeWorkerPool() : WorkerPool(1, "test") {}
  virtual ~FakeWorkerPool() {}

  static scoped_ptr<FakeWorkerPool> Create() {
    return make_scoped_ptr(new FakeWorkerPool);
  }

  void ScheduleTasks(const base::Closure& callback,
                     const base::Closure& reply,
                     const base::Closure& dependency,
                     int count) {
    scoped_refptr<FakeTaskImpl> dependency_task(
        new FakeTaskImpl(dependency, base::Closure()));

    internal::WorkerPoolTask::TaskVector tasks;
    for (int i = 0; i < count; ++i) {
      internal::WorkerPoolTask::TaskVector dependencies(1, dependency_task);
      tasks.push_back(new FakeTaskImpl(callback, reply, &dependencies));
    }
    scoped_refptr<FakeTaskImpl> completion_task(
        new FakeTaskImpl(base::Bind(&FakeWorkerPool::OnTasksCompleted,
                                    base::Unretained(this)),
                         base::Closure(),
                         &tasks));

    scheduled_tasks_completion_.reset(new CompletionEvent);

    TaskGraph graph;
    BuildTaskGraph(completion_task.get(), &graph);
    WorkerPool::SetTaskGraph(&graph);
    root_.swap(completion_task);
  }

  void WaitForTasksToComplete() {
    DCHECK(scheduled_tasks_completion_);
    scheduled_tasks_completion_->Wait();
  }

 private:
  void OnTasksCompleted() {
    DCHECK(scheduled_tasks_completion_);
    scheduled_tasks_completion_->Signal();
  }

  scoped_refptr<FakeTaskImpl> root_;
  scoped_ptr<CompletionEvent> scheduled_tasks_completion_;
};

class WorkerPoolTest : public testing::Test {
 public:
  WorkerPoolTest() {}
  virtual ~WorkerPoolTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    Reset();
  }
  virtual void TearDown() OVERRIDE {
    worker_pool_->Shutdown();
  }

  void Reset() {
    worker_pool_ = FakeWorkerPool::Create();
  }

  void RunAllTasksAndReset() {
    worker_pool_->WaitForTasksToComplete();
    worker_pool_->Shutdown();
    worker_pool_->CheckForCompletedTasks();
    Reset();
  }

  FakeWorkerPool* worker_pool() {
    return worker_pool_.get();
  }

  void RunTask(unsigned id) {
    run_task_ids_.push_back(id);
  }

  void OnTaskCompleted(unsigned id) {
    on_task_completed_ids_.push_back(id);
  }

  const std::vector<unsigned>& run_task_ids() {
    return run_task_ids_;
  }

  const std::vector<unsigned>& on_task_completed_ids() {
    return on_task_completed_ids_;
  }

 private:
  scoped_ptr<FakeWorkerPool> worker_pool_;
  std::vector<unsigned> run_task_ids_;
  std::vector<unsigned> on_task_completed_ids_;
};

TEST_F(WorkerPoolTest, Basic) {
  EXPECT_EQ(0u, run_task_ids().size());
  EXPECT_EQ(0u, on_task_completed_ids().size());

  worker_pool()->ScheduleTasks(
      base::Bind(&WorkerPoolTest::RunTask, base::Unretained(this), 0u),
      base::Bind(&WorkerPoolTest::OnTaskCompleted, base::Unretained(this), 0u),
      base::Closure(),
      1);
  RunAllTasksAndReset();

  EXPECT_EQ(1u, run_task_ids().size());
  EXPECT_EQ(1u, on_task_completed_ids().size());

  worker_pool()->ScheduleTasks(
      base::Bind(&WorkerPoolTest::RunTask, base::Unretained(this), 0u),
      base::Bind(&WorkerPoolTest::OnTaskCompleted, base::Unretained(this), 0u),
      base::Closure(),
      2);
  RunAllTasksAndReset();

  EXPECT_EQ(3u, run_task_ids().size());
  EXPECT_EQ(3u, on_task_completed_ids().size());
}

TEST_F(WorkerPoolTest, Dependencies) {
  worker_pool()->ScheduleTasks(
      base::Bind(&WorkerPoolTest::RunTask, base::Unretained(this), 1u),
      base::Bind(&WorkerPoolTest::OnTaskCompleted, base::Unretained(this), 1u),
      base::Bind(&WorkerPoolTest::RunTask, base::Unretained(this), 0u),
      1);
  RunAllTasksAndReset();

  // Check if dependency ran before task.
  ASSERT_EQ(2u, run_task_ids().size());
  EXPECT_EQ(0u, run_task_ids()[0]);
  EXPECT_EQ(1u, run_task_ids()[1]);
  ASSERT_EQ(1u, on_task_completed_ids().size());
  EXPECT_EQ(1u, on_task_completed_ids()[0]);

  worker_pool()->ScheduleTasks(
      base::Bind(&WorkerPoolTest::RunTask, base::Unretained(this), 1u),
      base::Bind(&WorkerPoolTest::OnTaskCompleted, base::Unretained(this), 1u),
      base::Bind(&WorkerPoolTest::RunTask, base::Unretained(this), 0u),
      2);
  RunAllTasksAndReset();

  // Dependency should only run once.
  ASSERT_EQ(5u, run_task_ids().size());
  EXPECT_EQ(0u, run_task_ids()[2]);
  EXPECT_EQ(1u, run_task_ids()[3]);
  EXPECT_EQ(1u, run_task_ids()[4]);
  ASSERT_EQ(3u, on_task_completed_ids().size());
  EXPECT_EQ(1u, on_task_completed_ids()[1]);
  EXPECT_EQ(1u, on_task_completed_ids()[2]);
}

}  // namespace

}  // namespace cc
