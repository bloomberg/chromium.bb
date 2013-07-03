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
  struct Task {
    Task(const base::Closure& callback,
         const base::Closure& reply,
         const base::Closure& dependent,
         unsigned dependent_count,
         unsigned priority) : callback(callback),
                              reply(reply),
                              dependent(dependent),
                              dependent_count(dependent_count),
                              priority(priority) {
    }

    base::Closure callback;
    base::Closure reply;
    base::Closure dependent;
    unsigned dependent_count;
    unsigned priority;
  };
  FakeWorkerPool() : WorkerPool(1, "test") {}
  virtual ~FakeWorkerPool() {}

  static scoped_ptr<FakeWorkerPool> Create() {
    return make_scoped_ptr(new FakeWorkerPool);
  }

  void ScheduleTasks(const std::vector<Task>& tasks) {
    TaskVector new_tasks;
    TaskVector new_dependents;
    TaskGraph new_graph;

    scoped_refptr<FakeWorkerPoolTaskImpl> new_completion_task(
        new FakeWorkerPoolTaskImpl(
            base::Bind(&FakeWorkerPool::OnTasksCompleted,
                       base::Unretained(this)),
            base::Closure()));
    scoped_ptr<internal::GraphNode> completion_node(
        new internal::GraphNode(new_completion_task.get(), 0u));

    for (std::vector<Task>::const_iterator it = tasks.begin();
         it != tasks.end(); ++it) {
      scoped_refptr<FakeWorkerPoolTaskImpl> new_task(
          new FakeWorkerPoolTaskImpl(it->callback, it->reply));
      scoped_ptr<internal::GraphNode> node(
          new internal::GraphNode(new_task.get(), it->priority));

      DCHECK(it->dependent_count);
      for (unsigned i = 0; i < it->dependent_count; ++i) {
        scoped_refptr<FakeWorkerPoolTaskImpl> new_dependent_task(
            new FakeWorkerPoolTaskImpl(it->dependent, base::Closure()));
        scoped_ptr<internal::GraphNode> dependent_node(
            new internal::GraphNode(new_dependent_task.get(), it->priority));
        dependent_node->add_dependent(completion_node.get());
        completion_node->add_dependency();
        node->add_dependent(dependent_node.get());
        dependent_node->add_dependency();
        new_graph.set(new_dependent_task.get(), dependent_node.Pass());
        new_dependents.push_back(new_dependent_task.get());
      }

      new_graph.set(new_task.get(), node.Pass());
      new_tasks.push_back(new_task.get());
    }

    new_graph.set(new_completion_task.get(), completion_node.Pass());

    scheduled_tasks_completion_.reset(new CompletionEvent);

    SetTaskGraph(&new_graph);

    dependents_.swap(new_dependents);
    completion_task_.swap(new_completion_task);
    tasks_.swap(new_tasks);
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
  TaskVector dependents_;
  scoped_refptr<FakeWorkerPoolTaskImpl> completion_task_;
  scoped_ptr<CompletionEvent> scheduled_tasks_completion_;

  DISALLOW_COPY_AND_ASSIGN(FakeWorkerPool);
};

class WorkerPoolTest : public testing::Test {
 public:
  WorkerPoolTest() {}
  virtual ~WorkerPoolTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    worker_pool_ = FakeWorkerPool::Create();
  }
  virtual void TearDown() OVERRIDE {
    worker_pool_->Shutdown();
    worker_pool_->CheckForCompletedTasks();
  }

  void ResetIds() {
    run_task_ids_.clear();
    on_task_completed_ids_.clear();
  }

  void RunAllTasks() {
    worker_pool_->WaitForTasksToComplete();
    worker_pool_->CheckForCompletedTasks();
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
      std::vector<FakeWorkerPool::Task>(
          1,
          FakeWorkerPool::Task(base::Bind(&WorkerPoolTest::RunTask,
                                          base::Unretained(this),
                                          0u),
                               base::Bind(&WorkerPoolTest::OnTaskCompleted,
                                          base::Unretained(this),
                                          0u),
                               base::Closure(),
                               1u,
                               0u)));
  RunAllTasks();

  EXPECT_EQ(1u, run_task_ids().size());
  EXPECT_EQ(1u, on_task_completed_ids().size());

  worker_pool()->ScheduleTasks(
      std::vector<FakeWorkerPool::Task>(
          1,
          FakeWorkerPool::Task(base::Bind(&WorkerPoolTest::RunTask,
                                          base::Unretained(this),
                                          0u),
                               base::Bind(&WorkerPoolTest::OnTaskCompleted,
                                          base::Unretained(this),
                                          0u),
                               base::Bind(&WorkerPoolTest::RunTask,
                                          base::Unretained(this),
                                          0u),
                               1u,
                               0u)));
  RunAllTasks();

  EXPECT_EQ(3u, run_task_ids().size());
  EXPECT_EQ(2u, on_task_completed_ids().size());

  worker_pool()->ScheduleTasks(
      std::vector<FakeWorkerPool::Task>(
          1, FakeWorkerPool::Task(base::Bind(&WorkerPoolTest::RunTask,
                                             base::Unretained(this),
                                             0u),
                                  base::Bind(&WorkerPoolTest::OnTaskCompleted,
                                             base::Unretained(this),
                                             0u),
                                  base::Bind(&WorkerPoolTest::RunTask,
                                             base::Unretained(this),
                                             0u),
                                  2u,
                                  0u)));
  RunAllTasks();

  EXPECT_EQ(6u, run_task_ids().size());
  EXPECT_EQ(3u, on_task_completed_ids().size());
}

