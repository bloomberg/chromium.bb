// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/worker_pool.h"

#include <vector>

#include "cc/base/completion_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

class FakeWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  FakeWorkerPoolTaskImpl(const base::Closure& callback,
                         const base::Closure& reply)
      : callback_(callback),
        reply_(reply) {
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    if (!callback_.is_null())
      callback_.Run();
  }
  virtual void CompleteOnOriginThread() OVERRIDE {
    if (!reply_.is_null())
      reply_.Run();
  }

 private:
  virtual ~FakeWorkerPoolTaskImpl() {}

  const base::Closure callback_;
  const base::Closure reply_;

  DISALLOW_COPY_AND_ASSIGN(FakeWorkerPoolTaskImpl);
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
    unsigned priority = 0u;
    TaskGraph graph;

    scoped_refptr<FakeWorkerPoolTaskImpl> completion_task(
        new FakeWorkerPoolTaskImpl(
            base::Bind(&FakeWorkerPool::OnTasksCompleted,
                       base::Unretained(this)),
            base::Closure()));
    scoped_ptr<GraphNode> completion_node(new GraphNode);
    completion_node->set_task(completion_task.get());

    scoped_refptr<FakeWorkerPoolTaskImpl> dependency_task(
        new FakeWorkerPoolTaskImpl(dependency, base::Closure()));
    scoped_ptr<GraphNode> dependency_node(new GraphNode);
    dependency_node->set_task(dependency_task.get());

    TaskVector tasks;
    for (int i = 0; i < count; ++i) {
      scoped_refptr<FakeWorkerPoolTaskImpl> task(
          new FakeWorkerPoolTaskImpl(callback, reply));
      scoped_ptr<GraphNode> node(new GraphNode);
      node->set_task(task.get());
      node->add_dependent(completion_node.get());
      completion_node->add_dependency();
      dependency_node->add_dependent(node.get());
      node->add_dependency();
      node->set_priority(priority++);
      graph.set(task.get(), node.Pass());
      tasks.push_back(task.get());
    }

    completion_node->set_priority(priority++);
    graph.set(completion_task.get(), completion_node.Pass());
    dependency_node->set_priority(priority++);
    graph.set(dependency_task.get(), dependency_node.Pass());

    scheduled_tasks_completion_.reset(new CompletionEvent);

    SetTaskGraph(&graph);

    tasks_.swap(tasks);
    completion_task_.swap(completion_task);
    dependency_task_.swap(dependency_task);
  }

  void WaitForTasksToComplete() {
    DCHECK(scheduled_tasks_completion_);
    scheduled_tasks_completion_->Wait();
  }

 private:
  typedef std::vector<scoped_refptr<internal::WorkerPoolTask> > TaskVector;

  void OnTasksCompleted() {
    DCHECK(scheduled_tasks_completion_);
    scheduled_tasks_completion_->Signal();
  }

  TaskVector tasks_;
  scoped_refptr<FakeWorkerPoolTaskImpl> completion_task_;
  scoped_refptr<FakeWorkerPoolTaskImpl> dependency_task_;
  scoped_ptr<CompletionEvent> scheduled_tasks_completion_;

  DISALLOW_COPY_AND_ASSIGN(FakeWorkerPool);
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
