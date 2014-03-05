// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include "base/time/time.h"
#include "cc/resources/direct_raster_worker_pool.h"
#include "cc/resources/image_raster_worker_pool.h"
#include "cc/resources/pixel_buffer_raster_worker_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/lap_timer.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {
namespace {

enum RasterWorkerPoolType {
  RASTER_WORKER_POOL_TYPE_PIXEL_BUFFER,
  RASTER_WORKER_POOL_TYPE_IMAGE,
  RASTER_WORKER_POOL_TYPE_DIRECT
};

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class PerfWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  PerfWorkerPoolTaskImpl() {}

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {}

  // Overridden from internal::WorkerPoolTask:
  virtual void ScheduleOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {}
  virtual void RunOnOriginThread() OVERRIDE {}
  virtual void CompleteOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {}
  virtual void RunReplyOnOriginThread() OVERRIDE { Reset(); }

  void Reset() {
    did_run_ = false;
    did_complete_ = false;
  }

 protected:
  virtual ~PerfWorkerPoolTaskImpl() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PerfWorkerPoolTaskImpl);
};

class PerfRasterWorkerPoolTaskImpl : public internal::RasterWorkerPoolTask {
 public:
  PerfRasterWorkerPoolTaskImpl(scoped_ptr<ScopedResource> resource,
                               internal::WorkerPoolTask::Vector* dependencies)
      : internal::RasterWorkerPoolTask(resource.get(), dependencies),
        resource_(resource.Pass()) {}

  // Overridden from internal::Task:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {}

  // Overridden from internal::WorkerPoolTask:
  virtual void ScheduleOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {
    client->AcquireCanvasForRaster(this, resource());
  }
  virtual void RunOnOriginThread() OVERRIDE {}
  virtual void CompleteOnOriginThread(internal::WorkerPoolTaskClient* client)
      OVERRIDE {
    client->ReleaseCanvasForRaster(this, resource());
  }
  virtual void RunReplyOnOriginThread() OVERRIDE { Reset(); }

  void Reset() {
    did_run_ = false;
    did_complete_ = false;
  }

 protected:
  virtual ~PerfRasterWorkerPoolTaskImpl() {}

 private:
  scoped_ptr<ScopedResource> resource_;

  DISALLOW_COPY_AND_ASSIGN(PerfRasterWorkerPoolTaskImpl);
};

class PerfTaskGraphRunnerImpl : public internal::TaskGraphRunner {
 public:
  PerfTaskGraphRunnerImpl() : internal::TaskGraphRunner(0, "Perf") {}
};

class PerfPixelBufferRasterWorkerPoolImpl : public PixelBufferRasterWorkerPool {
 public:
  PerfPixelBufferRasterWorkerPoolImpl(
      internal::TaskGraphRunner* task_graph_runner,
      ResourceProvider* resource_provider)
      : PixelBufferRasterWorkerPool(base::MessageLoopProxy::current().get(),
                                    task_graph_runner,
                                    resource_provider,
                                    std::numeric_limits<size_t>::max()) {}
};

class PerfImageRasterWorkerPoolImpl : public ImageRasterWorkerPool {
 public:
  PerfImageRasterWorkerPoolImpl(internal::TaskGraphRunner* task_graph_runner,
                                ResourceProvider* resource_provider)
      : ImageRasterWorkerPool(base::MessageLoopProxy::current().get(),
                              task_graph_runner,
                              resource_provider,
                              GL_TEXTURE_2D) {}
};

class PerfDirectRasterWorkerPoolImpl : public DirectRasterWorkerPool {
 public:
  PerfDirectRasterWorkerPoolImpl(ResourceProvider* resource_provider,
                                 ContextProvider* context_provider)
      : DirectRasterWorkerPool(base::MessageLoopProxy::current().get(),
                               resource_provider,
                               context_provider) {}
};

class RasterWorkerPoolPerfTestBase {
 public:
  typedef std::vector<scoped_refptr<internal::RasterWorkerPoolTask> >
      RasterTaskVector;

  RasterWorkerPoolPerfTestBase()
      : context_provider_(TestContextProvider::Create()),
        timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {
    output_surface_ = FakeOutputSurface::Create3d(context_provider_).Pass();
    CHECK(output_surface_->BindToClient(&output_surface_client_));

    resource_provider_ = ResourceProvider::Create(
                             output_surface_.get(), NULL, 0, false, 1).Pass();
  }
  virtual ~RasterWorkerPoolPerfTestBase() { resource_provider_.reset(); }

  void CreateImageDecodeTasks(
      unsigned num_image_decode_tasks,
      internal::WorkerPoolTask::Vector* image_decode_tasks) {
    for (unsigned i = 0; i < num_image_decode_tasks; ++i)
      image_decode_tasks->push_back(new PerfWorkerPoolTaskImpl);
  }

