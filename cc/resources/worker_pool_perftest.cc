// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/worker_pool.h"

#include "base/time.h"
#include "cc/base/completion_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class PerfTaskImpl : public internal::WorkerPoolTask {
 public:
  explicit PerfTaskImpl(internal::WorkerPoolTask::TaskVector* dependencies)
      : internal::WorkerPoolTask(dependencies) {}

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnThread(unsigned thread_index) OVERRIDE {}
  virtual void DispatchCompletionCallback() OVERRIDE {}

 private:
  virtual ~PerfTaskImpl() {}
};

class PerfControlTaskImpl : public internal::WorkerPoolTask {
 public:
  explicit PerfControlTaskImpl(
      internal::WorkerPoolTask::TaskVector* dependencies)
      : internal::WorkerPoolTask(dependencies),
        did_start_(new CompletionEvent),
        can_finish_(new CompletionEvent) {}

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnThread(unsigned thread_index) OVERRIDE {
    did_start_->Signal();
    can_finish_->Wait();
  }
  virtual void DispatchCompletionCallback() OVERRIDE {}

  void WaitForTaskToStartRunning() {
    did_start_->Wait();
  }

  void AllowTaskToFinish() {
    can_finish_->Signal();
  }

 private:
  virtual ~PerfControlTaskImpl() {}

  scoped_ptr<CompletionEvent> did_start_;
  scoped_ptr<CompletionEvent> can_finish_;
};

class PerfWorkerPool : public WorkerPool {
 public:
  PerfWorkerPool() : WorkerPool(1, "test") {}
  virtual ~PerfWorkerPool() {}

  static scoped_ptr<PerfWorkerPool> Create() {
    return make_scoped_ptr(new PerfWorkerPool);
  }

  void BuildTaskGraph(internal::WorkerPoolTask* root) {
    graph_.clear();
    WorkerPool::BuildTaskGraph(root, &graph_);
  }

  void ScheduleTasks() {
    SetTaskGraph(&graph_);
  }

 private:
  TaskGraph graph_;
};

class WorkerPoolPerfTest : public testing::Test {
 public:
  WorkerPoolPerfTest() : num_runs_(0) {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    worker_pool_ = PerfWorkerPool::Create();
  }
  virtual void TearDown() OVERRIDE {
    worker_pool_->Shutdown();
    worker_pool_->CheckForCompletedTasks();
  }

  void EndTest() {
    elapsed_ = base::TimeTicks::HighResNow() - start_time_;
  }

  void AfterTest(const std::string test_name) {
    // Format matches chrome/test/perf/perf_test.h:PrintResult
    printf("*RESULT %s: %.2f runs/s\n",
           test_name.c_str(),
           num_runs_ / elapsed_.InSecondsF());
  }

  void CreateTasks(internal::WorkerPoolTask::TaskVector* dependencies,
                   unsigned current_depth,
                   unsigned max_depth,
                   unsigned num_children_per_node) {
    internal::WorkerPoolTask::TaskVector children;
    if (current_depth < max_depth) {
      for (unsigned i = 0; i < num_children_per_node; ++i) {
        CreateTasks(&children,
                    current_depth + 1,
                    max_depth,
                    num_children_per_node);
      }
    } else if (leaf_task_.get()) {
      children.push_back(leaf_task_);
    }
    dependencies->push_back(make_scoped_refptr(new PerfTaskImpl(&children)));
  }

  bool DidRun() {
    ++num_runs_;
    if (num_runs_ == kWarmupRuns)
      start_time_ = base::TimeTicks::HighResNow();

    if (!start_time_.is_null() && (num_runs_ % kTimeCheckInterval) == 0) {
      base::TimeDelta elapsed = base::TimeTicks::HighResNow() - start_time_;
      if (elapsed >= base::TimeDelta::FromMilliseconds(kTimeLimitMillis)) {
        elapsed_ = elapsed;
        return false;
      }
    }

    return true;
  }

  void RunBuildTaskGraphTest(const std::string test_name,
                             unsigned max_depth,
                             unsigned num_children_per_node) {
    start_time_ = base::TimeTicks();
    num_runs_ = 0;
    internal::WorkerPoolTask::TaskVector children;
    CreateTasks(&children, 0, max_depth, num_children_per_node);
    scoped_refptr<PerfTaskImpl> root_task(
        make_scoped_refptr(new PerfTaskImpl(&children)));
    do {
      worker_pool_->BuildTaskGraph(root_task.get());
    } while (DidRun());

    AfterTest(test_name);
  }

