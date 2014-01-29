// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/task_graph_runner.h"

#include <vector>

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
  typedef std::vector<scoped_refptr<PerfTaskImpl> > Vector;

  PerfTaskImpl() {}

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {}

  void Reset() {
    did_schedule_ = false;
    did_run_ = false;
  }

 private:
  virtual ~PerfTaskImpl() {}

  DISALLOW_COPY_AND_ASSIGN(PerfTaskImpl);
};

class TaskGraphRunnerPerfTest : public testing::Test {
 public:
  TaskGraphRunnerPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    task_graph_runner_ =
        make_scoped_ptr(new internal::TaskGraphRunner(0,  // 0 worker threads
                                                      "PerfTest"));
    namespace_token_ = task_graph_runner_->GetNamespaceToken();
  }
  virtual void TearDown() OVERRIDE { task_graph_runner_.reset(); }

  void AfterTest(const std::string& test_name) {
    // Format matches chrome/test/perf/perf_test.h:PrintResult
    printf(
        "*RESULT %s: %.2f runs/s\n", test_name.c_str(), timer_.LapsPerSecond());
  }

  void RunBuildTaskGraphTest(const std::string& test_name,
                             int num_top_level_tasks,
                             int num_tasks,
                             int num_leaf_tasks) {
    PerfTaskImpl::Vector top_level_tasks;
    PerfTaskImpl::Vector tasks;
    PerfTaskImpl::Vector leaf_tasks;
    CreateTasks(num_top_level_tasks, &top_level_tasks);
    CreateTasks(num_tasks, &tasks);
    CreateTasks(num_leaf_tasks, &leaf_tasks);

    timer_.Reset();
    do {
      internal::GraphNode::Map graph;
      BuildTaskGraph(top_level_tasks, tasks, leaf_tasks, &graph);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("build_task_graph",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunScheduleTasksTest(const std::string& test_name,
                            int num_top_level_tasks,
                            int num_tasks,
                            int num_leaf_tasks) {
    PerfTaskImpl::Vector top_level_tasks;
    PerfTaskImpl::Vector tasks;
    PerfTaskImpl::Vector leaf_tasks;
    CreateTasks(num_top_level_tasks, &top_level_tasks);
    CreateTasks(num_tasks, &tasks);
    CreateTasks(num_leaf_tasks, &leaf_tasks);

    timer_.Reset();
    do {
      internal::GraphNode::Map graph;
      BuildTaskGraph(top_level_tasks, tasks, leaf_tasks, &graph);
      task_graph_runner_->SetTaskGraph(namespace_token_, &graph);
      // Shouldn't be any tasks to collect as we reschedule the same set
      // of tasks.
      DCHECK_EQ(0u, CollectCompletedTasks());
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    internal::GraphNode::Map empty;
    task_graph_runner_->SetTaskGraph(namespace_token_, &empty);
    CollectCompletedTasks();

    perf_test::PrintResult("schedule_tasks",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunScheduleAlternateTasksTest(const std::string& test_name,
                                     int num_top_level_tasks,
                                     int num_tasks,
                                     int num_leaf_tasks) {
    const int kNumVersions = 2;
    PerfTaskImpl::Vector top_level_tasks[kNumVersions];
    PerfTaskImpl::Vector tasks[kNumVersions];
    PerfTaskImpl::Vector leaf_tasks[kNumVersions];
    for (int i = 0; i < kNumVersions; ++i) {
      CreateTasks(num_top_level_tasks, &top_level_tasks[i]);
      CreateTasks(num_tasks, &tasks[i]);
      CreateTasks(num_leaf_tasks, &leaf_tasks[i]);
    }

    int count = 0;
    timer_.Reset();
    do {
      internal::GraphNode::Map graph;
      BuildTaskGraph(top_level_tasks[count % kNumVersions],
                     tasks[count % kNumVersions],
                     leaf_tasks[count % kNumVersions],
                     &graph);
      task_graph_runner_->SetTaskGraph(namespace_token_, &graph);
      CollectCompletedTasks();
      ++count;
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    internal::GraphNode::Map empty;
    task_graph_runner_->SetTaskGraph(namespace_token_, &empty);
    CollectCompletedTasks();

    perf_test::PrintResult("schedule_alternate_tasks",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunScheduleAndExecuteTasksTest(const std::string& test_name,
                                      int num_top_level_tasks,
                                      int num_tasks,
                                      int num_leaf_tasks) {
    PerfTaskImpl::Vector top_level_tasks;
    PerfTaskImpl::Vector tasks;
    PerfTaskImpl::Vector leaf_tasks;
    CreateTasks(num_top_level_tasks, &top_level_tasks);
    CreateTasks(num_tasks, &tasks);
    CreateTasks(num_leaf_tasks, &leaf_tasks);

    timer_.Reset();
    do {
      internal::GraphNode::Map graph;
      BuildTaskGraph(top_level_tasks, tasks, leaf_tasks, &graph);
      task_graph_runner_->SetTaskGraph(namespace_token_, &graph);
      while (task_graph_runner_->RunTaskForTesting())
        continue;
      CollectCompletedTasks();
      ResetTasks(&top_level_tasks);
      ResetTasks(&tasks);
      ResetTasks(&leaf_tasks);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult(
        "execute_tasks", "", test_name, timer_.LapsPerSecond(), "runs/s", true);
  }

 private:
  void CreateTasks(int num_tasks, PerfTaskImpl::Vector* tasks) {
    for (int i = 0; i < num_tasks; ++i)
      tasks->push_back(make_scoped_refptr(new PerfTaskImpl));
  }

  void ResetTasks(PerfTaskImpl::Vector* tasks) {
    for (PerfTaskImpl::Vector::iterator it = tasks->begin(); it != tasks->end();
         ++it) {
      PerfTaskImpl* task = it->get();
      task->Reset();
    }
  }

  void BuildTaskGraph(const PerfTaskImpl::Vector& top_level_tasks,
                      const PerfTaskImpl::Vector& tasks,
                      const PerfTaskImpl::Vector& leaf_tasks,
                      internal::GraphNode::Map* graph) {
    typedef std::vector<internal::GraphNode*> NodeVector;

    NodeVector top_level_nodes;
    top_level_nodes.reserve(top_level_tasks.size());
    for (PerfTaskImpl::Vector::const_iterator it = top_level_tasks.begin();
         it != top_level_tasks.end();
         ++it) {
      internal::Task* top_level_task = it->get();
      scoped_ptr<internal::GraphNode> top_level_node(
          new internal::GraphNode(top_level_task, 0u));

      top_level_nodes.push_back(top_level_node.get());
      graph->set(top_level_task, top_level_node.Pass());
    }

    NodeVector leaf_nodes;
    leaf_nodes.reserve(leaf_tasks.size());
    for (PerfTaskImpl::Vector::const_iterator it = leaf_tasks.begin();
         it != leaf_tasks.end();
         ++it) {
      internal::Task* leaf_task = it->get();
      scoped_ptr<internal::GraphNode> leaf_node(
          new internal::GraphNode(leaf_task, 0u));

      leaf_nodes.push_back(leaf_node.get());
      graph->set(leaf_task, leaf_node.Pass());
    }

    for (PerfTaskImpl::Vector::const_iterator it = tasks.begin();
         it != tasks.end();
         ++it) {
      internal::Task* task = it->get();
      scoped_ptr<internal::GraphNode> node(new internal::GraphNode(task, 0u));

      for (NodeVector::iterator node_it = top_level_nodes.begin();
           node_it != top_level_nodes.end();
           ++node_it) {
        internal::GraphNode* top_level_node = *node_it;
        node->add_dependent(top_level_node);
        top_level_node->add_dependency();
      }

      for (NodeVector::iterator node_it = leaf_nodes.begin();
           node_it != leaf_nodes.end();
           ++node_it) {
        internal::GraphNode* leaf_node = *node_it;
        leaf_node->add_dependent(node.get());
        node->add_dependency();
      }

      graph->set(task, node.Pass());
    }
  }

  size_t CollectCompletedTasks() {
    internal::Task::Vector completed_tasks;
    task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                              &completed_tasks);
    return completed_tasks.size();
  }

  scoped_ptr<internal::TaskGraphRunner> task_graph_runner_;
  internal::NamespaceToken namespace_token_;
  LapTimer timer_;
};

TEST_F(TaskGraphRunnerPerfTest, BuildTaskGraph) {
  RunBuildTaskGraphTest("0_1_0", 0, 1, 0);
  RunBuildTaskGraphTest("0_32_0", 0, 32, 0);
  RunBuildTaskGraphTest("2_1_0", 2, 1, 0);
  RunBuildTaskGraphTest("2_32_0", 2, 32, 0);
  RunBuildTaskGraphTest("2_1_1", 2, 1, 1);
  RunBuildTaskGraphTest("2_32_1", 2, 32, 1);
}

TEST_F(TaskGraphRunnerPerfTest, ScheduleTasks) {
  RunScheduleTasksTest("0_1_0", 0, 1, 0);
  RunScheduleTasksTest("0_32_0", 0, 32, 0);
  RunScheduleTasksTest("2_1_0", 2, 1, 0);
  RunScheduleTasksTest("2_32_0", 2, 32, 0);
  RunScheduleTasksTest("2_1_1", 2, 1, 1);
  RunScheduleTasksTest("2_32_1", 2, 32, 1);
}

TEST_F(TaskGraphRunnerPerfTest, ScheduleAlternateTasks) {
  RunScheduleAlternateTasksTest("0_1_0", 0, 1, 0);
  RunScheduleAlternateTasksTest("0_32_0", 0, 32, 0);
  RunScheduleAlternateTasksTest("2_1_0", 2, 1, 0);
  RunScheduleAlternateTasksTest("2_32_0", 2, 32, 0);
  RunScheduleAlternateTasksTest("2_1_1", 2, 1, 1);
  RunScheduleAlternateTasksTest("2_32_1", 2, 32, 1);
}

TEST_F(TaskGraphRunnerPerfTest, ScheduleAndExecuteTasks) {
  RunScheduleAndExecuteTasksTest("0_1_0", 0, 1, 0);
  RunScheduleAndExecuteTasksTest("0_32_0", 0, 32, 0);
  RunScheduleAndExecuteTasksTest("2_1_0", 2, 1, 0);
  RunScheduleAndExecuteTasksTest("2_32_0", 2, 32, 0);
  RunScheduleAndExecuteTasksTest("2_1_1", 2, 1, 1);
  RunScheduleAndExecuteTasksTest("2_32_1", 2, 32, 1);
}

}  // namespace
}  // namespace cc
