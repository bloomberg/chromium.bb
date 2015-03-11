// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile_task_worker_pool.h"

#include <limits>
#include <vector>

#include "base/cancelable_callback.h"
#include "cc/base/unique_notifier.h"
#include "cc/resources/bitmap_tile_task_worker_pool.h"
#include "cc/resources/gpu_rasterizer.h"
#include "cc/resources/gpu_tile_task_worker_pool.h"
#include "cc/resources/one_copy_tile_task_worker_pool.h"
#include "cc/resources/picture_pile.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/pixel_buffer_tile_task_worker_pool.h"
#include "cc/resources/raster_buffer.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"
#include "cc/resources/tile_task_runner.h"
#include "cc/resources/zero_copy_tile_task_worker_pool.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

const size_t kMaxTransferBufferUsageBytes = 10000U;
// A resource of this dimension^2 * 4 must be greater than the above transfer
// buffer constant.
const size_t kLargeResourceDimension = 1000U;

enum TileTaskWorkerPoolType {
  TILE_TASK_WORKER_POOL_TYPE_PIXEL_BUFFER,
  TILE_TASK_WORKER_POOL_TYPE_ZERO_COPY,
  TILE_TASK_WORKER_POOL_TYPE_ONE_COPY,
  TILE_TASK_WORKER_POOL_TYPE_GPU,
  TILE_TASK_WORKER_POOL_TYPE_BITMAP
};

class TestRasterTaskImpl : public RasterTask {
 public:
  typedef base::Callback<void(const RasterSource::SolidColorAnalysis& analysis,
                              bool was_canceled)> Reply;

  TestRasterTaskImpl(const Resource* resource,
                     const Reply& reply,
                     ImageDecodeTask::Vector* dependencies)
      : RasterTask(resource, dependencies),
        reply_(reply),
        picture_pile_(FakePicturePileImpl::CreateEmptyPile(gfx::Size(1, 1),
                                                           gfx::Size(1, 1))) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    raster_buffer_->Playback(picture_pile_.get(), gfx::Rect(0, 0, 1, 1), 1.0);
  }

  // Overridden from TileTask:
  void ScheduleOnOriginThread(TileTaskClient* client) override {
    raster_buffer_ = client->AcquireBufferForRaster(resource());
  }
  void CompleteOnOriginThread(TileTaskClient* client) override {
    client->ReleaseBufferForRaster(raster_buffer_.Pass());
  }
  void RunReplyOnOriginThread() override {
    reply_.Run(RasterSource::SolidColorAnalysis(), !HasFinishedRunning());
  }

 protected:
  ~TestRasterTaskImpl() override {}

 private:
  const Reply reply_;
  scoped_ptr<RasterBuffer> raster_buffer_;
  scoped_refptr<PicturePileImpl> picture_pile_;

  DISALLOW_COPY_AND_ASSIGN(TestRasterTaskImpl);
};

class BlockingTestRasterTaskImpl : public TestRasterTaskImpl {
 public:
  BlockingTestRasterTaskImpl(const Resource* resource,
                             const Reply& reply,
                             base::Lock* lock,
                             ImageDecodeTask::Vector* dependencies)
      : TestRasterTaskImpl(resource, reply, dependencies), lock_(lock) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    base::AutoLock lock(*lock_);
    TestRasterTaskImpl::RunOnWorkerThread();
  }

  // Overridden from TileTask:
  void RunReplyOnOriginThread() override {}

 protected:
  ~BlockingTestRasterTaskImpl() override {}

 private:
  base::Lock* lock_;

  DISALLOW_COPY_AND_ASSIGN(BlockingTestRasterTaskImpl);
};

