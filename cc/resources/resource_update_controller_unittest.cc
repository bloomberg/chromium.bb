// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_update_controller.h"

#include "cc/resources/prioritized_resource_manager.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/scheduler_test_common.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/test/tiled_layer_test_common.h"
#include "cc/trees/single_thread_proxy.h"  // For DebugScopedSetImplThread
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using testing::Test;
using WebKit::WGC3Denum;
using WebKit::WGC3Dint;
using WebKit::WGC3Duint;
using WebKit::WGC3Dsizei;
using WebKit::WebGLId;
using WebKit::WebString;

namespace cc {
namespace {

const int kFlushPeriodFull = 4;
const int kFlushPeriodPartial = kFlushPeriodFull;

class ResourceUpdateControllerTest;

class WebGraphicsContext3DForUploadTest : public TestWebGraphicsContext3D {
 public:
  explicit WebGraphicsContext3DForUploadTest(ResourceUpdateControllerTest* test)
      : test_(test),
        support_shallow_flush_(true) {}

  virtual void flush(void) OVERRIDE;
  virtual void shallowFlushCHROMIUM(void) OVERRIDE;
  virtual void texSubImage2D(
      WGC3Denum target,
      WGC3Dint level,
      WGC3Dint xoffset,
      WGC3Dint yoffset,
      WGC3Dsizei width,
      WGC3Dsizei height,
      WGC3Denum format,
      WGC3Denum type,
      const void* pixels) OVERRIDE;
  virtual GrGLInterface* onCreateGrGLInterface() OVERRIDE { return NULL; }

  virtual WebString getString(WGC3Denum name) OVERRIDE {
    if (support_shallow_flush_)
      return WebString("GL_CHROMIUM_shallow_flush");
    return WebString("");
  }

  virtual void getQueryObjectuivEXT(
      WebGLId id,
      WGC3Denum pname,
      WGC3Duint* value);

 private:
  ResourceUpdateControllerTest* test_;
  bool support_shallow_flush_;
};

class ResourceUpdateControllerTest : public Test {
 public:
  ResourceUpdateControllerTest()
      : proxy_(scoped_ptr<Thread>(NULL)),
        queue_(make_scoped_ptr(new ResourceUpdateQueue)),
        resource_manager_(PrioritizedResourceManager::Create(&proxy_)),
        query_results_available_(0),
        full_upload_count_expected_(0),
        partial_count_expected_(0),
        total_upload_count_expected_(0),
        max_upload_count_per_update_(0),
        num_consecutive_flushes_(0),
        num_dangling_uploads_(0),
        num_total_uploads_(0),
        num_total_flushes_(0) {}

  virtual ~ResourceUpdateControllerTest() {
    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager_->ClearAllMemory(resource_provider_.get());
  }

 public:
  void OnFlush() {
    // Check for back-to-back flushes.
    EXPECT_EQ(0, num_consecutive_flushes_) << "Back-to-back flushes detected.";

    num_dangling_uploads_ = 0;
    num_consecutive_flushes_++;
    num_total_flushes_++;
  }

  void OnUpload() {
    // Check for too many consecutive uploads
    if (num_total_uploads_ < full_upload_count_expected_) {
      EXPECT_LT(num_dangling_uploads_, kFlushPeriodFull)
          << "Too many consecutive full uploads detected.";
    } else {
      EXPECT_LT(num_dangling_uploads_, kFlushPeriodPartial)
          << "Too many consecutive partial uploads detected.";
    }

    num_consecutive_flushes_ = 0;
    num_dangling_uploads_++;
    num_total_uploads_++;
  }

  bool IsQueryResultAvailable() {
    if (!query_results_available_)
      return false;

    query_results_available_--;
    return true;
  }

 protected:
  virtual void SetUp() {
    output_surface_ =
        FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
            new WebGraphicsContext3DForUploadTest(this)));
    bitmap_.setConfig(SkBitmap::kARGB_8888_Config, 300, 150);
    bitmap_.allocPixels();

