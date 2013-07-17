// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/raster_worker_pool.h"

#include <vector>

#include "cc/resources/image_raster_worker_pool.h"
#include "cc/resources/picture_pile.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/pixel_buffer_raster_worker_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"
#include "cc/test/fake_output_surface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class TestRasterWorkerPoolTaskImpl : public internal::RasterWorkerPoolTask {
 public:
  typedef base::Callback<void(const PicturePileImpl::Analysis& analysis,
                              bool was_canceled,
                              bool did_raster)> Reply;

  TestRasterWorkerPoolTaskImpl(
      const Resource* resource,
      const Reply& reply,
      TaskVector* dependencies)
      : internal::RasterWorkerPoolTask(resource, dependencies),
        reply_(reply),
        did_raster_(false) {}

  // Overridden from internal::WorkerPoolTask:
  virtual bool RunOnWorkerThread(SkDevice* device, unsigned thread_index)
      OVERRIDE {
    did_raster_ = true;
    return true;
  }
  virtual void CompleteOnOriginThread() OVERRIDE {
    reply_.Run(PicturePileImpl::Analysis(), !HasFinishedRunning(), did_raster_);
  }

 protected:
  virtual ~TestRasterWorkerPoolTaskImpl() {}

 private:
  const Reply reply_;
  bool did_raster_;

  DISALLOW_COPY_AND_ASSIGN(TestRasterWorkerPoolTaskImpl);
};

class RasterWorkerPoolTest : public testing::Test,
                             public RasterWorkerPoolClient  {
 public:
  RasterWorkerPoolTest()
      : output_surface_(FakeOutputSurface::Create3d()),
        resource_provider_(
            ResourceProvider::Create(output_surface_.get(), 0)),
        check_interval_milliseconds_(1),
        timeout_seconds_(5),
        timed_out_(false) {
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
          resource_provider(), 1);
    } else {
      raster_worker_pool_ = PixelBufferRasterWorkerPool::Create(
          resource_provider(), 1);
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

  void AppendTask(unsigned id) {
    const gfx::Size size(1, 1);

    scoped_ptr<ScopedResource> resource(
        ScopedResource::create(resource_provider()));
    resource->Allocate(size, GL_RGBA, ResourceProvider::TextureUsageAny);
    const Resource* const_resource = resource.get();

    RasterWorkerPool::Task::Set empty;
    tasks_.push_back(
        RasterWorkerPool::RasterTask(new TestRasterWorkerPoolTaskImpl(
            const_resource,
            base::Bind(&RasterWorkerPoolTest::OnTaskCompleted,
                       base::Unretained(this),
                       base::Passed(&resource),
                       id),
            &empty.tasks_)));
  }

  virtual void OnTaskCompleted(scoped_ptr<ScopedResource> resource,
                               unsigned id,
                               const PicturePileImpl::Analysis& analysis,
                               bool was_canceled,
                               bool did_raster) {}

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

class BasicRasterWorkerPoolTest : public RasterWorkerPoolTest {
 public:
  virtual void OnTaskCompleted(scoped_ptr<ScopedResource> resource,
                               unsigned id,
                               const PicturePileImpl::Analysis& analysis,
                               bool was_canceled,
                               bool did_raster) OVERRIDE {
    EXPECT_TRUE(did_raster);
    on_task_completed_ids_.push_back(id);
    if (on_task_completed_ids_.size() == 2)
      EndTest();
  }

  // Overridden from RasterWorkerPoolTest:
  virtual void BeginTest() OVERRIDE {
    AppendTask(0u);
    AppendTask(1u);
    ScheduleTasks();
  }
  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(2u, on_task_completed_ids_.size());
    tasks_.clear();
  }

  std::vector<unsigned> on_task_completed_ids_;
};

PIXEL_BUFFER_AND_IMAGE_TEST_F(BasicRasterWorkerPoolTest);

class RasterWorkerPoolTestFailedMapResource : public RasterWorkerPoolTest {
 public:
  virtual void OnTaskCompleted(scoped_ptr<ScopedResource> resource,
                               unsigned id,
                               const PicturePileImpl::Analysis& analysis,
                               bool was_canceled,
                               bool did_raster) OVERRIDE {
    EXPECT_FALSE(did_raster);
    EndTest();
  }

  // Overridden from RasterWorkerPoolTest:
  virtual void BeginTest() OVERRIDE {
    TestWebGraphicsContext3D* context3d =
        static_cast<TestWebGraphicsContext3D*>(output_surface_->context3d());
    context3d->set_times_map_image_chromium_succeeds(0);
    context3d->set_times_map_buffer_chromium_succeeds(0);
    AppendTask(0u);
    ScheduleTasks();
  }

  virtual void AfterTest() OVERRIDE {
    ASSERT_EQ(1u, tasks_.size());
    tasks_.clear();
  }
};

PIXEL_BUFFER_AND_IMAGE_TEST_F(RasterWorkerPoolTestFailedMapResource);

}  // namespace

}  // namespace cc
