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

class TestRasterTaskImpl : public internal::RasterWorkerPoolTask {
 public:
  TestRasterTaskImpl(const Resource* resource,
                     const RasterWorkerPool::RasterTask::Reply& reply,
                     internal::WorkerPoolTask::TaskVector* dependencies)
      : internal::RasterWorkerPoolTask(resource, dependencies),
        reply_(reply) {}

  virtual bool RunOnThread(SkDevice* device, unsigned thread_index) OVERRIDE {
    return true;
  }
  virtual void DispatchCompletionCallback() OVERRIDE {
    reply_.Run(!HasFinishedRunning());
  }

 protected:
  virtual ~TestRasterTaskImpl() {}

 private:
  const RasterWorkerPool::RasterTask::Reply reply_;
};

class RasterWorkerPoolTest : public testing::Test {
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

  virtual void BeginTest() = 0;
  virtual void AfterTest() = 0;

  ResourceProvider* resource_provider() const {
    return resource_provider_.get();
  }

  RasterWorkerPool* worker_pool() {
    return raster_worker_pool_.get();
  }

  RasterWorkerPool::RasterTask CreateRasterTask(
      const Resource* resource,
      const RasterWorkerPool::RasterTask::Reply& reply,
      RasterWorkerPool::Task::Set& dependencies) {
    return RasterWorkerPool::RasterTask(
        new TestRasterTaskImpl(resource, reply, &dependencies.tasks_));
  }

  void RunTest(bool use_map_image) {
    if (use_map_image) {
      raster_worker_pool_ = ImageRasterWorkerPool::Create(
          resource_provider(), 1);
    } else {
      raster_worker_pool_ = PixelBufferRasterWorkerPool::Create(
          resource_provider(), 1);
    }

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

  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<RasterWorkerPool> raster_worker_pool_;
  base::CancelableClosure check_;
  int check_interval_milliseconds_;
  base::CancelableClosure timeout_;
  int timeout_seconds_;
  bool timed_out_;
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
  bool RunRasterTask(SkDevice* device, PicturePileImpl* picture_pile) {
    return true;
  }

  void OnTaskCompleted(scoped_ptr<ScopedResource> resource,
                       unsigned id,
                       bool was_canceled) {
    on_task_completed_ids_.push_back(id);
    if (on_task_completed_ids_.size() == 2)
      EndTest();
  }

  void AppendTask(unsigned id) {
    const gfx::Size size(1, 1);

    scoped_refptr<PicturePile> picture_pile(new PicturePile);
    picture_pile->set_num_raster_threads(1);
    scoped_refptr<PicturePileImpl> picture_pile_impl(
        PicturePileImpl::CreateFromOther(picture_pile.get(), false));

    scoped_ptr<ScopedResource> resource(
        ScopedResource::create(resource_provider()));
    resource->Allocate(size, GL_RGBA, ResourceProvider::TextureUsageAny);
    const Resource* const_resource = resource.get();

    RasterWorkerPool::Task::Set empty;
    tasks_.push_back(CreateRasterTask(
        const_resource,
        base::Bind(&BasicRasterWorkerPoolTest::OnTaskCompleted,
                   base::Unretained(this),
                   base::Passed(&resource),
                   id),
        empty));
  }

  void ScheduleTasks() {
    RasterWorkerPool::RasterTask::Queue tasks;

    for (std::vector<RasterWorkerPool::RasterTask>::iterator it =
             tasks_.begin();
         it != tasks_.end(); ++it)
      tasks.Append(*it);

    worker_pool()->ScheduleTasks(&tasks);
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

  std::vector<RasterWorkerPool::RasterTask> tasks_;
  std::vector<unsigned> on_task_completed_ids_;
};

PIXEL_BUFFER_AND_IMAGE_TEST_F(BasicRasterWorkerPoolTest);

}  // namespace

}  // namespace cc