    for (int i = 0; i < 4; i++) {
      textures_[i] = PrioritizedResource::Create(resource_manager_.get(),
                                                 gfx::Size(300, 150), GL_RGBA);
      textures_[i]->
          set_request_priority(PriorityCalculator::VisiblePriority(true));
    }
    resource_manager_->PrioritizeTextures();

    resource_provider_ = ResourceProvider::Create(output_surface_.get());
  }

  void AppendFullUploadsOfIndexedTextureToUpdateQueue(int count,
                                                      int texture_index) {
    full_upload_count_expected_ += count;
    total_upload_count_expected_ += count;

    const gfx::Rect rect(0, 0, 300, 150);
    const ResourceUpdate upload = ResourceUpdate::Create(
        textures_[texture_index].get(), &bitmap_, rect, rect, gfx::Vector2d());
    for (int i = 0; i < count; i++)
      queue_->AppendFullUpload(upload);
  }

  void AppendFullUploadsToUpdateQueue(int count) {
    AppendFullUploadsOfIndexedTextureToUpdateQueue(count, 0);
  }

  void AppendPartialUploadsOfIndexedTextureToUpdateQueue(int count,
                                                         int texture_index) {
    partial_count_expected_ += count;
    total_upload_count_expected_ += count;

    const gfx::Rect rect(0, 0, 100, 100);
    const ResourceUpdate upload = ResourceUpdate::Create(
        textures_[texture_index].get(), &bitmap_, rect, rect, gfx::Vector2d());
    for (int i = 0; i < count; i++)
      queue_->AppendPartialUpload(upload);
  }

  void AppendPartialUploadsToUpdateQueue(int count) {
    AppendPartialUploadsOfIndexedTextureToUpdateQueue(count, 0);
  }

  void SetMaxUploadCountPerUpdate(int count) {
    max_upload_count_per_update_ = count;
  }

  void UpdateTextures() {
    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(&proxy_);
    scoped_ptr<ResourceUpdateController> update_controller =
        ResourceUpdateController::Create(
            NULL, proxy_.ImplThread(), queue_.Pass(), resource_provider_.get());
    update_controller->Finalize();
  }

  void MakeQueryResultAvailable() { query_results_available_++; }

 protected:
  // Classes required to interact and test the ResourceUpdateController
  FakeProxy proxy_;
  scoped_ptr<OutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<ResourceUpdateQueue> queue_;
  scoped_ptr<PrioritizedResource> textures_[4];
  scoped_ptr<PrioritizedResourceManager> resource_manager_;
  SkBitmap bitmap_;
  int query_results_available_;

  // Properties / expectations of this test
  int full_upload_count_expected_;
  int partial_count_expected_;
  int total_upload_count_expected_;
  int max_upload_count_per_update_;

  // Dynamic properties of this test
  int num_consecutive_flushes_;
  int num_dangling_uploads_;
  int num_total_uploads_;
  int num_total_flushes_;
};

void WebGraphicsContext3DForUploadTest::flush(void) { test_->OnFlush(); }

void WebGraphicsContext3DForUploadTest::shallowFlushCHROMIUM(void) {
  test_->OnFlush();
}

void WebGraphicsContext3DForUploadTest::texSubImage2D(
    WGC3Denum target,
    WGC3Dint level,
    WGC3Dint xoffset,
    WGC3Dint yoffset,
    WGC3Dsizei width,
    WGC3Dsizei height,
    WGC3Denum format,
    WGC3Denum type,
    const void* pixels) {
  test_->OnUpload();
}

void WebGraphicsContext3DForUploadTest::getQueryObjectuivEXT(
    WebGLId id,
    WGC3Denum pname,
    WGC3Duint* params) {
  if (pname == GL_QUERY_RESULT_AVAILABLE_EXT)
    *params = test_->IsQueryResultAvailable();
}