  void CreateRasterTasks(
      unsigned num_raster_tasks,
      const internal::WorkerPoolTask::Vector& image_decode_tasks,
      RasterTaskVector* raster_tasks) {
    const gfx::Size size(1, 1);

    for (unsigned i = 0; i < num_raster_tasks; ++i) {
      scoped_ptr<ScopedResource> resource(
          ScopedResource::Create(resource_provider_.get()));
      resource->Allocate(size, ResourceProvider::TextureUsageAny, RGBA_8888);

      internal::WorkerPoolTask::Vector dependencies = image_decode_tasks;
      raster_tasks->push_back(
          new PerfRasterWorkerPoolTaskImpl(resource.Pass(), &dependencies));
    }
  }

  void BuildRasterTaskQueue(RasterTaskQueue* queue,
                            const RasterTaskVector& raster_tasks) {
    for (size_t i = 0u; i < raster_tasks.size(); ++i) {
      bool required_for_activation = (i % 2) == 0;
      queue->items.push_back(RasterTaskQueue::Item(raster_tasks[i].get(),
                                                   required_for_activation));
      queue->required_for_activation_count += required_for_activation;
    }
  }

 protected:
  scoped_refptr<TestContextProvider> context_provider_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  LapTimer timer_;
};

class RasterWorkerPoolPerfTest
    : public RasterWorkerPoolPerfTestBase,
      public testing::TestWithParam<RasterWorkerPoolType>,
      public RasterWorkerPoolClient {
 public:
  RasterWorkerPoolPerfTest() : task_graph_runner_(new PerfTaskGraphRunnerImpl) {
    switch (GetParam()) {
      case RASTER_WORKER_POOL_TYPE_PIXEL_BUFFER:
        raster_worker_pool_.reset(new PerfPixelBufferRasterWorkerPoolImpl(
            task_graph_runner_.get(), resource_provider_.get()));
        break;
      case RASTER_WORKER_POOL_TYPE_IMAGE:
        raster_worker_pool_.reset(new PerfImageRasterWorkerPoolImpl(
            task_graph_runner_.get(), resource_provider_.get()));
        break;
      case RASTER_WORKER_POOL_TYPE_DIRECT:
        raster_worker_pool_.reset(new PerfDirectRasterWorkerPoolImpl(
            resource_provider_.get(), context_provider_));
        break;
    }

    DCHECK(raster_worker_pool_);
    raster_worker_pool_->SetClient(this);
  }

  // Overridden from testing::Test:
  virtual void TearDown() OVERRIDE {
    raster_worker_pool_->Shutdown();
    raster_worker_pool_->CheckForCompletedTasks();
  }

  // Overriden from RasterWorkerPoolClient:
  virtual bool ShouldForceTasksRequiredForActivationToComplete()
      const OVERRIDE {
    return false;
  }
  virtual void DidFinishRunningTasks() OVERRIDE {
    raster_worker_pool_->CheckForCompletedTasks();
    base::MessageLoop::current()->Quit();
  }
  virtual void DidFinishRunningTasksRequiredForActivation() OVERRIDE {}

  void RunMessageLoopUntilAllTasksHaveCompleted() {
    while (task_graph_runner_->RunTaskForTesting())
      continue;
    base::MessageLoop::current()->Run();
  }

  void RunScheduleTasksTest(const std::string& test_name,
                            unsigned num_raster_tasks,
                            unsigned num_image_decode_tasks) {
    internal::WorkerPoolTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(num_raster_tasks, image_decode_tasks, &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same queue.
    RasterTaskQueue queue;

    timer_.Reset();
    do {
      queue.Reset();
      BuildRasterTaskQueue(&queue, raster_tasks);
      raster_worker_pool_->ScheduleTasks(&queue);
      raster_worker_pool_->CheckForCompletedTasks();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    RasterTaskQueue empty;
    raster_worker_pool_->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();

    perf_test::PrintResult("schedule_tasks",
                           TestModifierString(),
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunScheduleAlternateTasksTest(const std::string& test_name,
                                     unsigned num_raster_tasks,
                                     unsigned num_image_decode_tasks) {
    const size_t kNumVersions = 2;
    internal::WorkerPoolTask::Vector image_decode_tasks[kNumVersions];
    RasterTaskVector raster_tasks[kNumVersions];
    for (size_t i = 0; i < kNumVersions; ++i) {
      CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks[i]);
      CreateRasterTasks(
          num_raster_tasks, image_decode_tasks[i], &raster_tasks[i]);
    }

    // Avoid unnecessary heap allocations by reusing the same queue.
    RasterTaskQueue queue;

    size_t count = 0;
    timer_.Reset();
    do {
      queue.Reset();
      BuildRasterTaskQueue(&queue, raster_tasks[count % kNumVersions]);
      raster_worker_pool_->ScheduleTasks(&queue);
      raster_worker_pool_->CheckForCompletedTasks();
      ++count;
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    RasterTaskQueue empty;
    raster_worker_pool_->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();

    perf_test::PrintResult("schedule_alternate_tasks",
                           TestModifierString(),
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

  void RunScheduleAndExecuteTasksTest(const std::string& test_name,
                                      unsigned num_raster_tasks,
                                      unsigned num_image_decode_tasks) {
    internal::WorkerPoolTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(num_raster_tasks, image_decode_tasks, &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same queue.
    RasterTaskQueue queue;

    timer_.Reset();
    do {
      queue.Reset();
      BuildRasterTaskQueue(&queue, raster_tasks);
      raster_worker_pool_->ScheduleTasks(&queue);
      RunMessageLoopUntilAllTasksHaveCompleted();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    RasterTaskQueue empty;
    raster_worker_pool_->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();

    perf_test::PrintResult("schedule_and_execute_tasks",
                           TestModifierString(),
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }

 private:
  std::string TestModifierString() const {
    switch (GetParam()) {
      case RASTER_WORKER_POOL_TYPE_PIXEL_BUFFER:
        return std::string("_pixel_raster_worker_pool");
      case RASTER_WORKER_POOL_TYPE_IMAGE:
        return std::string("_image_raster_worker_pool");
      case RASTER_WORKER_POOL_TYPE_DIRECT:
        return std::string("_direct_raster_worker_pool");
    }
    NOTREACHED();
    return std::string();
  }

  scoped_ptr<internal::TaskGraphRunner> task_graph_runner_;
  scoped_ptr<RasterWorkerPool> raster_worker_pool_;
};

TEST_P(RasterWorkerPoolPerfTest, ScheduleTasks) {
  RunScheduleTasksTest("1_0", 1, 0);
  RunScheduleTasksTest("32_0", 32, 0);
  RunScheduleTasksTest("1_1", 1, 1);
  RunScheduleTasksTest("32_1", 32, 1);
  RunScheduleTasksTest("1_4", 1, 4);
  RunScheduleTasksTest("32_4", 32, 4);
}

TEST_P(RasterWorkerPoolPerfTest, ScheduleAlternateTasks) {
  RunScheduleAlternateTasksTest("1_0", 1, 0);
  RunScheduleAlternateTasksTest("32_0", 32, 0);
  RunScheduleAlternateTasksTest("1_1", 1, 1);
  RunScheduleAlternateTasksTest("32_1", 32, 1);
  RunScheduleAlternateTasksTest("1_4", 1, 4);
  RunScheduleAlternateTasksTest("32_4", 32, 4);
}

TEST_P(RasterWorkerPoolPerfTest, ScheduleAndExecuteTasks) {
  RunScheduleAndExecuteTasksTest("1_0", 1, 0);
  RunScheduleAndExecuteTasksTest("32_0", 32, 0);
  RunScheduleAndExecuteTasksTest("1_1", 1, 1);
  RunScheduleAndExecuteTasksTest("32_1", 32, 1);
  RunScheduleAndExecuteTasksTest("1_4", 1, 4);
  RunScheduleAndExecuteTasksTest("32_4", 32, 4);
}

INSTANTIATE_TEST_CASE_P(RasterWorkerPoolPerfTests,
                        RasterWorkerPoolPerfTest,
                        ::testing::Values(RASTER_WORKER_POOL_TYPE_PIXEL_BUFFER,
                                          RASTER_WORKER_POOL_TYPE_IMAGE,
                                          RASTER_WORKER_POOL_TYPE_DIRECT));

class RasterWorkerPoolCommonPerfTest : public RasterWorkerPoolPerfTestBase,
                                       public testing::Test {
 public:
  void RunBuildRasterTaskQueueTest(const std::string& test_name,
                                   unsigned num_raster_tasks,
                                   unsigned num_image_decode_tasks) {
    internal::WorkerPoolTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(num_raster_tasks, image_decode_tasks, &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same queue.
    RasterTaskQueue queue;

    timer_.Reset();
    do {
      queue.Reset();
      BuildRasterTaskQueue(&queue, raster_tasks);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("build_raster_task_queue",
                           "",
                           test_name,
                           timer_.LapsPerSecond(),
                           "runs/s",
                           true);
  }
};

TEST_F(RasterWorkerPoolCommonPerfTest, BuildRasterTaskQueue) {
  RunBuildRasterTaskQueueTest("1_0", 1, 0);
  RunBuildRasterTaskQueueTest("32_0", 32, 0);
  RunBuildRasterTaskQueueTest("1_1", 1, 1);
  RunBuildRasterTaskQueueTest("32_1", 32, 1);
  RunBuildRasterTaskQueueTest("1_4", 1, 4);
  RunBuildRasterTaskQueueTest("32_4", 32, 4);
}

}  // namespace
}  // namespace cc