class TileTaskWorkerPoolTest
    : public testing::TestWithParam<TileTaskWorkerPoolType>,
      public TileTaskRunnerClient {
 public:
  struct RasterTaskResult {
    unsigned id;
    bool canceled;
  };

  typedef std::vector<scoped_refptr<RasterTask>> RasterTaskVector;

  enum NamedTaskSet { REQUIRED_FOR_ACTIVATION, REQUIRED_FOR_DRAW, ALL };

  TileTaskWorkerPoolTest()
      : context_provider_(TestContextProvider::Create()),
        worker_context_provider_(TestContextProvider::Create()),
        all_tile_tasks_finished_(
            base::MessageLoopProxy::current().get(),
            base::Bind(&TileTaskWorkerPoolTest::AllTileTasksFinished,
                       base::Unretained(this))),
        timeout_seconds_(5),
        timed_out_(false) {}

  // Overridden from testing::Test:
  void SetUp() override {
    switch (GetParam()) {
      case TILE_TASK_WORKER_POOL_TYPE_PIXEL_BUFFER:
        Create3dOutputSurfaceAndResourceProvider();
        tile_task_worker_pool_ = PixelBufferTileTaskWorkerPool::Create(
            base::MessageLoopProxy::current().get(),
            TileTaskWorkerPool::GetTaskGraphRunner(), context_provider_.get(),
            resource_provider_.get(), kMaxTransferBufferUsageBytes);
        break;
      case TILE_TASK_WORKER_POOL_TYPE_ZERO_COPY:
        Create3dOutputSurfaceAndResourceProvider();
        tile_task_worker_pool_ = ZeroCopyTileTaskWorkerPool::Create(
            base::MessageLoopProxy::current().get(),
            TileTaskWorkerPool::GetTaskGraphRunner(), resource_provider_.get());
        break;
      case TILE_TASK_WORKER_POOL_TYPE_ONE_COPY:
        Create3dOutputSurfaceAndResourceProvider();
        staging_resource_pool_ = ResourcePool::Create(resource_provider_.get(),
                                                      GL_TEXTURE_2D);
        tile_task_worker_pool_ = OneCopyTileTaskWorkerPool::Create(
            base::MessageLoopProxy::current().get(),
            TileTaskWorkerPool::GetTaskGraphRunner(), context_provider_.get(),
            resource_provider_.get(), staging_resource_pool_.get());
        break;
      case TILE_TASK_WORKER_POOL_TYPE_GPU:
        Create3dOutputSurfaceAndResourceProvider();
        rasterizer_ = GpuRasterizer::Create(
            context_provider_.get(), resource_provider_.get(), false, false, 0);
        tile_task_worker_pool_ = GpuTileTaskWorkerPool::Create(
            base::MessageLoopProxy::current().get(),
            TileTaskWorkerPool::GetTaskGraphRunner(),
            static_cast<GpuRasterizer*>(rasterizer_.get()));
        break;
      case TILE_TASK_WORKER_POOL_TYPE_BITMAP:
        CreateSoftwareOutputSurfaceAndResourceProvider();
        tile_task_worker_pool_ = BitmapTileTaskWorkerPool::Create(
            base::MessageLoopProxy::current().get(),
            TileTaskWorkerPool::GetTaskGraphRunner(), resource_provider_.get());
        break;
    }

    DCHECK(tile_task_worker_pool_);
    tile_task_worker_pool_->AsTileTaskRunner()->SetClient(this);
  }

  void TearDown() override {
    tile_task_worker_pool_->AsTileTaskRunner()->Shutdown();
    tile_task_worker_pool_->AsTileTaskRunner()->CheckForCompletedTasks();
  }

  void AllTileTasksFinished() {
    tile_task_worker_pool_->AsTileTaskRunner()->CheckForCompletedTasks();
    base::MessageLoop::current()->Quit();
  }

  // Overriden from TileTaskWorkerPoolClient:
  void DidFinishRunningTileTasks(TaskSet task_set) override {
    EXPECT_FALSE(completed_task_sets_[task_set]);
    completed_task_sets_[task_set] = true;
    if (task_set == ALL) {
      EXPECT_TRUE((~completed_task_sets_).none());
      all_tile_tasks_finished_.Schedule();
    }
  }

  TaskSetCollection TasksThatShouldBeForcedToComplete() const override {
    return TaskSetCollection();
  }

  void RunMessageLoopUntilAllTasksHaveCompleted() {
    if (timeout_seconds_) {
      timeout_.Reset(base::Bind(&TileTaskWorkerPoolTest::OnTimeout,
                                base::Unretained(this)));
      base::MessageLoopProxy::current()->PostDelayedTask(
          FROM_HERE, timeout_.callback(),
          base::TimeDelta::FromSeconds(timeout_seconds_));
    }

    base::MessageLoop::current()->Run();

    timeout_.Cancel();

    ASSERT_FALSE(timed_out_) << "Test timed out";
  }

  void ScheduleTasks() {
    TileTaskQueue queue;

    for (RasterTaskVector::const_iterator it = tasks_.begin();
         it != tasks_.end(); ++it) {
      TaskSetCollection task_sets;
      task_sets[REQUIRED_FOR_ACTIVATION] = true;
      task_sets[REQUIRED_FOR_DRAW] = true;
      task_sets[ALL] = true;
      queue.items.push_back(TileTaskQueue::Item(it->get(), task_sets));
    }

    completed_task_sets_.reset();
    tile_task_worker_pool_->AsTileTaskRunner()->ScheduleTasks(&queue);
  }

  void AppendTask(unsigned id, const gfx::Size& size) {
    scoped_ptr<ScopedResource> resource(
        ScopedResource::Create(resource_provider_.get()));
    resource->Allocate(size, ResourceProvider::TEXTURE_HINT_IMMUTABLE,
                       RGBA_8888);
    const Resource* const_resource = resource.get();

    ImageDecodeTask::Vector empty;
    tasks_.push_back(new TestRasterTaskImpl(
        const_resource,
        base::Bind(&TileTaskWorkerPoolTest::OnTaskCompleted,
                   base::Unretained(this), base::Passed(&resource), id),
        &empty));
  }

  void AppendTask(unsigned id) { AppendTask(id, gfx::Size(1, 1)); }

  void AppendBlockingTask(unsigned id, base::Lock* lock) {
    const gfx::Size size(1, 1);

    scoped_ptr<ScopedResource> resource(
        ScopedResource::Create(resource_provider_.get()));
    resource->Allocate(size, ResourceProvider::TEXTURE_HINT_IMMUTABLE,
                       RGBA_8888);
    const Resource* const_resource = resource.get();

    ImageDecodeTask::Vector empty;
    tasks_.push_back(new BlockingTestRasterTaskImpl(
        const_resource,
        base::Bind(&TileTaskWorkerPoolTest::OnTaskCompleted,
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
    output_surface_ = FakeOutputSurface::Create3d(
                          context_provider_, worker_context_provider_).Pass();
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    TestWebGraphicsContext3D* context3d = context_provider_->TestContext3d();
    context3d->set_support_sync_query(true);
    resource_provider_ = ResourceProvider::Create(output_surface_.get(), NULL,
                                                  &gpu_memory_buffer_manager_,
                                                  NULL, 0, false, 1).Pass();
  }

  void CreateSoftwareOutputSurfaceAndResourceProvider() {
    output_surface_ = FakeOutputSurface::CreateSoftware(
        make_scoped_ptr(new SoftwareOutputDevice));
    CHECK(output_surface_->BindToClient(&output_surface_client_));
    resource_provider_ =
        ResourceProvider::Create(output_surface_.get(), &shared_bitmap_manager_,
                                 NULL, NULL, 0, false, 1).Pass();
  }

  void OnTaskCompleted(scoped_ptr<ScopedResource> resource,
                       unsigned id,
                       const RasterSource::SolidColorAnalysis& analysis,
                       bool was_canceled) {
    RasterTaskResult result;
    result.id = id;
    result.canceled = was_canceled;
    completed_tasks_.push_back(result);
  }

  void OnTimeout() {
    timed_out_ = true;
    base::MessageLoop::current()->Quit();
  }

 protected:
  scoped_refptr<TestContextProvider> context_provider_;
  scoped_refptr<TestContextProvider> worker_context_provider_;
  scoped_ptr<Rasterizer> rasterizer_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<ResourcePool> staging_resource_pool_;
  scoped_ptr<TileTaskWorkerPool> tile_task_worker_pool_;
  TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  TestSharedBitmapManager shared_bitmap_manager_;
  base::CancelableClosure timeout_;
  UniqueNotifier all_tile_tasks_finished_;
  int timeout_seconds_;
  bool timed_out_;
  RasterTaskVector tasks_;
  std::vector<RasterTaskResult> completed_tasks_;
  TaskSetCollection completed_task_sets_;
};

TEST_P(TileTaskWorkerPoolTest, Basic) {
  AppendTask(0u);
  AppendTask(1u);
  ScheduleTasks();

  RunMessageLoopUntilAllTasksHaveCompleted();

  ASSERT_EQ(2u, completed_tasks().size());
  EXPECT_FALSE(completed_tasks()[0].canceled);
  EXPECT_FALSE(completed_tasks()[1].canceled);
}

TEST_P(TileTaskWorkerPoolTest, FailedMapResource) {
  if (GetParam() == TILE_TASK_WORKER_POOL_TYPE_BITMAP)
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
TEST_P(TileTaskWorkerPoolTest, FalseThrottling) {
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

TEST_P(TileTaskWorkerPoolTest, LargeResources) {
  gfx::Size size(kLargeResourceDimension, kLargeResourceDimension);

  {
    // Verify a resource of this size is larger than the transfer buffer.
    scoped_ptr<ScopedResource> resource(
        ScopedResource::Create(resource_provider_.get()));
    resource->Allocate(size, ResourceProvider::TEXTURE_HINT_IMMUTABLE,
                       RGBA_8888);
    EXPECT_GE(resource->bytes(), kMaxTransferBufferUsageBytes);
  }

  AppendTask(0u, size);
  AppendTask(1u, size);
  AppendTask(2u, size);
  ScheduleTasks();

  // This will time out if a resource that is larger than the throttle limit
  // never gets scheduled.
  RunMessageLoopUntilAllTasksHaveCompleted();
}

TEST_P(TileTaskWorkerPoolTest, LostContext) {
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

INSTANTIATE_TEST_CASE_P(
    TileTaskWorkerPoolTests,
    TileTaskWorkerPoolTest,
    ::testing::Values(TILE_TASK_WORKER_POOL_TYPE_PIXEL_BUFFER,
                      TILE_TASK_WORKER_POOL_TYPE_ZERO_COPY,
                      TILE_TASK_WORKER_POOL_TYPE_ONE_COPY,
                      TILE_TASK_WORKER_POOL_TYPE_GPU,
                      TILE_TASK_WORKER_POOL_TYPE_BITMAP));

}  // namespace
}  // namespace cc