// ZERO UPLOADS TESTS
TEST_F(ResourceUpdateControllerTest, ZeroUploads) {
  AppendFullUploadsToUpdateQueue(0);
  AppendPartialUploadsToUpdateQueue(0);
  UpdateTextures();

  EXPECT_EQ(0, num_total_flushes_);
  EXPECT_EQ(0, num_total_uploads_);
}

// ONE UPLOAD TESTS
TEST_F(ResourceUpdateControllerTest, OneFullUpload) {
  AppendFullUploadsToUpdateQueue(1);
  AppendPartialUploadsToUpdateQueue(0);
  UpdateTextures();

  EXPECT_EQ(1, num_total_flushes_);
  EXPECT_EQ(1, num_total_uploads_);
  EXPECT_EQ(0, num_dangling_uploads_)
      << "Last upload wasn't followed by a flush.";
}

TEST_F(ResourceUpdateControllerTest, OnePartialUpload) {
  AppendFullUploadsToUpdateQueue(0);
  AppendPartialUploadsToUpdateQueue(1);
  UpdateTextures();

  EXPECT_EQ(1, num_total_flushes_);
  EXPECT_EQ(1, num_total_uploads_);
  EXPECT_EQ(0, num_dangling_uploads_)
      << "Last upload wasn't followed by a flush.";
}

TEST_F(ResourceUpdateControllerTest, OneFullOnePartialUpload) {
  AppendFullUploadsToUpdateQueue(1);
  AppendPartialUploadsToUpdateQueue(1);
  UpdateTextures();

  EXPECT_EQ(1, num_total_flushes_);
  EXPECT_EQ(2, num_total_uploads_);
  EXPECT_EQ(0, num_dangling_uploads_)
      << "Last upload wasn't followed by a flush.";
}

// This class of tests upload a number of textures that is a multiple
// of the flush period.
const int full_upload_flush_multipler = 7;
const int full_count = full_upload_flush_multipler * kFlushPeriodFull;

const int partial_upload_flush_multipler = 11;
const int partial_count =
    partial_upload_flush_multipler * kFlushPeriodPartial;

TEST_F(ResourceUpdateControllerTest, ManyFullUploads) {
  AppendFullUploadsToUpdateQueue(full_count);
  AppendPartialUploadsToUpdateQueue(0);
  UpdateTextures();

  EXPECT_EQ(full_upload_flush_multipler, num_total_flushes_);
  EXPECT_EQ(full_count, num_total_uploads_);
  EXPECT_EQ(0, num_dangling_uploads_)
      << "Last upload wasn't followed by a flush.";
}

TEST_F(ResourceUpdateControllerTest, ManyPartialUploads) {
  AppendFullUploadsToUpdateQueue(0);
  AppendPartialUploadsToUpdateQueue(partial_count);
  UpdateTextures();

  EXPECT_EQ(partial_upload_flush_multipler, num_total_flushes_);
  EXPECT_EQ(partial_count, num_total_uploads_);
  EXPECT_EQ(0, num_dangling_uploads_)
      << "Last upload wasn't followed by a flush.";
}

TEST_F(ResourceUpdateControllerTest, ManyFullManyPartialUploads) {
  AppendFullUploadsToUpdateQueue(full_count);
  AppendPartialUploadsToUpdateQueue(partial_count);
  UpdateTextures();

  EXPECT_EQ(full_upload_flush_multipler + partial_upload_flush_multipler,
            num_total_flushes_);
  EXPECT_EQ(full_count + partial_count, num_total_uploads_);
  EXPECT_EQ(0, num_dangling_uploads_)
      << "Last upload wasn't followed by a flush.";
}

