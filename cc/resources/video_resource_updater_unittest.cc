// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/video_resource_updater.h"

#include "base/memory/shared_memory.h"
#include "cc/resources/resource_provider.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/blocking_task_runner.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class WebGraphicsContext3DUploadCounter : public TestWebGraphicsContext3D {
 public:
  void texSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     const void* pixels) override {
    ++upload_count_;
  }

  int UploadCount() { return upload_count_; }
  void ResetUploadCount() { upload_count_ = 0; }

 private:
  int upload_count_;
};

class VideoResourceUpdaterTest : public testing::Test {
 protected:
  VideoResourceUpdaterTest() {
    scoped_ptr<WebGraphicsContext3DUploadCounter> context3d(
        new WebGraphicsContext3DUploadCounter());

    context3d_ = context3d.get();

    output_surface3d_ =
        FakeOutputSurface::Create3d(context3d.Pass());
    CHECK(output_surface3d_->BindToClient(&client_));
    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider3d_ =
        ResourceProvider::Create(output_surface3d_.get(),
                                 shared_bitmap_manager_.get(),
                                 NULL,
                                 NULL,
                                 0,
                                 false,
                                 1);
  }

  scoped_refptr<media::VideoFrame> CreateTestYUVVideoFrame() {
    const int kDimension = 10;
    gfx::Size size(kDimension, kDimension);
    static uint8 y_data[kDimension * kDimension] = { 0 };
    static uint8 u_data[kDimension * kDimension / 2] = { 0 };
    static uint8 v_data[kDimension * kDimension / 2] = { 0 };

    return media::VideoFrame::WrapExternalYuvData(
        media::VideoFrame::YV16,  // format
        size,                     // coded_size
        gfx::Rect(size),          // visible_rect
        size,                     // natural_size
        size.width(),             // y_stride
        size.width() / 2,         // u_stride
        size.width() / 2,         // v_stride
        y_data,                   // y_data
        u_data,                   // u_data
        v_data,                   // v_data
        base::TimeDelta(),        // timestamp,
        base::Closure());         // no_longer_needed_cb
  }

  WebGraphicsContext3DUploadCounter* context3d_;
  FakeOutputSurfaceClient client_;
  scoped_ptr<FakeOutputSurface> output_surface3d_;
  scoped_ptr<TestSharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> resource_provider3d_;
};

TEST_F(VideoResourceUpdaterTest, SoftwareFrame) {
  VideoResourceUpdater updater(output_surface3d_->context_provider(),
                               resource_provider3d_.get());
  scoped_refptr<media::VideoFrame> video_frame = CreateTestYUVVideoFrame();

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
}

TEST_F(VideoResourceUpdaterTest, ReuseResource) {
  VideoResourceUpdater updater(output_surface3d_->context_provider(),
                               resource_provider3d_.get());
  scoped_refptr<media::VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::TimeDelta::FromSeconds(1234));

  // Allocate the resources for a YUV video frame.
  context3d_->ResetUploadCount();
  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
  EXPECT_EQ(size_t(3), resources.mailboxes.size());
  EXPECT_EQ(size_t(3), resources.release_callbacks.size());
  // Expect exactly three texture uploads, one for each plane.
  EXPECT_EQ(3, context3d_->UploadCount());

  // Simulate the ResourceProvider releasing the resources back to the video
  // updater.
  for (ReleaseCallbackImpl& release_callback : resources.release_callbacks)
    release_callback.Run(0, false, nullptr);

  // Allocate resources for the same frame.
  context3d_->ResetUploadCount();
  resources = updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
  EXPECT_EQ(size_t(3), resources.mailboxes.size());
  EXPECT_EQ(size_t(3), resources.release_callbacks.size());
  // The data should be reused so expect no texture uploads.
  EXPECT_EQ(0, context3d_->UploadCount());
}

TEST_F(VideoResourceUpdaterTest, ReuseResourceNoDelete) {
  VideoResourceUpdater updater(output_surface3d_->context_provider(),
                               resource_provider3d_.get());
  scoped_refptr<media::VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::TimeDelta::FromSeconds(1234));

  // Allocate the resources for a YUV video frame.
  context3d_->ResetUploadCount();
  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
  EXPECT_EQ(size_t(3), resources.mailboxes.size());
  EXPECT_EQ(size_t(3), resources.release_callbacks.size());
  // Expect exactly three texture uploads, one for each plane.
  EXPECT_EQ(3, context3d_->UploadCount());

  // Allocate resources for the same frame.
  context3d_->ResetUploadCount();
  resources = updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
  EXPECT_EQ(size_t(3), resources.mailboxes.size());
  EXPECT_EQ(size_t(3), resources.release_callbacks.size());
  // The data should be reused so expect no texture uploads.
  EXPECT_EQ(0, context3d_->UploadCount());
}

}  // namespace
}  // namespace cc
