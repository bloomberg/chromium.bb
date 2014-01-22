// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/task_graph_runner.h"

#include <vector>

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

const int kNamespaceCount = 3;

class TaskGraphRunnerTestBase {
 public:
  struct Task {
    Task(int namespace_index,
         unsigned id,
         unsigned dependent_id,
         unsigned dependent_count,
         unsigned priority) : namespace_index(namespace_index),
                              id(id),
                              dependent_id(dependent_id),
                              dependent_count(dependent_count),
                              priority(priority) {
    }

    int namespace_index;
    unsigned id;
    unsigned dependent_id;
    unsigned dependent_count;
    unsigned priority;
  };

  void ResetIds(int namespace_index) {
    run_task_ids_[namespace_index].clear();
    on_task_completed_ids_[namespace_index].clear();
  }

  void RunAllTasks(int namespace_index) {
    task_graph_runner_->WaitForTasksToFinishRunning(
        namespace_token_[namespace_index]);

    internal::Task::Vector completed_tasks;
    task_graph_runner_->CollectCompletedTasks(
        namespace_token_[namespace_index], &completed_tasks);
    for (internal::Task::Vector::const_iterator it = completed_tasks.begin();
         it != completed_tasks.end();
         ++it) {
      FakeTaskImpl* task = static_cast<FakeTaskImpl*>(it->get());
      task->CompleteOnOriginThread();
    }
  }

  void RunTask(int namespace_index, unsigned id) {
    run_task_ids_[namespace_index].push_back(id);
  }

  void OnTaskCompleted(int namespace_index, unsigned id) {
    on_task_completed_ids_[namespace_index].push_back(id);
  }

  const std::vector<unsigned>& run_task_ids(int namespace_index) {
    return run_task_ids_[namespace_index];
  }

  const std::vector<unsigned>& on_task_completed_ids(int namespace_index) {
    return on_task_completed_ids_[namespace_index];
  }

  void ScheduleTasks(int namespace_index, const std::vector<Task>& tasks) {
    internal::Task::Vector new_tasks;
    internal::Task::Vector new_dependents;
    internal::GraphNode::Map new_graph;

    for (std::vector<Task>::const_iterator it = tasks.begin();
         it != tasks.end(); ++it) {
      scoped_refptr<FakeTaskImpl> new_task(
        new FakeTaskImpl(this, it->namespace_index, it->id));
      scoped_ptr<internal::GraphNode> node(
          new internal::GraphNode(new_task.get(), it->priority));

      for (unsigned i = 0; i < it->dependent_count; ++i) {
        scoped_refptr<FakeDependentTaskImpl> new_dependent_task(
            new FakeDependentTaskImpl(
                this, it->namespace_index, it->dependent_id));
        scoped_ptr<internal::GraphNode> dependent_node(
            new internal::GraphNode(new_dependent_task.get(), it->priority));
        node->add_dependent(dependent_node.get());
        dependent_node->add_dependency();
        new_graph.set(new_dependent_task.get(), dependent_node.Pass());
        new_dependents.push_back(new_dependent_task.get());
      }

      new_graph.set(new_task.get(), node.Pass());
      new_tasks.push_back(new_task.get());
    }

    task_graph_runner_->SetTaskGraph(
        namespace_token_[namespace_index], &new_graph);

    dependents_[namespace_index].swap(new_dependents);
    tasks_[namespace_index].swap(new_tasks);
  }

 protected:
  class FakeTaskImpl : public internal::Task {
   public:
    FakeTaskImpl(TaskGraphRunnerTestBase* test,
                 int namespace_index,
                 int id)
      : test_(test),
        namespace_index_(namespace_index),
        id_(id) {
    }

    // Overridden from internal::Task:
    virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
      test_->RunTask(namespace_index_, id_);
    }

    virtual void CompleteOnOriginThread() {
      test_->OnTaskCompleted(namespace_index_, id_);
    }

   protected:
    virtual ~FakeTaskImpl() {}

   private:
    TaskGraphRunnerTestBase* test_;
    int namespace_index_;
    int id_;