class FakeResourceUpdateControllerClient
    : public cc::ResourceUpdateControllerClient {
 public:
  FakeResourceUpdateControllerClient() { Reset(); }
  void Reset() { ready_to_finalize_called_ = false; }
  bool ReadyToFinalizeCalled() const { return ready_to_finalize_called_; }

  virtual void ReadyToFinalizeTextureUpdates() OVERRIDE {
    ready_to_finalize_called_ = true;
  }

 protected:
  bool ready_to_finalize_called_;
};

class FakeResourceUpdateController : public cc::ResourceUpdateController {
 public:
  static scoped_ptr<FakeResourceUpdateController> Create(
      cc::ResourceUpdateControllerClient* client, cc::Thread* thread,
      scoped_ptr<ResourceUpdateQueue> queue,
      ResourceProvider* resource_provider) {
    return make_scoped_ptr(new FakeResourceUpdateController(
        client, thread, queue.Pass(), resource_provider));
  }

  void SetNow(base::TimeTicks time) { now_ = time; }
  virtual base::TimeTicks Now() const OVERRIDE { return now_; }
  void SetUpdateMoreTexturesTime(base::TimeDelta time) {
    update_more_textures_time_ = time;
  }
  virtual base::TimeDelta UpdateMoreTexturesTime() const OVERRIDE {
    return update_more_textures_time_;
  }
  void SetUpdateMoreTexturesSize(size_t size) {
    update_more_textures_size_ = size;
  }
  virtual size_t UpdateMoreTexturesSize() const OVERRIDE {
    return update_more_textures_size_;
  }

 protected:
  FakeResourceUpdateController(cc::ResourceUpdateControllerClient* client,
                               cc::Thread* thread,
                               scoped_ptr<ResourceUpdateQueue> queue,
                               ResourceProvider* resource_provider)
      : cc::ResourceUpdateController(
            client, thread, queue.Pass(), resource_provider),
        update_more_textures_size_(0) {}

  base::TimeTicks now_;
  base::TimeDelta update_more_textures_time_;
  size_t update_more_textures_size_;
};

static void RunPendingTask(FakeThread* thread,
                           FakeResourceUpdateController* controller) {
  EXPECT_TRUE(thread->HasPendingTask());
  controller->SetNow(controller->Now() + base::TimeDelta::FromMilliseconds(
                                             thread->PendingDelayMs()));
  thread->RunPendingTask();
}

TEST_F(ResourceUpdateControllerTest, UpdateMoreTextures) {
  FakeResourceUpdateControllerClient client;
  FakeThread thread;

  SetMaxUploadCountPerUpdate(1);
  AppendFullUploadsToUpdateQueue(3);
  AppendPartialUploadsToUpdateQueue(0);

  DebugScopedSetImplThreadAndMainThreadBlocked
  impl_thread_and_main_thread_blocked(&proxy_);
  scoped_ptr<FakeResourceUpdateController> controller(
      FakeResourceUpdateController::Create(&client, &thread, queue_.Pass(),
                                           resource_provider_.get()));

  controller->SetNow(controller->Now() + base::TimeDelta::FromMilliseconds(1));
  controller->SetUpdateMoreTexturesTime(base::TimeDelta::FromMilliseconds(100));
  controller->SetUpdateMoreTexturesSize(1);
  // Not enough time for any updates.
  controller->PerformMoreUpdates(controller->Now() +
                                 base::TimeDelta::FromMilliseconds(90));
  EXPECT_FALSE(thread.HasPendingTask());

  controller->SetUpdateMoreTexturesTime(base::TimeDelta::FromMilliseconds(100));
  controller->SetUpdateMoreTexturesSize(1);
  // Only enough time for 1 update.
  controller->PerformMoreUpdates(controller->Now() +
                                 base::TimeDelta::FromMilliseconds(120));
  EXPECT_FALSE(thread.HasPendingTask());
  EXPECT_EQ(1, num_total_uploads_);

  // Complete one upload.
  MakeQueryResultAvailable();

  controller->SetUpdateMoreTexturesTime(base::TimeDelta::FromMilliseconds(100));
  controller->SetUpdateMoreTexturesSize(1);
  // Enough time for 2 updates.
  controller->PerformMoreUpdates(controller->Now() +
                                 base::TimeDelta::FromMilliseconds(220));
  RunPendingTask(&thread, controller.get());
  EXPECT_FALSE(thread.HasPendingTask());
  EXPECT_TRUE(client.ReadyToFinalizeCalled());
  EXPECT_EQ(3, num_total_uploads_);
}

