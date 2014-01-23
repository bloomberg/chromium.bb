// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include <limits>
#include <vector>

#include "cc/resources/image_raster_worker_pool.h"
#include "cc/resources/picture_pile.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/pixel_buffer_raster_worker_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class TestRasterWorkerPoolTaskImpl : public internal::RasterWorkerPoolTask {
 public:
  enum RasterThread {
    RASTER_THREAD_NONE,
    RASTER_THREAD_ORIGIN,
    RASTER_THREAD_WORKER
  };
  typedef base::Callback<void(const PicturePileImpl::Analysis& analysis,
                              bool was_canceled,
                              RasterThread raster_thread)> Reply;

  TestRasterWorkerPoolTaskImpl(
      const Resource* resource,
      const Reply& reply,
      internal::Task::Vector* dependencies,
      bool use_gpu_rasterization)
      : internal::RasterWorkerPoolTask(resource,
                                       dependencies,
                                       use_gpu_rasterization),
        reply_(reply),
        raster_thread_(RASTER_THREAD_NONE) {}

  // Overridden from internal::RasterWorkerPoolTask:
  virtual bool RunOnWorkerThread(unsigned thread_index,
                                 void* buffer,
                                 gfx::Size size,
                                 int stride) OVERRIDE {
    raster_thread_ = RASTER_THREAD_WORKER;
    return true;
  }
  virtual void RunOnOriginThread(ResourceProvider* resource_provider,
                                 ContextProvider* context_provider) OVERRIDE {
    raster_thread_ = RASTER_THREAD_ORIGIN;
  }
  virtual void CompleteOnOriginThread() OVERRIDE {
    reply_.Run(PicturePileImpl::Analysis(),
               !HasFinishedRunning() || WasCanceled(),
               raster_thread_);
  }

 protected:
  virtual ~TestRasterWorkerPoolTaskImpl() {}

 private:
  const Reply reply_;
  RasterThread raster_thread_;

  DISALLOW_COPY_AND_ASSIGN(TestRasterWorkerPoolTaskImpl);
};

class BlockingRasterWorkerPoolTaskImpl : public TestRasterWorkerPoolTaskImpl {
 public:
  BlockingRasterWorkerPoolTaskImpl(const Resource* resource,
                                   const Reply& reply,
                                   base::Lock* lock,
                                   internal::Task::Vector* dependencies,
                                   bool use_gpu_rasterization)
      : TestRasterWorkerPoolTaskImpl(resource,
                                     reply,
                                     dependencies,
                                     use_gpu_rasterization),
        lock_(lock) {}

  // Overridden from internal::RasterWorkerPoolTask:
  virtual bool RunOnWorkerThread(unsigned thread_index,
                                 void* buffer,
                                 gfx::Size size,
                                 int stride) OVERRIDE {
    base::AutoLock lock(*lock_);
    return TestRasterWorkerPoolTaskImpl::RunOnWorkerThread(
        thread_index, buffer, size, stride);
  }
  virtual void CompleteOnOriginThread() OVERRIDE {}

 protected:
  virtual ~BlockingRasterWorkerPoolTaskImpl() {}

 private:
  base::Lock* lock_;

  DISALLOW_COPY_AND_ASSIGN(BlockingRasterWorkerPoolTaskImpl);
};

