// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/worker_pool.h"

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

class PerfWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {}
  virtual void CompleteOnOriginThread() OVERRIDE {}

 private:
  virtual ~PerfWorkerPoolTaskImpl() {}
};

class PerfControlWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  PerfControlWorkerPoolTaskImpl() : did_start_(new CompletionEvent),
                                    can_finish_(new CompletionEvent) {}

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    did_start_->Signal();
    can_finish_->Wait();
  }
  virtual void CompleteOnOriginThread() OVERRIDE {}

  void WaitForTaskToStartRunning() {
    did_start_->Wait();
  }

  void AllowTaskToFinish() {
    can_finish_->Signal();
  }

 private:
  virtual ~PerfControlWorkerPoolTaskImpl() {}

  scoped_ptr<CompletionEvent> did_start_;
  scoped_ptr<CompletionEvent> can_finish_;

  DISALLOW_COPY_AND_ASSIGN(PerfControlWorkerPoolTaskImpl);
};

class PerfWorkerPool : public WorkerPool {
 public:
  PerfWorkerPool() : WorkerPool(1, "test") {}
  virtual ~PerfWorkerPool() {}

  static scoped_ptr<PerfWorkerPool> Create() {
    return make_scoped_ptr(new PerfWorkerPool);
  }

  void ScheduleTasks(internal::WorkerPoolTask* root_task,
                     internal::WorkerPoolTask* leaf_task,
                     unsigned max_depth,
                     unsigned num_children_per_node) {
    TaskVector tasks;
    TaskGraph graph;

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

    SetTaskGraph(&graph);

    tasks_.swap(tasks);
  }

 private:
  typedef std::vector<scoped_refptr<internal::WorkerPoolTask> > TaskVector;

  void BuildTaskGraph(TaskVector* tasks,
                      TaskGraph* graph,
                      internal::GraphNode* dependent_node,
                      internal::GraphNode* leaf_node,
                      unsigned current_depth,
                      unsigned max_depth,
                      unsigned num_children_per_node) {
    scoped_refptr<PerfWorkerPoolTaskImpl> task(new PerfWorkerPoolTaskImpl);
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

  TaskVector tasks_;

  DISALLOW_COPY_AND_ASSIGN(PerfWorkerPool);
};

class WorkerPoolPerfTest : public testing::Test {
 public:
  WorkerPoolPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    worker_pool_ = PerfWorkerPool::Create();
  }
  virtual void TearDown() OVERRIDE {
    worker_pool_->Shutdown();
    worker_pool_->CheckForCompletedTasks();
  }

  void AfterTest(const std::string& test_name) {
    // Format matches chrome/test/perf/perf_test.h:PrintResult
    printf(
        "*RESULT %s: %.2f runs/s\n", test_name.c_str(), timer_.LapsPerSecond());
  }

  void RunScheduleTasksTest(const std::string& test_name,
                            unsigned max_depth,
                            unsigned num_children_per_node) {
    timer_.Reset();
    do {
      scoped_refptr<PerfControlWorkerPoolTaskImpl> leaf_task(
          new PerfControlWorkerPoolTaskImpl);
      worker_pool_->ScheduleTasks(
          NULL, leaf_task.get(), max_depth, num_children_per_node);
      leaf_task->WaitForTaskToStartRunning();
      worker_pool_->ScheduleTasks(NULL, NULL, 0, 0);
      worker_pool_->CheckForCompletedTasks();
      leaf_task->AllowTaskToFinish();
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
      scoped_refptr<PerfControlWorkerPoolTaskImpl> root_task(
          new PerfControlWorkerPoolTaskImpl);
      worker_pool_->ScheduleTasks(
          root_task.get(), NULL, max_depth, num_children_per_node);
      root_task->WaitForTaskToStartRunning();
      root_task->AllowTaskToFinish();
      worker_pool_->CheckForCompletedTasks();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("execute_tasks", "", test_name,
                           timer_.LapsPerSecond(), "runs/s", true);
  }

 protected:
  scoped_ptr<PerfWorkerPool> worker_pool_;
  LapTimer timer_;
};

TEST_F(WorkerPoolPerfTest, ScheduleTasks) {
  RunScheduleTasksTest("1_10", 1, 10);
  RunScheduleTasksTest("1_1000", 1, 1000);
  RunScheduleTasksTest("2_10", 2, 10);
  RunScheduleTasksTest("5_5", 5, 5);
  RunScheduleTasksTest("10_2", 10, 2);
  RunScheduleTasksTest("1000_1", 1000, 1);
  RunScheduleTasksTest("10_1", 10, 1);
}

TEST_F(WorkerPoolPerfTest, ExecuteTasks) {
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
