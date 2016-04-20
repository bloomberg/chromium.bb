// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "cc/debug/lap_timer.h"
#include "cc/output/context_provider.h"
#include "cc/raster/bitmap_tile_task_worker_pool.h"
#include "cc/raster/gpu_rasterizer.h"
#include "cc/raster/gpu_tile_task_worker_pool.h"
#include "cc/raster/one_copy_tile_task_worker_pool.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/raster/tile_task_runner.h"
#include "cc/raster/tile_task_worker_pool.h"
#include "cc/raster/zero_copy_tile_task_worker_pool.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/test_context_support.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace cc {
namespace {

class PerfGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
  // Overridden from gpu::gles2::GLES2Interface:
  GLuint CreateImageCHROMIUM(ClientBuffer buffer,
                             GLsizei width,
                             GLsizei height,
                             GLenum internalformat) override {
    return 1u;
  }
  void GenBuffers(GLsizei n, GLuint* buffers) override {
    for (GLsizei i = 0; i < n; ++i)
      buffers[i] = 1u;
  }
  void GenTextures(GLsizei n, GLuint* textures) override {
    for (GLsizei i = 0; i < n; ++i)
      textures[i] = 1u;
  }
  void GetIntegerv(GLenum pname, GLint* params) override {
    if (pname == GL_MAX_TEXTURE_SIZE)
      *params = INT_MAX;
  }
  void GenQueriesEXT(GLsizei n, GLuint* queries) override {
    for (GLsizei i = 0; i < n; ++i)
      queries[i] = 1u;
  }
  void GetQueryObjectuivEXT(GLuint query,
                            GLenum pname,
                            GLuint* params) override {
    if (pname == GL_QUERY_RESULT_AVAILABLE_EXT)
      *params = 1;
  }
};

class PerfContextProvider : public ContextProvider {
 public:
  PerfContextProvider() : context_gl_(new PerfGLES2Interface) {}

  bool BindToCurrentThread() override { return true; }
  gpu::Capabilities ContextCapabilities() override {
    gpu::Capabilities capabilities;
    capabilities.image = true;
    capabilities.sync_query = true;
    return capabilities;
  }
  gpu::gles2::GLES2Interface* ContextGL() override { return context_gl_.get(); }
  gpu::ContextSupport* ContextSupport() override { return &support_; }
  class GrContext* GrContext() override {
    if (gr_context_)
      return gr_context_.get();

    skia::RefPtr<const GrGLInterface> null_interface =
        skia::AdoptRef(GrGLCreateNullInterface());
    gr_context_ = skia::AdoptRef(GrContext::Create(
        kOpenGL_GrBackend,
        reinterpret_cast<GrBackendContext>(null_interface.get())));
    return gr_context_.get();
  }
  void InvalidateGrContext(uint32_t state) override {
    if (gr_context_)
      gr_context_.get()->resetContext(state);
  }
  void SetupLock() override {}
  base::Lock* GetLock() override { return &context_lock_; }
  void DeleteCachedResources() override {}
  void SetLostContextCallback(const LostContextCallback& cb) override {}

 private:
  ~PerfContextProvider() override {}

  std::unique_ptr<PerfGLES2Interface> context_gl_;
  skia::RefPtr<class GrContext> gr_context_;
  TestContextSupport support_;
  base::Lock context_lock_;
};

enum TileTaskWorkerPoolType {
  TILE_TASK_WORKER_POOL_TYPE_ZERO_COPY,
  TILE_TASK_WORKER_POOL_TYPE_ONE_COPY,
  TILE_TASK_WORKER_POOL_TYPE_GPU,
  TILE_TASK_WORKER_POOL_TYPE_BITMAP
};

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class PerfImageDecodeTaskImpl : public TileTask {
 public:
  PerfImageDecodeTaskImpl() : TileTask(true) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {}

  // Overridden from TileTask:
  void ScheduleOnOriginThread(RasterBufferProvider* provider) override {}
  void CompleteOnOriginThread(RasterBufferProvider* provider) override {
    Reset();
  }

  void Reset() {
    did_run_ = false;
    did_complete_ = false;
  }

 protected:
  ~PerfImageDecodeTaskImpl() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PerfImageDecodeTaskImpl);
};

class PerfRasterTaskImpl : public TileTask {
 public:
  PerfRasterTaskImpl(std::unique_ptr<ScopedResource> resource,
                     TileTask::Vector* dependencies)
      : TileTask(true, dependencies), resource_(std::move(resource)) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {}

