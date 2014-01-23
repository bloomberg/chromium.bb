// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/task_graph_runner.h"

#include "base/time/time.h"
#include "cc/base/completion_event.h"
#include "cc/test/lap_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class PerfTaskImpl : public internal::Task {
 public:
  PerfTaskImpl() {}

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {}

 private:
  virtual ~PerfTaskImpl() {}

  DISALLOW_COPY_AND_ASSIGN(PerfTaskImpl);
};

class PerfControlTaskImpl : public internal::Task {
 public:
  PerfControlTaskImpl() {}

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    did_start_.Signal();
    can_finish_.Wait();
  }

  void WaitForTaskToStartRunning() {
    did_start_.Wait();
  }

  void AllowTaskToFinish() {
    can_finish_.Signal();
  }

 private:
  virtual ~PerfControlTaskImpl() {}

  CompletionEvent did_start_;
  CompletionEvent can_finish_;

  DISALLOW_COPY_AND_ASSIGN(PerfControlTaskImpl);
};

class TaskGraphRunnerPerfTest : public testing::Test {
 public:
  TaskGraphRunnerPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {
  }

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    task_graph_runner_ = make_scoped_ptr(
        new internal::TaskGraphRunner(1, "PerfTest"));
    namespace_token_ = task_graph_runner_->GetNamespaceToken();
  }
  virtual void TearDown() OVERRIDE {
    task_graph_runner_.reset();
  }

  void AfterTest(const std::string& test_name) {
    // Format matches chrome/test/perf/perf_test.h:PrintResult
    printf("*RESULT %s: %.2f runs/s\n",
           test_name.c_str(),
           timer_.LapsPerSecond());
  }

  void RunScheduleTasksTest(const std::string& test_name,
                            unsigned max_depth,
                            unsigned num_children_per_node) {
    timer_.Reset();
    do {
      scoped_refptr<PerfControlTaskImpl> leaf_task(new PerfControlTaskImpl);
      ScheduleTasks(NULL, leaf_task.get(), max_depth, num_children_per_node);
      leaf_task->WaitForTaskToStartRunning();
      ScheduleTasks(NULL, NULL, 0, 0);
      leaf_task->AllowTaskToFinish();
      task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
      internal::Task::Vector completed_tasks;
      task_graph_runner_->CollectCompletedTasks(
          namespace_token_, &completed_tasks);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("schedule_tasks", "", test_name,
                           timer_.LapsPerSecond(), "runs/s", true);
  }

  void RunExecuteTasksTest(const std::string& test_name,
                           unsigned max_depth,
                           unsigned num_children_per_node) {
    timer_.Reset();
    do {
      ScheduleTasks(NULL, NULL, max_depth, num_children_per_node);
      task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
      internal::Task::Vector completed_tasks;
      task_graph_runner_->CollectCompletedTasks(
          namespace_token_, &completed_tasks);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("execute_tasks", "", test_name,
                           timer_.LapsPerSecond(), "runs/s", true);
  }

 private:
  void ScheduleTasks(internal::Task* root_task,
                     internal::Task* leaf_task,
                     unsigned max_depth,
                     unsigned num_children_per_node) {
    internal::Task::Vector tasks;
    internal::GraphNode::Map graph;

    scoped_ptr<internal::GraphNode> root_node;
    if (root_task)
      root_node = make_scoped_ptr(new internal::GraphNode(root_task, 0u));

    scoped_ptr<internal::GraphNode> leaf_node;
    if (leaf_task)
      leaf_node = make_scoped_ptr(new internal::GraphNode(leaf_task, 0u));

    if (max_depth) {
      BuildTaskGraph(&tasks,
                     &graph,
                     root_node.get(),
                     leaf_node.get(),
                     0,
                     max_depth,
                     num_children_per_node);
    }

    if (leaf_node)
      graph.set(leaf_task, leaf_node.Pass());

    if (root_node)
      graph.set(root_task, root_node.Pass());

    task_graph_runner_->SetTaskGraph(namespace_token_, &graph);

    tasks_.swap(tasks);
  }

  void BuildTaskGraph(internal::Task::Vector* tasks,
                      internal::GraphNode::Map* graph,
                      internal::GraphNode* dependent_node,
                      internal::GraphNode* leaf_node,
                      unsigned current_depth,
                      unsigned max_depth,
                      unsigned num_children_per_node) {
    scoped_refptr<PerfTaskImpl> task(new PerfTaskImpl);
    scoped_ptr<internal::GraphNode> node(
        new internal::GraphNode(task.get(), 0u));

    if (current_depth < max_depth) {
      for (unsigned i = 0; i < num_children_per_node; ++i) {
        BuildTaskGraph(tasks,
                       graph,
                       node.get(),
                       leaf_node,
                       current_depth + 1,
                       max_depth,
                       num_children_per_node);
      }
    } else if (leaf_node) {
      leaf_node->add_dependent(node.get());
      node->add_dependency();
    }

    if (dependent_node) {
      node->add_dependent(dependent_node);
      dependent_node->add_dependency();
    }
    graph->set(task.get(), node.Pass());
    tasks->push_back(task.get());
  }

  scoped_ptr<internal::TaskGraphRunner> task_graph_runner_;
  internal::NamespaceToken namespace_token_;
  internal::Task::Vector tasks_;
  LapTimer timer_;
};

TEST_F(TaskGraphRunnerPerfTest, ScheduleTasks) {
  RunScheduleTasksTest("1_10", 1, 10);
  RunScheduleTasksTest("1_1000", 1, 1000);
  RunScheduleTasksTest("2_10", 2, 10);
  RunScheduleTasksTest("5_5", 5, 5);
  RunScheduleTasksTest("10_2", 10, 2);
  RunScheduleTasksTest("1000_1", 1000, 1);
  RunScheduleTasksTest("10_1", 10, 1);
}

TEST_F(TaskGraphRunnerPerfTest, ExecuteTasks) {
  RunExecuteTasksTest("1_10", 1, 10);
  RunExecuteTasksTest("1_1000", 1, 1000);
  RunExecuteTasksTest("2_10", 2, 10);
  RunExecuteTasksTest("5_5", 5, 5);
  RunExecuteTasksTest("10_2", 10, 2);
  RunExecuteTasksTest("1000_1", 1000, 1);
  RunExecuteTasksTest("10_1", 10, 1);
}

}  // namespace
}  // namespace cc