class RasterWorkerPoolTest : public testing::Test,
                             public RasterWorkerPoolClient  {
 public:
  RasterWorkerPoolTest()
      : context_provider_(TestContextProvider::Create()),
        check_interval_milliseconds_(1),
        timeout_seconds_(5),
        timed_out_(false) {
    output_surface_ = FakeOutputSurface::Create3d(context_provider_).Pass();
    CHECK(output_surface_->BindToClient(&output_surface_client_));

    resource_provider_ = ResourceProvider::Create(
        output_surface_.get(), NULL, 0, false, 1).Pass();
  }
  virtual ~RasterWorkerPoolTest() {
    resource_provider_.reset();
  }

  // Overridden from testing::Test:
  virtual void TearDown() OVERRIDE {
    if (!raster_worker_pool_)
      return;
    raster_worker_pool_->Shutdown();
    raster_worker_pool_->CheckForCompletedTasks();
  }

  // Overriden from RasterWorkerPoolClient:
  virtual bool ShouldForceTasksRequiredForActivationToComplete() const
      OVERRIDE {
    return false;
  }
  virtual void DidFinishRunningTasks() OVERRIDE {}
  virtual void DidFinishRunningTasksRequiredForActivation() OVERRIDE {}

  virtual void BeginTest() = 0;
  virtual void AfterTest() = 0;

  ResourceProvider* resource_provider() const {
    return resource_provider_.get();
  }

  RasterWorkerPool* worker_pool() {
    return raster_worker_pool_.get();
  }

  void RunTest(bool use_map_image) {
    if (use_map_image) {
      raster_worker_pool_ = ImageRasterWorkerPool::Create(
          resource_provider(), context_provider_.get(), GL_TEXTURE_2D);
    } else {
      raster_worker_pool_ =
          PixelBufferRasterWorkerPool::Create(
              resource_provider(),
              context_provider_.get(),
              std::numeric_limits<size_t>::max());
    }

    raster_worker_pool_->SetClient(this);

    BeginTest();

    ScheduleCheckForCompletedTasks();

    if (timeout_seconds_) {
      timeout_.Reset(base::Bind(&RasterWorkerPoolTest::OnTimeout,
                                base::Unretained(this)));
      base::MessageLoopProxy::current()->PostDelayedTask(
          FROM_HERE,
          timeout_.callback(),
          base::TimeDelta::FromSeconds(timeout_seconds_));
    }

    base::MessageLoop::current()->Run();

    check_.Cancel();
    timeout_.Cancel();

    if (timed_out_) {
      FAIL() << "Test timed out";
      return;
    }
    AfterTest();
  }

  void EndTest() {
    base::MessageLoop::current()->Quit();
  }

  void ScheduleTasks() {
    RasterWorkerPool::RasterTask::Queue tasks;

    for (std::vector<RasterWorkerPool::RasterTask>::iterator it =
             tasks_.begin();
         it != tasks_.end(); ++it)
      tasks.Append(*it, false);

    worker_pool()->ScheduleTasks(&tasks);
  }

  void AppendTask(unsigned id, bool use_gpu_rasterization) {
    const gfx::Size size(1, 1);

    scoped_ptr<ScopedResource> resource(
        ScopedResource::Create(resource_provider()));
    resource->Allocate(size, ResourceProvider::TextureUsageAny, RGBA_8888);
    const Resource* const_resource = resource.get();

    RasterWorkerPool::Task::Set empty;
    tasks_.push_back(
        RasterWorkerPool::RasterTask(new TestRasterWorkerPoolTaskImpl(
            const_resource,
            base::Bind(&RasterWorkerPoolTest::OnTaskCompleted,
                       base::Unretained(this),
                       base::Passed(&resource),
                       id),
            &empty.tasks_,
            use_gpu_rasterization)));
  }

  void AppendBlockingTask(unsigned id, base::Lock* lock) {
    const gfx::Size size(1, 1);

    scoped_ptr<ScopedResource> resource(
        ScopedResource::Create(resource_provider()));
    resource->Allocate(size, ResourceProvider::TextureUsageAny, RGBA_8888);
    const Resource* const_resource = resource.get();

    RasterWorkerPool::Task::Set empty;
    tasks_.push_back(
        RasterWorkerPool::RasterTask(new BlockingRasterWorkerPoolTaskImpl(
            const_resource,
            base::Bind(&RasterWorkerPoolTest::OnTaskCompleted,
                       base::Unretained(this),
                       base::Passed(&resource),
                       id),
            lock,
            &empty.tasks_,
            false)));
  }

  virtual void OnTaskCompleted(
      scoped_ptr<ScopedResource> resource,
      unsigned id,
      const PicturePileImpl::Analysis& analysis,
      bool was_canceled,
      TestRasterWorkerPoolTaskImpl::RasterThread raster_thread) {}

 private:
  void ScheduleCheckForCompletedTasks() {
    check_.Reset(base::Bind(&RasterWorkerPoolTest::OnCheckForCompletedTasks,
                            base::Unretained(this)));
    base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE,
        check_.callback(),
        base::TimeDelta::FromMilliseconds(check_interval_milliseconds_));
  }

  void OnCheckForCompletedTasks() {
    raster_worker_pool_->CheckForCompletedTasks();
    ScheduleCheckForCompletedTasks();
  }

  void OnTimeout() {
    timed_out_ = true;
    base::MessageLoop::current()->Quit();
  }

 protected:
  scoped_refptr<TestContextProvider> context_provider_;
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<RasterWorkerPool> raster_worker_pool_;
  base::CancelableClosure check_;
  int check_interval_milliseconds_;
  base::CancelableClosure timeout_;
  int timeout_seconds_;
  bool timed_out_;
  std::vector<RasterWorkerPool::RasterTask> tasks_;
};

