// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/raster_buffer_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "cc/base/unique_notifier.h"
#include "cc/raster/bitmap_raster_buffer_provider.h"
#include "cc/raster/gpu_raster_buffer_provider.h"
#include "cc/raster/gpu_rasterizer.h"
#include "cc/raster/one_copy_raster_buffer_provider.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/tiles/tile_task_manager.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

const size_t kMaxBytesPerCopyOperation = 1000U;
const size_t kMaxStagingBuffers = 32U;

enum RasterBufferProviderType {
  RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY,
  RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY,
  RASTER_BUFFER_PROVIDER_TYPE_GPU,
  RASTER_BUFFER_PROVIDER_TYPE_BITMAP
};

class TestRasterTaskImpl : public TileTask {
 public:
  typedef base::Callback<void(bool was_canceled)> Reply;

  TestRasterTaskImpl(const Resource* resource,
                     const Reply& reply,
                     TileTask::Vector* dependencies)
      : TileTask(true, dependencies),
        resource_(resource),
        reply_(reply),
        raster_source_(FakeRasterSource::CreateFilled(gfx::Size(1, 1))) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    uint64_t new_content_id = 0;
    raster_buffer_->Playback(raster_source_.get(), gfx::Rect(1, 1),
                             gfx::Rect(1, 1), new_content_id, 1.f,
                             RasterSource::PlaybackSettings());
  }

  // Overridden from TileTask:
  void ScheduleOnOriginThread(RasterBufferProvider* provider) override {
    // The raster buffer has no tile ids associated with it for partial update,
    // so doesn't need to provide a valid dirty rect.
    raster_buffer_ = provider->AcquireBufferForRaster(resource_, 0, 0);
  }
  void CompleteOnOriginThread(RasterBufferProvider* provider) override {
    provider->ReleaseBufferForRaster(std::move(raster_buffer_));
    reply_.Run(!state().IsFinished());
  }

 protected:
  ~TestRasterTaskImpl() override {}

 private:
  const Resource* resource_;
  const Reply reply_;
  std::unique_ptr<RasterBuffer> raster_buffer_;
  scoped_refptr<RasterSource> raster_source_;

  DISALLOW_COPY_AND_ASSIGN(TestRasterTaskImpl);
};

class BlockingTestRasterTaskImpl : public TestRasterTaskImpl {
 public:
  BlockingTestRasterTaskImpl(const Resource* resource,
                             const Reply& reply,
                             base::Lock* lock,
                             TileTask::Vector* dependencies)
      : TestRasterTaskImpl(resource, reply, dependencies), lock_(lock) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    base::AutoLock lock(*lock_);
    TestRasterTaskImpl::RunOnWorkerThread();
  }

 protected:
  ~BlockingTestRasterTaskImpl() override {}

 private:
  base::Lock* lock_;

  DISALLOW_COPY_AND_ASSIGN(BlockingTestRasterTaskImpl);
};

