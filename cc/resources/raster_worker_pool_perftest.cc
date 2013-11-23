// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include "base/time/time.h"
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

class PerfRasterWorkerPool : public RasterWorkerPool {
 public:
  PerfRasterWorkerPool() : RasterWorkerPool(NULL, 1) {}
  virtual ~PerfRasterWorkerPool() {}

  static scoped_ptr<PerfRasterWorkerPool> Create() {
    return make_scoped_ptr(new PerfRasterWorkerPool);
  }

  // Overridden from RasterWorkerPool:
  virtual void ScheduleTasks(RasterTask::Queue* queue) OVERRIDE {
    NOTREACHED();
  }
  virtual GLenum GetResourceTarget() const OVERRIDE {
    NOTREACHED();
    return GL_TEXTURE_2D;
  }
  virtual ResourceFormat GetResourceFormat() const OVERRIDE {
    NOTREACHED();
    return RGBA_8888;
  }
  virtual void OnRasterTasksFinished() OVERRIDE {
    NOTREACHED();
  }
  virtual void OnRasterTasksRequiredForActivationFinished() OVERRIDE {
    NOTREACHED();
  }

  void SetRasterTasks(RasterTask::Queue* queue) {
    RasterWorkerPool::SetRasterTasks(queue);

    TaskMap perf_tasks;
    for (RasterTaskVector::const_iterator it = raster_tasks().begin();
         it != raster_tasks().end(); ++it) {
      internal::RasterWorkerPoolTask* task = it->get();

      scoped_refptr<internal::WorkerPoolTask> new_perf_task(
          new PerfWorkerPoolTaskImpl);
      perf_tasks[task] = new_perf_task;
    }

    perf_tasks_.swap(perf_tasks);
  }

  void BuildTaskGraph() {
    unsigned priority = 0;
    TaskGraph graph;

    scoped_refptr<internal::WorkerPoolTask>
        raster_required_for_activation_finished_task(
            CreateRasterRequiredForActivationFinishedTask());
    internal::GraphNode* raster_required_for_activation_finished_node =
        CreateGraphNodeForTask(
            raster_required_for_activation_finished_task.get(),
            priority++,
            &graph);

    scoped_refptr<internal::WorkerPoolTask> raster_finished_task(
        CreateRasterFinishedTask());
    internal::GraphNode* raster_finished_node =
        CreateGraphNodeForTask(raster_finished_task.get(),
                               priority++,
                               &graph);

    for (RasterTaskVector::const_iterator it = raster_tasks().begin();
         it != raster_tasks().end(); ++it) {
      internal::RasterWorkerPoolTask* task = it->get();

      TaskMap::iterator perf_it = perf_tasks_.find(task);
      DCHECK(perf_it != perf_tasks_.end());
      if (perf_it != perf_tasks_.end()) {
        internal::WorkerPoolTask* perf_task = perf_it->second.get();

        internal::GraphNode* perf_node =
            CreateGraphNodeForRasterTask(perf_task,
                                         task->dependencies(),
                                         priority++,
                                         &graph);

        if (IsRasterTaskRequiredForActivation(task)) {
          raster_required_for_activation_finished_node->add_dependency();
          perf_node->add_dependent(
              raster_required_for_activation_finished_node);
        }

        raster_finished_node->add_dependency();
        perf_node->add_dependent(raster_finished_node);
      }
    }
  }

 private:
  TaskMap perf_tasks_;

  DISALLOW_COPY_AND_ASSIGN(PerfRasterWorkerPool);
};

class RasterWorkerPoolPerfTest : public testing::Test {
 public:
  RasterWorkerPoolPerfTest()
      : timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    raster_worker_pool_ = PerfRasterWorkerPool::Create();
  }
  virtual void TearDown() OVERRIDE {
    raster_worker_pool_->Shutdown();
  }

  void CreateTasks(RasterWorkerPool::RasterTask::Queue* tasks,
                   unsigned num_raster_tasks,
                   unsigned num_image_decode_tasks) {
    typedef std::vector<RasterWorkerPool::Task> TaskVector;
    TaskVector image_decode_tasks;

    for (unsigned i = 0; i < num_image_decode_tasks; ++i) {
      image_decode_tasks.push_back(
          RasterWorkerPool::CreateImageDecodeTask(
              NULL,
              0,
              NULL,
              base::Bind(
                  &RasterWorkerPoolPerfTest::OnImageDecodeTaskCompleted)));
    }

    for (unsigned i = 0; i < num_raster_tasks; ++i) {
      RasterWorkerPool::Task::Set decode_tasks;
      for (TaskVector::iterator it = image_decode_tasks.begin();
           it != image_decode_tasks.end(); ++it)
        decode_tasks.Insert(*it);

      tasks->Append(
          RasterWorkerPool::CreateRasterTask(
              NULL,
              NULL,
              gfx::Rect(),
              1.0,
              HIGH_QUALITY_RASTER_MODE,
              TileResolution(),
              1,
              NULL,
              1,
              NULL,
              base::Bind(&RasterWorkerPoolPerfTest::OnRasterTaskCompleted),
              &decode_tasks),
          false);
    }
  }

  void RunBuildTaskGraphTest(const std::string& test_name,
                             unsigned num_raster_tasks,
                             unsigned num_image_decode_tasks) {
    timer_.Reset();
    RasterWorkerPool::RasterTask::Queue tasks;
    CreateTasks(&tasks, num_raster_tasks, num_image_decode_tasks);
    raster_worker_pool_->SetRasterTasks(&tasks);
    do {
      raster_worker_pool_->BuildTaskGraph();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("build_task_graph", "", test_name,
                           timer_.LapsPerSecond(), "runs/s", true);
  }

 protected:
  static void OnRasterTaskCompleted(const PicturePileImpl::Analysis& analysis,
                                    bool was_canceled) {}
  static void OnImageDecodeTaskCompleted(bool was_canceled) {}

  scoped_ptr<PerfRasterWorkerPool> raster_worker_pool_;
  LapTimer timer_;
};

TEST_F(RasterWorkerPoolPerfTest, BuildTaskGraph) {
  RunBuildTaskGraphTest("10_0", 10, 0);
  RunBuildTaskGraphTest("100_0", 100, 0);
  RunBuildTaskGraphTest("1000_0", 1000, 0);
  RunBuildTaskGraphTest("10_1", 10, 1);
  RunBuildTaskGraphTest("100_1", 100, 1);
  RunBuildTaskGraphTest("1000_1", 1000, 1);
  RunBuildTaskGraphTest("10_4", 10, 4);
  RunBuildTaskGraphTest("100_4", 100, 4);
  RunBuildTaskGraphTest("1000_4", 1000, 4);
  RunBuildTaskGraphTest("10_16", 10, 16);
  RunBuildTaskGraphTest("100_16", 100, 16);
  RunBuildTaskGraphTest("1000_16", 1000, 16);
}

}  // namespace

}  // namespace cc