namespace {

#define PIXEL_BUFFER_TEST_F(TEST_FIXTURE_NAME)                  \
  TEST_F(TEST_FIXTURE_NAME, RunPixelBuffer) {                   \
    RunTest(false);                                             \
  }

#define IMAGE_TEST_F(TEST_FIXTURE_NAME)                         \
  TEST_F(TEST_FIXTURE_NAME, RunImage) {                         \
    RunTest(true);                                              \
  }

#define PIXEL_BUFFER_AND_IMAGE_TEST_F(TEST_FIXTURE_NAME)        \
  PIXEL_BUFFER_TEST_F(TEST_FIXTURE_NAME);                       \
  IMAGE_TEST_F(TEST_FIXTURE_NAME)

class RasterWorkerPoolTestSofwareRaster : public RasterWorkerPoolTest {
 public:
  virtual void OnTaskCompleted(
      scoped_ptr<ScopedResource> resource,
      unsigned id,
      const PicturePileImpl::Analysis& analysis,
      bool was_canceled,
      TestRasterWorkerPoolTaskImpl::RasterThread raster_thread) OVERRIDE {
    EXPECT_FALSE(was_canceled);
    EXPECT_EQ(TestRasterWorkerPoolTaskImpl::RASTER_THREAD_WORKER,
              raster_thread);
    on_task_completed_ids_.push_back(id);
    if (on_task_completed_ids_.size() == 2)
      EndTest();
  }

  // Overridden from RasterWorkerPoolTest:
  virtual void BeginTest() OVERRIDE {
    AppendTask(0u, false);
    AppendTask(1u, false);
    ScheduleTasks();
  }
  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(2u, on_task_completed_ids_.size());
    tasks_.clear();
  }

  std::vector<unsigned> on_task_completed_ids_;
};

PIXEL_BUFFER_AND_IMAGE_TEST_F(RasterWorkerPoolTestSofwareRaster);

class RasterWorkerPoolTestGpuRaster : public RasterWorkerPoolTest {
 public:
  virtual void OnTaskCompleted(
      scoped_ptr<ScopedResource> resource,
      unsigned id,
      const PicturePileImpl::Analysis& analysis,
      bool was_canceled,
      TestRasterWorkerPoolTaskImpl::RasterThread raster_thread) OVERRIDE {
    EXPECT_EQ(0u, id);
    EXPECT_FALSE(was_canceled);
    EXPECT_EQ(TestRasterWorkerPoolTaskImpl::RASTER_THREAD_ORIGIN,
              raster_thread);
    EndTest();
  }

  // Overridden from RasterWorkerPoolTest:
  virtual void BeginTest() OVERRIDE {
    AppendTask(0u, true);  // GPU raster.
    ScheduleTasks();
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(1u, tasks_.size());
  }
};
PIXEL_BUFFER_AND_IMAGE_TEST_F(RasterWorkerPoolTestGpuRaster);