class RasterBufferProviderTest
    : public testing::TestWithParam<RasterBufferProviderType> {
 public:
  struct RasterTaskResult {
    unsigned id;
    bool canceled;
  };

  typedef std::vector<scoped_refptr<TileTask>> RasterTaskVector;

  enum NamedTaskSet { REQUIRED_FOR_ACTIVATION, REQUIRED_FOR_DRAW, ALL };

  RasterBufferProviderTest()
      : context_provider_(TestContextProvider::Create()),
        worker_context_provider_(TestContextProvider::CreateWorker()),
        all_tile_tasks_finished_(
            base::ThreadTaskRunnerHandle::Get().get(),
            base::Bind(&RasterBufferProviderTest::AllTileTasksFinished,
                       base::Unretained(this))),
        timeout_seconds_(5),
        timed_out_(false) {}

  // Overridden from testing::Test:
  void SetUp() override {
    std::unique_ptr<RasterBufferProvider> raster_buffer_provider;
    switch (GetParam()) {
      case RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY:
        Create3dOutputSurfaceAndResourceProvider();
        raster_buffer_provider = ZeroCopyRasterBufferProvider::Create(
            resource_provider_.get(), PlatformColor::BestTextureFormat());
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY:
        Create3dOutputSurfaceAndResourceProvider();
        raster_buffer_provider = OneCopyRasterBufferProvider::Create(
            base::ThreadTaskRunnerHandle::Get().get(), context_provider_.get(),
            resource_provider_.get(), kMaxBytesPerCopyOperation, false,
            kMaxStagingBuffers, PlatformColor::BestTextureFormat());
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_GPU:
        Create3dOutputSurfaceAndResourceProvider();
        raster_buffer_provider = GpuRasterBufferProvider::Create(
            context_provider_.get(), resource_provider_.get(), false, 0);
        break;
      case RASTER_BUFFER_PROVIDER_TYPE_BITMAP:
        CreateSoftwareOutputSurfaceAndResourceProvider();
        raster_buffer_provider =
            BitmapRasterBufferProvider::Create(resource_provider_.get());
        break;
    }

    DCHECK(raster_buffer_provider);

    tile_task_manager_ = TileTaskManagerImpl::Create(
        std::move(raster_buffer_provider), &task_graph_runner_);
  }

  void TearDown() override {
    tile_task_manager_->Shutdown();
    tile_task_manager_->CheckForCompletedTasks();
  }

  void AllTileTasksFinished() {
    tile_task_manager_->CheckForCompletedTasks();
    base::MessageLoop::current()->QuitWhenIdle();
  }

  void RunMessageLoopUntilAllTasksHaveCompleted() {
    task_graph_runner_.RunUntilIdle();
    tile_task_manager_->CheckForCompletedTasks();
  }

  void ScheduleTasks() {
    graph_.Reset();

    size_t priority = 0;

    for (RasterTaskVector::const_iterator it = tasks_.begin();
         it != tasks_.end(); ++it) {
      graph_.nodes.emplace_back(it->get(), 0 /* group */, priority++,
                                0 /* dependencies */);
    }

    tile_task_manager_->ScheduleTasks(&graph_);
  }

  void AppendTask(unsigned id, const gfx::Size& size) {
    std::unique_ptr<ScopedResource> resource(
        ScopedResource::Create(resource_provider_.get()));
    resource->Allocate(size, ResourceProvider::TEXTURE_HINT_IMMUTABLE,
                       RGBA_8888);
    const Resource* const_resource = resource.get();

    TileTask::Vector empty;
    tasks_.push_back(new TestRasterTaskImpl(
        const_resource,
        base::Bind(&RasterBufferProviderTest::OnTaskCompleted,
                   base::Unretained(this), base::Passed(&resource), id),
        &empty));
  }

  void AppendTask(unsigned id) { AppendTask(id, gfx::Size(1, 1)); }

  void AppendBlockingTask(unsigned id, base::Lock* lock) {
    const gfx::Size size(1, 1);

    std::unique_ptr<ScopedResource> resource(
        ScopedResource::Create(resource_provider_.get()));
    resource->Allocate(size, ResourceProvider::TEXTURE_HINT_IMMUTABLE,
                       RGBA_8888);
    const Resource* const_resource = resource.get();

    TileTask::Vector empty;
    tasks_.push_back(new BlockingTestRasterTaskImpl(
        const_resource,
        base::Bind(&RasterBufferProviderTest::OnTaskCompleted,
                   base::Unretained(this), base::Passed(&resource), id),
        lock, &empty));
  }

  const std::vector<RasterTaskResult>& completed_tasks() const {
    return completed_tasks_;
  }

  void LoseContext(ContextProvider* context_provider) {
    if (!context_provider)
      return;
    context_provider->ContextGL()->LoseContextCHROMIUM(
        GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
    context_provider->ContextGL()->Flush();
  }

 private:
  void Create3dOutputSurfaceAndResourceProvider() {
    output_surface_ = FakeOutputSurface::Create3d(context_provider_,
                                                  worker_context_provider_);
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    TestWebGraphicsContext3D* context3d = context_provider_->TestContext3d();
    context3d->set_support_sync_query(true);
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

  void OnTaskCompleted(std::unique_ptr<ScopedResource> resource,
                       unsigned id,
                       bool was_canceled) {
    RasterTaskResult result;
    result.id = id;
    result.canceled = was_canceled;
    completed_tasks_.push_back(result);
  }

  void OnTimeout() {
    timed_out_ = true;
    base::MessageLoop::current()->QuitWhenIdle();
  }

 protected:
  scoped_refptr<TestContextProvider> context_provider_;
  scoped_refptr<TestContextProvider> worker_context_provider_;
  FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<ResourceProvider> resource_provider_;
  std::unique_ptr<TileTaskManager> tile_task_manager_;
  TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  TestSharedBitmapManager shared_bitmap_manager_;
  SynchronousTaskGraphRunner task_graph_runner_;
  base::CancelableClosure timeout_;
  UniqueNotifier all_tile_tasks_finished_;
  int timeout_seconds_;
  bool timed_out_;
  RasterTaskVector tasks_;
  std::vector<RasterTaskResult> completed_tasks_;
  TaskGraph graph_;
};

TEST_P(RasterBufferProviderTest, Basic) {
  AppendTask(0u);
  AppendTask(1u);
  ScheduleTasks();

  RunMessageLoopUntilAllTasksHaveCompleted();

  ASSERT_EQ(2u, completed_tasks().size());
  EXPECT_FALSE(completed_tasks()[0].canceled);
  EXPECT_FALSE(completed_tasks()[1].canceled);
}

TEST_P(RasterBufferProviderTest, FailedMapResource) {
  if (GetParam() == RASTER_BUFFER_PROVIDER_TYPE_BITMAP)
    return;

  TestWebGraphicsContext3D* context3d = context_provider_->TestContext3d();
  context3d->set_times_map_buffer_chromium_succeeds(0);
  AppendTask(0u);
  ScheduleTasks();

  RunMessageLoopUntilAllTasksHaveCompleted();

  ASSERT_EQ(1u, completed_tasks().size());
  EXPECT_FALSE(completed_tasks()[0].canceled);
}

// This test checks that replacing a pending raster task with another does
// not prevent the DidFinishRunningTileTasks notification from being sent.
TEST_P(RasterBufferProviderTest, FalseThrottling) {
  base::Lock lock;

  // Schedule a task that is prevented from completing with a lock.
  lock.Acquire();
  AppendBlockingTask(0u, &lock);
  ScheduleTasks();

  // Schedule another task to replace the still-pending task. Because the old
  // task is not a throttled task in the new task set, it should not prevent
  // DidFinishRunningTileTasks from getting signaled.
  RasterTaskVector tasks;
  tasks.swap(tasks_);
  AppendTask(1u);
  ScheduleTasks();

  // Unblock the first task to allow the second task to complete.
  lock.Release();

  RunMessageLoopUntilAllTasksHaveCompleted();
}

TEST_P(RasterBufferProviderTest, LostContext) {
  LoseContext(output_surface_->context_provider());
  LoseContext(output_surface_->worker_context_provider());

  AppendTask(0u);
  AppendTask(1u);
  ScheduleTasks();

  RunMessageLoopUntilAllTasksHaveCompleted();

  ASSERT_EQ(2u, completed_tasks().size());
  EXPECT_FALSE(completed_tasks()[0].canceled);
  EXPECT_FALSE(completed_tasks()[1].canceled);
}

INSTANTIATE_TEST_CASE_P(RasterBufferProviderTests,
                        RasterBufferProviderTest,
                        ::testing::Values(RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY,
                                          RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY,
                                          RASTER_BUFFER_PROVIDER_TYPE_GPU,
                                          RASTER_BUFFER_PROVIDER_TYPE_BITMAP));

}  // namespace
}  // namespace cc