TEST_F(WorkerPoolTest, Dependencies) {
  worker_pool()->ScheduleTasks(
      std::vector<FakeWorkerPool::Task>(
          1, FakeWorkerPool::Task(base::Bind(&WorkerPoolTest::RunTask,
                                             base::Unretained(this),
                                             0u),
                                  base::Bind(&WorkerPoolTest::OnTaskCompleted,
                                             base::Unretained(this),
                                             0u),
                                  base::Bind(&WorkerPoolTest::RunTask,
                                             base::Unretained(this),
                                             1u),
                                  1u,
                                  0u)));
  RunAllTasks();

  // Check if task ran before dependent.
  ASSERT_EQ(2u, run_task_ids().size());
  EXPECT_EQ(0u, run_task_ids()[0]);
  EXPECT_EQ(1u, run_task_ids()[1]);
  ASSERT_EQ(1u, on_task_completed_ids().size());
  EXPECT_EQ(0u, on_task_completed_ids()[0]);

  worker_pool()->ScheduleTasks(
      std::vector<FakeWorkerPool::Task>(
          1, FakeWorkerPool::Task(base::Bind(&WorkerPoolTest::RunTask,
                                             base::Unretained(this),
                                             2u),
                                  base::Bind(&WorkerPoolTest::OnTaskCompleted,
                                             base::Unretained(this),
                                             2u),
                                  base::Bind(&WorkerPoolTest::RunTask,
                                             base::Unretained(this),
                                             3u),
                                  2u,
                                  0u)));
  RunAllTasks();

  // Task should only run once.
  ASSERT_EQ(5u, run_task_ids().size());
  EXPECT_EQ(2u, run_task_ids()[2]);
  EXPECT_EQ(3u, run_task_ids()[3]);
  EXPECT_EQ(3u, run_task_ids()[4]);
  ASSERT_EQ(2u, on_task_completed_ids().size());
  EXPECT_EQ(2u, on_task_completed_ids()[1]);
}

TEST_F(WorkerPoolTest, Priority) {
  {
    FakeWorkerPool::Task tasks[] = {
        FakeWorkerPool::Task(base::Bind(&WorkerPoolTest::RunTask,
                                        base::Unretained(this),
                                        0u),
                             base::Bind(&WorkerPoolTest::OnTaskCompleted,
                                        base::Unretained(this),
                                        0u),
                             base::Bind(&WorkerPoolTest::RunTask,
                                        base::Unretained(this),
                                        2u),
                             1u,
                             1u),  // Priority 1
        FakeWorkerPool::Task(base::Bind(&WorkerPoolTest::RunTask,
                                        base::Unretained(this),
                                        1u),
                             base::Bind(&WorkerPoolTest::OnTaskCompleted,
                                        base::Unretained(this),
                                        1u),
                             base::Bind(&WorkerPoolTest::RunTask,
                                        base::Unretained(this),
                                        3u),
                             1u,
                             0u)  // Priority 0
    };
    worker_pool()->ScheduleTasks(
        std::vector<FakeWorkerPool::Task>(tasks, tasks + arraysize(tasks)));
  }
  RunAllTasks();

  // Check if tasks ran in order of priority.
  ASSERT_EQ(4u, run_task_ids().size());
  EXPECT_EQ(1u, run_task_ids()[0]);
  EXPECT_EQ(3u, run_task_ids()[1]);
  EXPECT_EQ(0u, run_task_ids()[2]);
  EXPECT_EQ(2u, run_task_ids()[3]);
  ASSERT_EQ(2u, on_task_completed_ids().size());
  EXPECT_EQ(1u, on_task_completed_ids()[0]);
  EXPECT_EQ(0u, on_task_completed_ids()[1]);

  ResetIds();
  {
    std::vector<FakeWorkerPool::Task> tasks;
    tasks.push_back(
        FakeWorkerPool::Task(base::Bind(&WorkerPoolTest::RunTask,
                                        base::Unretained(this),
                                        0u),
                             base::Bind(&WorkerPoolTest::OnTaskCompleted,
                                        base::Unretained(this),
                                        0u),
                             base::Bind(&WorkerPoolTest::RunTask,
                                        base::Unretained(this),
                                        3u),
                             1u,    // 1 dependent
                             1u));  // Priority 1
    tasks.push_back(
        FakeWorkerPool::Task(base::Bind(&WorkerPoolTest::RunTask,
                                        base::Unretained(this),
                                        1u),
                             base::Bind(&WorkerPoolTest::OnTaskCompleted,
                                        base::Unretained(this),
                                        1u),
                             base::Bind(&WorkerPoolTest::RunTask,
                                        base::Unretained(this),
                                        4u),
                             2u,    // 2 dependents
                             1u));  // Priority 1
    tasks.push_back(
        FakeWorkerPool::Task(base::Bind(&WorkerPoolTest::RunTask,
                                        base::Unretained(this),
                                        2u),
                             base::Bind(&WorkerPoolTest::OnTaskCompleted,
                                        base::Unretained(this),
                                        2u),
                             base::Bind(&WorkerPoolTest::RunTask,
                                        base::Unretained(this),
                                        5u),
                             1u,    // 1 dependent
                             0u));  // Priority 0
    worker_pool()->ScheduleTasks(tasks);
  }
  RunAllTasks();

  // Check if tasks ran in order of priority and that task with more
  // dependents ran first when priority is the same.
  ASSERT_LE(3u, run_task_ids().size());
  EXPECT_EQ(2u, run_task_ids()[0]);
  EXPECT_EQ(5u, run_task_ids()[1]);
  EXPECT_EQ(1u, run_task_ids()[2]);
  ASSERT_EQ(3u, on_task_completed_ids().size());
  EXPECT_EQ(2u, on_task_completed_ids()[0]);
  EXPECT_EQ(1u, on_task_completed_ids()[1]);
  EXPECT_EQ(0u, on_task_completed_ids()[2]);
}

}  // namespace

}  // namespace cc