  // Overridden from TileTask:
  void ScheduleOnOriginThread(RasterBufferProvider* provider) override {
    // No tile ids are given to support partial updates.
    raster_buffer_ = provider->AcquireBufferForRaster(resource_.get(), 0, 0);
  }
  void CompleteOnOriginThread(RasterBufferProvider* provider) override {
    provider->ReleaseBufferForRaster(std::move(raster_buffer_));
    Reset();
  }

  void Reset() {
    did_run_ = false;
    did_complete_ = false;
  }

 protected:
  ~PerfRasterTaskImpl() override {}

 private:
  std::unique_ptr<ScopedResource> resource_;
  std::unique_ptr<RasterBuffer> raster_buffer_;

  DISALLOW_COPY_AND_ASSIGN(PerfRasterTaskImpl);
};

class TileTaskWorkerPoolPerfTestBase {
 public:
  typedef std::vector<scoped_refptr<TileTask>> RasterTaskVector;

  enum NamedTaskSet { REQUIRED_FOR_ACTIVATION, REQUIRED_FOR_DRAW, ALL };

  TileTaskWorkerPoolPerfTestBase()
      : context_provider_(make_scoped_refptr(new PerfContextProvider)),
        task_runner_(new base::TestSimpleTaskRunner),
        task_graph_runner_(new SynchronousTaskGraphRunner),
        timer_(kWarmupRuns,
               base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  void CreateImageDecodeTasks(unsigned num_image_decode_tasks,
                              TileTask::Vector* image_decode_tasks) {
    for (unsigned i = 0; i < num_image_decode_tasks; ++i)
      image_decode_tasks->push_back(new PerfImageDecodeTaskImpl);
  }

  void CreateRasterTasks(unsigned num_raster_tasks,
                         const TileTask::Vector& image_decode_tasks,
                         RasterTaskVector* raster_tasks) {
    const gfx::Size size(1, 1);

    for (unsigned i = 0; i < num_raster_tasks; ++i) {
      std::unique_ptr<ScopedResource> resource(
          ScopedResource::Create(resource_provider_.get()));
      resource->Allocate(size, ResourceProvider::TEXTURE_HINT_IMMUTABLE,
                         RGBA_8888);

      TileTask::Vector dependencies = image_decode_tasks;
      raster_tasks->push_back(
          new PerfRasterTaskImpl(std::move(resource), &dependencies));
    }
  }

  void BuildTileTaskGraph(TaskGraph* graph,
                          const RasterTaskVector& raster_tasks) {
    uint16_t priority = 0;

    for (auto& raster_task : raster_tasks) {
      priority++;

      for (auto& decode_task : raster_task->dependencies()) {
        graph->nodes.push_back(
            TaskGraph::Node(decode_task.get(), 0u /* group */, priority, 0u));
        graph->edges.push_back(
            TaskGraph::Edge(raster_task.get(), decode_task.get()));
      }

      graph->nodes.push_back(TaskGraph::Node(
          raster_task.get(), 0u /* group */, priority,
          static_cast<uint32_t>(raster_task->dependencies().size())));
    }
  }

 protected:
  scoped_refptr<ContextProvider> context_provider_;
  FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<ResourceProvider> resource_provider_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<SynchronousTaskGraphRunner> task_graph_runner_;
  LapTimer timer_;
};

class TileTaskWorkerPoolPerfTest
    : public TileTaskWorkerPoolPerfTestBase,
      public testing::TestWithParam<TileTaskWorkerPoolType> {
 public:
  // Overridden from testing::Test:
  void SetUp() override {
    switch (GetParam()) {
      case TILE_TASK_WORKER_POOL_TYPE_ZERO_COPY:
        Create3dOutputSurfaceAndResourceProvider();
        tile_task_worker_pool_ = ZeroCopyTileTaskWorkerPool::Create(
            task_runner_.get(), task_graph_runner_.get(),
            resource_provider_.get(), PlatformColor::BestTextureFormat());
        break;
      case TILE_TASK_WORKER_POOL_TYPE_ONE_COPY:
        Create3dOutputSurfaceAndResourceProvider();
        tile_task_worker_pool_ = OneCopyTileTaskWorkerPool::Create(
            task_runner_.get(), task_graph_runner_.get(),
            context_provider_.get(), resource_provider_.get(),
            std::numeric_limits<int>::max(), false,
            std::numeric_limits<int>::max(),
            PlatformColor::BestTextureFormat());
        break;
      case TILE_TASK_WORKER_POOL_TYPE_GPU:
        Create3dOutputSurfaceAndResourceProvider();
        tile_task_worker_pool_ = GpuTileTaskWorkerPool::Create(
            task_runner_.get(), task_graph_runner_.get(),
            context_provider_.get(), resource_provider_.get(), false, 0);
        break;
      case TILE_TASK_WORKER_POOL_TYPE_BITMAP:
        CreateSoftwareOutputSurfaceAndResourceProvider();
        tile_task_worker_pool_ = BitmapTileTaskWorkerPool::Create(
            task_runner_.get(), task_graph_runner_.get(),
            resource_provider_.get());
        break;
    }

    DCHECK(tile_task_worker_pool_);
  }
  void TearDown() override {
    tile_task_worker_pool_->AsTileTaskRunner()->Shutdown();
    tile_task_worker_pool_->AsTileTaskRunner()->CheckForCompletedTasks();
  }

  void RunMessageLoopUntilAllTasksHaveCompleted() {
    task_graph_runner_->RunUntilIdle();
    task_runner_->RunUntilIdle();
  }

  void RunScheduleTasksTest(const std::string& test_name,
                            unsigned num_raster_tasks,
                            unsigned num_image_decode_tasks) {
    TileTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(num_raster_tasks, image_decode_tasks, &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same graph.
    TaskGraph graph;

    timer_.Reset();
    do {
      graph.Reset();
      BuildTileTaskGraph(&graph, raster_tasks);
      tile_task_worker_pool_->AsTileTaskRunner()->ScheduleTasks(&graph);
      tile_task_worker_pool_->AsTileTaskRunner()->CheckForCompletedTasks();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    TaskGraph empty;
    tile_task_worker_pool_->AsTileTaskRunner()->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();

    perf_test::PrintResult("schedule_tasks", TestModifierString(), test_name,
                           timer_.LapsPerSecond(), "runs/s", true);
  }

  void RunScheduleAlternateTasksTest(const std::string& test_name,
                                     unsigned num_raster_tasks,
                                     unsigned num_image_decode_tasks) {
    const size_t kNumVersions = 2;
    TileTask::Vector image_decode_tasks[kNumVersions];
    RasterTaskVector raster_tasks[kNumVersions];
    for (size_t i = 0; i < kNumVersions; ++i) {
      CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks[i]);
      CreateRasterTasks(num_raster_tasks, image_decode_tasks[i],
                        &raster_tasks[i]);
    }

    // Avoid unnecessary heap allocations by reusing the same graph.
    TaskGraph graph;

    size_t count = 0;
    timer_.Reset();
    do {
      graph.Reset();
      BuildTileTaskGraph(&graph, raster_tasks[count % kNumVersions]);
      tile_task_worker_pool_->AsTileTaskRunner()->ScheduleTasks(&graph);
      tile_task_worker_pool_->AsTileTaskRunner()->CheckForCompletedTasks();
      ++count;
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    TaskGraph empty;
    tile_task_worker_pool_->AsTileTaskRunner()->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();

    perf_test::PrintResult("schedule_alternate_tasks", TestModifierString(),
                           test_name, timer_.LapsPerSecond(), "runs/s", true);
  }

  void RunScheduleAndExecuteTasksTest(const std::string& test_name,
                                      unsigned num_raster_tasks,
                                      unsigned num_image_decode_tasks) {
    TileTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(num_raster_tasks, image_decode_tasks, &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same graph.
    TaskGraph graph;

    timer_.Reset();
    do {
      graph.Reset();
      BuildTileTaskGraph(&graph, raster_tasks);
      tile_task_worker_pool_->AsTileTaskRunner()->ScheduleTasks(&graph);
      RunMessageLoopUntilAllTasksHaveCompleted();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    TaskGraph empty;
    tile_task_worker_pool_->AsTileTaskRunner()->ScheduleTasks(&empty);
    RunMessageLoopUntilAllTasksHaveCompleted();

    perf_test::PrintResult("schedule_and_execute_tasks", TestModifierString(),
                           test_name, timer_.LapsPerSecond(), "runs/s", true);
  }

 private:
  void Create3dOutputSurfaceAndResourceProvider() {
    output_surface_ = FakeOutputSurface::Create3d(context_provider_);
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    resource_provider_ = FakeResourceProvider::Create(
        output_surface_.get(), nullptr, &gpu_memory_buffer_manager_);
  }

  void CreateSoftwareOutputSurfaceAndResourceProvider() {
    output_surface_ = FakeOutputSurface::CreateSoftware(
        base::WrapUnique(new SoftwareOutputDevice));
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    resource_provider_ = FakeResourceProvider::Create(
        output_surface_.get(), &shared_bitmap_manager_, nullptr);
  }

  std::string TestModifierString() const {
    switch (GetParam()) {
      case TILE_TASK_WORKER_POOL_TYPE_ZERO_COPY:
        return std::string("_zero_copy_tile_task_worker_pool");
      case TILE_TASK_WORKER_POOL_TYPE_ONE_COPY:
        return std::string("_one_copy_tile_task_worker_pool");
      case TILE_TASK_WORKER_POOL_TYPE_GPU:
        return std::string("_gpu_tile_task_worker_pool");
      case TILE_TASK_WORKER_POOL_TYPE_BITMAP:
        return std::string("_bitmap_tile_task_worker_pool");
    }
    NOTREACHED();
    return std::string();
  }

  std::unique_ptr<TileTaskWorkerPool> tile_task_worker_pool_;
  TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  TestSharedBitmapManager shared_bitmap_manager_;
};

TEST_P(TileTaskWorkerPoolPerfTest, ScheduleTasks) {
  RunScheduleTasksTest("1_0", 1, 0);
  RunScheduleTasksTest("32_0", 32, 0);
  RunScheduleTasksTest("1_1", 1, 1);
  RunScheduleTasksTest("32_1", 32, 1);
  RunScheduleTasksTest("1_4", 1, 4);
  RunScheduleTasksTest("32_4", 32, 4);
}

TEST_P(TileTaskWorkerPoolPerfTest, ScheduleAlternateTasks) {
  RunScheduleAlternateTasksTest("1_0", 1, 0);
  RunScheduleAlternateTasksTest("32_0", 32, 0);
  RunScheduleAlternateTasksTest("1_1", 1, 1);
  RunScheduleAlternateTasksTest("32_1", 32, 1);
  RunScheduleAlternateTasksTest("1_4", 1, 4);
  RunScheduleAlternateTasksTest("32_4", 32, 4);
}

TEST_P(TileTaskWorkerPoolPerfTest, ScheduleAndExecuteTasks) {
  RunScheduleAndExecuteTasksTest("1_0", 1, 0);
  RunScheduleAndExecuteTasksTest("32_0", 32, 0);
  RunScheduleAndExecuteTasksTest("1_1", 1, 1);
  RunScheduleAndExecuteTasksTest("32_1", 32, 1);
  RunScheduleAndExecuteTasksTest("1_4", 1, 4);
  RunScheduleAndExecuteTasksTest("32_4", 32, 4);
}

INSTANTIATE_TEST_CASE_P(TileTaskWorkerPoolPerfTests,
                        TileTaskWorkerPoolPerfTest,
                        ::testing::Values(TILE_TASK_WORKER_POOL_TYPE_ZERO_COPY,
                                          TILE_TASK_WORKER_POOL_TYPE_ONE_COPY,
                                          TILE_TASK_WORKER_POOL_TYPE_GPU,
                                          TILE_TASK_WORKER_POOL_TYPE_BITMAP));

class TileTaskWorkerPoolCommonPerfTest : public TileTaskWorkerPoolPerfTestBase,
                                         public testing::Test {
 public:
  // Overridden from testing::Test:
  void SetUp() override {
    output_surface_ = FakeOutputSurface::Create3d(context_provider_);
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    resource_provider_ =
        FakeResourceProvider::Create(output_surface_.get(), nullptr);
  }

  void RunBuildTileTaskGraphTest(const std::string& test_name,
                                 unsigned num_raster_tasks,
                                 unsigned num_image_decode_tasks) {
    TileTask::Vector image_decode_tasks;
    RasterTaskVector raster_tasks;
    CreateImageDecodeTasks(num_image_decode_tasks, &image_decode_tasks);
    CreateRasterTasks(num_raster_tasks, image_decode_tasks, &raster_tasks);

    // Avoid unnecessary heap allocations by reusing the same graph.
    TaskGraph graph;

    timer_.Reset();
    do {
      graph.Reset();
      BuildTileTaskGraph(&graph, raster_tasks);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("build_raster_task_graph", "", test_name,
                           timer_.LapsPerSecond(), "runs/s", true);
  }
};

TEST_F(TileTaskWorkerPoolCommonPerfTest, BuildTileTaskGraph) {
  RunBuildTileTaskGraphTest("1_0", 1, 0);
  RunBuildTileTaskGraphTest("32_0", 32, 0);
  RunBuildTileTaskGraphTest("1_1", 1, 1);
  RunBuildTileTaskGraphTest("32_1", 32, 1);
  RunBuildTileTaskGraphTest("1_4", 1, 4);
  RunBuildTileTaskGraphTest("32_4", 32, 4);
}

}  // namespace
}  // namespace cc