  void RunScheduleTasksTest(const std::string test_name,
                            unsigned max_depth,
                            unsigned num_children_per_node) {
    start_time_ = base::TimeTicks();
    num_runs_ = 0;
    do {
      internal::WorkerPoolTask::TaskVector empty;
      leaf_task_ = make_scoped_refptr(new PerfControlTaskImpl(&empty));
      internal::WorkerPoolTask::TaskVector children;
      CreateTasks(&children, 0, max_depth, num_children_per_node);
      scoped_refptr<PerfTaskImpl> root_task(
          make_scoped_refptr(new PerfTaskImpl(&children)));

      worker_pool_->BuildTaskGraph(root_task.get());
      worker_pool_->ScheduleTasks();
      leaf_task_->WaitForTaskToStartRunning();
      worker_pool_->BuildTaskGraph(NULL);
      worker_pool_->ScheduleTasks();
      worker_pool_->CheckForCompletedTasks();
      leaf_task_->AllowTaskToFinish();
    } while (DidRun());

    AfterTest(test_name);
  }

  void RunExecuteTasksTest(const std::string test_name,
                           unsigned max_depth,
                           unsigned num_children_per_node) {
    start_time_ = base::TimeTicks();
    num_runs_ = 0;
    do {
      internal::WorkerPoolTask::TaskVector children;
      CreateTasks(&children, 0, max_depth, num_children_per_node);
      scoped_refptr<PerfControlTaskImpl> root_task(
          make_scoped_refptr(new PerfControlTaskImpl(&children)));

      worker_pool_->BuildTaskGraph(root_task.get());
      worker_pool_->ScheduleTasks();
      root_task->WaitForTaskToStartRunning();
      root_task->AllowTaskToFinish();
      worker_pool_->CheckForCompletedTasks();
    } while (DidRun());

    AfterTest(test_name);
  }

 protected:
  scoped_ptr<PerfWorkerPool> worker_pool_;
  scoped_refptr<PerfControlTaskImpl> leaf_task_;
  base::TimeTicks start_time_;
  base::TimeDelta elapsed_;
  int num_runs_;
};

TEST_F(WorkerPoolPerfTest, BuildTaskGraph) {
  RunBuildTaskGraphTest("build_task_graph_1_10", 1, 10);
  RunBuildTaskGraphTest("build_task_graph_1_1000", 1, 1000);
  RunBuildTaskGraphTest("build_task_graph_2_10", 2, 10);
  RunBuildTaskGraphTest("build_task_graph_5_5", 5, 5);
  RunBuildTaskGraphTest("build_task_graph_10_2", 10, 2);
  RunBuildTaskGraphTest("build_task_graph_1000_1", 1000, 1);
  RunBuildTaskGraphTest("build_task_graph_10_1", 10, 1);
}

TEST_F(WorkerPoolPerfTest, ScheduleTasks) {
  RunScheduleTasksTest("schedule_tasks_1_10", 1, 10);
  RunScheduleTasksTest("schedule_tasks_1_1000", 1, 1000);
  RunScheduleTasksTest("schedule_tasks_2_10", 2, 10);
  RunScheduleTasksTest("schedule_tasks_5_5", 5, 5);
  RunScheduleTasksTest("schedule_tasks_10_2", 10, 2);
  RunScheduleTasksTest("schedule_tasks_1000_1", 1000, 1);
  RunScheduleTasksTest("schedule_tasks_10_1", 10, 1);
}

TEST_F(WorkerPoolPerfTest, ExecuteTasks) {
  RunExecuteTasksTest("execute_tasks_1_10", 1, 10);
  RunExecuteTasksTest("execute_tasks_1_1000", 1, 1000);
  RunExecuteTasksTest("execute_tasks_2_10", 2, 10);
  RunExecuteTasksTest("execute_tasks_5_5", 5, 5);
  RunExecuteTasksTest("execute_tasks_10_2", 10, 2);
  RunExecuteTasksTest("execute_tasks_1000_1", 1000, 1);
  RunExecuteTasksTest("execute_tasks_10_1", 10, 1);
}

}  // namespace

}  // namespace cc