class RasterWorkerPoolTestHybridRaster : public RasterWorkerPoolTest {
 public:
  virtual void OnTaskCompleted(
      scoped_ptr<ScopedResource> resource,
      unsigned id,
      const PicturePileImpl::Analysis& analysis,
      bool was_canceled,
      TestRasterWorkerPoolTaskImpl::RasterThread raster_thread) OVERRIDE {
    EXPECT_FALSE(was_canceled);
    switch (id) {
      case 0u:
        EXPECT_EQ(TestRasterWorkerPoolTaskImpl::RASTER_THREAD_WORKER,
                  raster_thread);
        break;
      case 1u:
        EXPECT_EQ(TestRasterWorkerPoolTaskImpl::RASTER_THREAD_ORIGIN,
                  raster_thread);
        break;
      default:
        NOTREACHED();
    }
    completed_task_ids_.push_back(id);
    if (completed_task_ids_.size() == 2)
      EndTest();
  }

  // Overridden from RasterWorkerPoolTest:
  virtual void BeginTest() OVERRIDE {
    AppendTask(0u, false);  // Software raster.
    AppendTask(1u, true);  // GPU raster.
    ScheduleTasks();
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(2u, tasks_.size());
    EXPECT_EQ(2u, completed_task_ids_.size());
  }

  std::vector<unsigned> completed_task_ids_;
};
PIXEL_BUFFER_AND_IMAGE_TEST_F(RasterWorkerPoolTestHybridRaster);

class RasterWorkerPoolTestFailedMapResource : public RasterWorkerPoolTest {
 public:
  virtual void OnTaskCompleted(
      scoped_ptr<ScopedResource> resource,
      unsigned id,
      const PicturePileImpl::Analysis& analysis,
      bool was_canceled,
      TestRasterWorkerPoolTaskImpl::RasterThread raster_thread) OVERRIDE {
    EXPECT_FALSE(was_canceled);
    EXPECT_EQ(TestRasterWorkerPoolTaskImpl::RASTER_THREAD_NONE,
              raster_thread);
    EndTest();
  }

  // Overridden from RasterWorkerPoolTest:
  virtual void BeginTest() OVERRIDE {
    TestWebGraphicsContext3D* context3d = context_provider_->TestContext3d();
    context3d->set_times_map_image_chromium_succeeds(0);
    context3d->set_times_map_buffer_chromium_succeeds(0);
    AppendTask(0u, false);
    ScheduleTasks();
  }

  virtual void AfterTest() OVERRIDE {
    ASSERT_EQ(1u, tasks_.size());
    tasks_.clear();
  }
};

PIXEL_BUFFER_AND_IMAGE_TEST_F(RasterWorkerPoolTestFailedMapResource);

class RasterWorkerPoolTestFalseThrottling : public RasterWorkerPoolTest {
 public:
  // Overridden from RasterWorkerPoolTest:
  virtual void BeginTest() OVERRIDE {
    // This test checks that replacing a pending raster task with another does
    // not prevent the DidFinishRunningTasks notification from being sent.

    // Schedule a task that is prevented from completing with a lock.
    lock_.Acquire();
    AppendBlockingTask(0u, &lock_);
    ScheduleTasks();

    // Schedule another task to replace the still-pending task. Because the old
    // task is not a throttled task in the new task set, it should not prevent
    // DidFinishRunningTasks from getting signaled.
    tasks_.clear();
    AppendTask(1u, false);
    ScheduleTasks();

    // Unblock the first task to allow the second task to complete.
    lock_.Release();
  }

  virtual void AfterTest() OVERRIDE {}

  virtual void DidFinishRunningTasks() OVERRIDE {
    EndTest();
  }

  base::Lock lock_;
};

PIXEL_BUFFER_AND_IMAGE_TEST_F(RasterWorkerPoolTestFalseThrottling);

}  // namespace

}  // namespace cc