    DISALLOW_COPY_AND_ASSIGN(FakeTaskImpl);
  };

  class FakeDependentTaskImpl : public FakeTaskImpl {
   public:
    FakeDependentTaskImpl(TaskGraphRunnerTestBase* test,
                          int namespace_index,
                          int id)
        : FakeTaskImpl(test, namespace_index, id) {
    }

    // Overridden from FakeTaskImpl:
    virtual void CompleteOnOriginThread() OVERRIDE {}

   private:
    virtual ~FakeDependentTaskImpl() {}

    DISALLOW_COPY_AND_ASSIGN(FakeDependentTaskImpl);
  };

  scoped_ptr<internal::TaskGraphRunner> task_graph_runner_;
  internal::NamespaceToken namespace_token_[kNamespaceCount];
  internal::Task::Vector tasks_[kNamespaceCount];
  internal::Task::Vector dependents_[kNamespaceCount];
  std::vector<unsigned> run_task_ids_[kNamespaceCount];
  std::vector<unsigned> on_task_completed_ids_[kNamespaceCount];
};

class TaskGraphRunnerTest : public TaskGraphRunnerTestBase,
                            public testing::TestWithParam<int> {
 public:
  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    task_graph_runner_ = make_scoped_ptr(
        new internal::TaskGraphRunner(GetParam(), "Test"));
    for (int i = 0; i < kNamespaceCount; ++i)
      namespace_token_[i] = task_graph_runner_->GetNamespaceToken();
  }
  virtual void TearDown() OVERRIDE {
    task_graph_runner_.reset();
  }
};

TEST_P(TaskGraphRunnerTest, Basic) {
  for (int i = 0; i < kNamespaceCount; ++i) {
    EXPECT_EQ(0u, run_task_ids(i).size());
    EXPECT_EQ(0u, on_task_completed_ids(i).size());

    ScheduleTasks(i, std::vector<Task>(1, Task(i, 0u, 0u, 0u, 0u)));
  }

  for (int i = 0; i < kNamespaceCount; ++i) {
    RunAllTasks(i);

    EXPECT_EQ(1u, run_task_ids(i).size());
    EXPECT_EQ(1u, on_task_completed_ids(i).size());
  }

  for (int i = 0; i < kNamespaceCount; ++i)
    ScheduleTasks(i, std::vector<Task>(1, Task(i, 0u, 0u, 1u, 0u)));

  for (int i = 0; i < kNamespaceCount; ++i) {
    RunAllTasks(i);

    EXPECT_EQ(3u, run_task_ids(i).size());
    EXPECT_EQ(2u, on_task_completed_ids(i).size());
  }

  for (int i = 0; i < kNamespaceCount; ++i)
    ScheduleTasks(i, std::vector<Task>(1, Task(i, 0u, 0u, 2u, 0u)));

  for (int i = 0; i < kNamespaceCount; ++i) {
    RunAllTasks(i);

    EXPECT_EQ(6u, run_task_ids(i).size());
    EXPECT_EQ(3u, on_task_completed_ids(i).size());
  }
}

TEST_P(TaskGraphRunnerTest, Dependencies) {
  for (int i = 0; i < kNamespaceCount; ++i) {
    ScheduleTasks(i, std::vector<Task>(1, Task(i,
                                               0u,
                                               1u,
                                               1u,  // 1 dependent
                                               0u)));
  }

  for (int i = 0; i < kNamespaceCount; ++i) {
    RunAllTasks(i);

    // Check if task ran before dependent.
    ASSERT_EQ(2u, run_task_ids(i).size());
    EXPECT_EQ(0u, run_task_ids(i)[0]);
    EXPECT_EQ(1u, run_task_ids(i)[1]);
    ASSERT_EQ(1u, on_task_completed_ids(i).size());
    EXPECT_EQ(0u, on_task_completed_ids(i)[0]);
  }

  for (int i = 0; i < kNamespaceCount; ++i) {
    ScheduleTasks(i, std::vector<Task>(1, Task(i,
                                               2u,
                                               3u,
                                               2u,  // 2 dependents
                                               0u)));
  }

  for (int i = 0; i < kNamespaceCount; ++i) {
    RunAllTasks(i);

    // Task should only run once.
    ASSERT_EQ(5u, run_task_ids(i).size());
    EXPECT_EQ(2u, run_task_ids(i)[2]);
    EXPECT_EQ(3u, run_task_ids(i)[3]);
    EXPECT_EQ(3u, run_task_ids(i)[4]);
    ASSERT_EQ(2u, on_task_completed_ids(i).size());
    EXPECT_EQ(2u, on_task_completed_ids(i)[1]);
  }
}