TEST_F(ResourceUpdateControllerTest, NoMoreUpdates) {
  FakeResourceUpdateControllerClient client;
  FakeThread thread;

  SetMaxUploadCountPerUpdate(1);
  AppendFullUploadsToUpdateQueue(2);
  AppendPartialUploadsToUpdateQueue(0);

  DebugScopedSetImplThreadAndMainThreadBlocked
  impl_thread_and_main_thread_blocked(&proxy_);
  scoped_ptr<FakeResourceUpdateController> controller(
      FakeResourceUpdateController::Create(&client, &thread, queue_.Pass(),
                                           resource_provider_.get()));

  controller->SetNow(controller->Now() + base::TimeDelta::FromMilliseconds(1));
  controller->SetUpdateMoreTexturesTime(base::TimeDelta::FromMilliseconds(100));
  controller->SetUpdateMoreTexturesSize(1);
  // Enough time for 3 updates but only 2 necessary.
  controller->PerformMoreUpdates(controller->Now() +
                                 base::TimeDelta::FromMilliseconds(310));
  RunPendingTask(&thread, controller.get());
  EXPECT_FALSE(thread.HasPendingTask());
  EXPECT_TRUE(client.ReadyToFinalizeCalled());
  EXPECT_EQ(2, num_total_uploads_);

  controller->SetUpdateMoreTexturesTime(base::TimeDelta::FromMilliseconds(100));
  controller->SetUpdateMoreTexturesSize(1);
  // Enough time for updates but no more updates left.
  controller->PerformMoreUpdates(controller->Now() +
                                 base::TimeDelta::FromMilliseconds(310));
  // 0-delay task used to call ReadyToFinalizeTextureUpdates().
  RunPendingTask(&thread, controller.get());
  EXPECT_FALSE(thread.HasPendingTask());
  EXPECT_TRUE(client.ReadyToFinalizeCalled());
  EXPECT_EQ(2, num_total_uploads_);
}

TEST_F(ResourceUpdateControllerTest, UpdatesCompleteInFiniteTime) {
  FakeResourceUpdateControllerClient client;
  FakeThread thread;

  SetMaxUploadCountPerUpdate(1);
  AppendFullUploadsToUpdateQueue(2);
  AppendPartialUploadsToUpdateQueue(0);

  DebugScopedSetImplThreadAndMainThreadBlocked
  impl_thread_and_main_thread_blocked(&proxy_);
  scoped_ptr<FakeResourceUpdateController> controller(
      FakeResourceUpdateController::Create(&client, &thread, queue_.Pass(),
                                           resource_provider_.get()));

  controller->SetNow(controller->Now() + base::TimeDelta::FromMilliseconds(1));
  controller->SetUpdateMoreTexturesTime(base::TimeDelta::FromMilliseconds(500));
  controller->SetUpdateMoreTexturesSize(1);

  for (int i = 0; i < 100; i++) {
    if (client.ReadyToFinalizeCalled())
      break;

    // Not enough time for any updates.
    controller->PerformMoreUpdates(controller->Now() +
                                   base::TimeDelta::FromMilliseconds(400));

    if (thread.HasPendingTask())
      RunPendingTask(&thread, controller.get());
  }

  EXPECT_FALSE(thread.HasPendingTask());
  EXPECT_TRUE(client.ReadyToFinalizeCalled());
  EXPECT_EQ(2, num_total_uploads_);
}

}  // namespace
}  // namespace cc