INSTANTIATE_TEST_CASE_P(TaskGraphRunnerTests,
                        TaskGraphRunnerTest,
                        ::testing::Range(1, 5));

class TaskGraphRunnerSingleThreadTest : public TaskGraphRunnerTestBase,
                                        public testing::Test {
 public:
  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    task_graph_runner_ = make_scoped_ptr(
        new internal::TaskGraphRunner(1, "Test"));
    for (int i = 0; i < kNamespaceCount; ++i)
      namespace_token_[i] = task_graph_runner_->GetNamespaceToken();
  }
  virtual void TearDown() OVERRIDE {
    task_graph_runner_.reset();
  }
};

TEST_F(TaskGraphRunnerSingleThreadTest, Priority) {
  for (int i = 0; i < kNamespaceCount; ++i) {
    Task tasks[] = {
      Task(i,
           0u,
           2u,
           1u,
           1u),  // Priority 1
      Task(i,
           1u,
           3u,
           1u,
           0u)   // Priority 0
    };
    ScheduleTasks(i, std::vector<Task>(tasks, tasks + arraysize(tasks)));
  }

  for (int i = 0; i < kNamespaceCount; ++i) {
    RunAllTasks(i);

    // Check if tasks ran in order of priority.
    ASSERT_EQ(4u, run_task_ids(i).size());
    EXPECT_EQ(1u, run_task_ids(i)[0]);
    EXPECT_EQ(3u, run_task_ids(i)[1]);
    EXPECT_EQ(0u, run_task_ids(i)[2]);
    EXPECT_EQ(2u, run_task_ids(i)[3]);
    ASSERT_EQ(2u, on_task_completed_ids(i).size());
    EXPECT_EQ(1u, on_task_completed_ids(i)[0]);
    EXPECT_EQ(0u, on_task_completed_ids(i)[1]);
  }

  for (int i = 0; i < kNamespaceCount; ++i)
    ResetIds(i);

  for (int i = 0; i < kNamespaceCount; ++i) {
    std::vector<Task> tasks;
    tasks.push_back(Task(i,
                         0u,
                         3u,
                         1u,    // 1 dependent
                         1u));  // Priority 1
    tasks.push_back(Task(i,
                         1u,
                         4u,
                         2u,    // 2 dependents
                         1u));  // Priority 1
    tasks.push_back(Task(i,
                         2u,
                         5u,
                         1u,    // 1 dependent
                         0u));  // Priority 0
    ScheduleTasks(i, tasks);
  }

  for (int i = 0; i < kNamespaceCount; ++i) {
    RunAllTasks(i);

    // Check if tasks ran in order of priority and that task with more
    // dependents ran first when priority is the same.
    ASSERT_LE(3u, run_task_ids(i).size());
    EXPECT_EQ(2u, run_task_ids(i)[0]);
    EXPECT_EQ(5u, run_task_ids(i)[1]);
    EXPECT_EQ(1u, run_task_ids(i)[2]);
    ASSERT_EQ(3u, on_task_completed_ids(i).size());
    EXPECT_EQ(2u, on_task_completed_ids(i)[0]);
    EXPECT_EQ(1u, on_task_completed_ids(i)[1]);
    EXPECT_EQ(0u, on_task_completed_ids(i)[2]);
  }
}

}  // namespace
}  // namespace cc
