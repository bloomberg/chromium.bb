// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/video_resource_updater.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "cc/resources/resource_provider.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
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

  void texStorage2DEXT(GLenum target,
                       GLint levels,
                       GLuint internalformat,
                       GLint width,
                       GLint height) override {
  }

  GLuint createTexture() override {
    ++created_texture_count_;
    return TestWebGraphicsContext3D::createTexture();
  }

  void deleteTexture(GLuint texture) override {
    --created_texture_count_;
    TestWebGraphicsContext3D::deleteTexture(texture);
  }

  void deleteTextures(GLsizei count, const GLuint* ids) override {
    created_texture_count_ -= count;
    TestWebGraphicsContext3D::deleteTextures(count, ids);
  }

  int UploadCount() { return upload_count_; }
  void ResetUploadCount() { upload_count_ = 0; }

  int TextureCreationCount() { return created_texture_count_; }
  void ResetTextureCreationCount() { created_texture_count_ = 0; }

 private:
  int upload_count_;
  int created_texture_count_;
};

class SharedBitmapManagerAllocationCounter : public TestSharedBitmapManager {
 public:
  std::unique_ptr<viz::SharedBitmap> AllocateSharedBitmap(
      const gfx::Size& size) override {
    ++allocation_count_;
    return TestSharedBitmapManager::AllocateSharedBitmap(size);
  }

  int AllocationCount() { return allocation_count_; }
  void ResetAllocationCount() { allocation_count_ = 0; }

 private:
  int allocation_count_;
};

class VideoResourceUpdaterTest : public testing::Test {
 protected:
  VideoResourceUpdaterTest() {
    std::unique_ptr<WebGraphicsContext3DUploadCounter> context3d(
        new WebGraphicsContext3DUploadCounter());

    context3d_ = context3d.get();
    context3d_->set_support_texture_storage(true);

    context_provider_ = TestContextProvider::Create(std::move(context3d));
    context_provider_->BindToCurrentThread();
  }

  void SetUp() override {
    testing::Test::SetUp();
    shared_bitmap_manager_.reset(new SharedBitmapManagerAllocationCounter());
    resource_provider3d_ = FakeResourceProvider::Create(
        context_provider_.get(), shared_bitmap_manager_.get(),
        high_bit_for_testing_);
    resource_provider_software_ = FakeResourceProvider::Create(
        nullptr, shared_bitmap_manager_.get(), high_bit_for_testing_);
  }

  scoped_refptr<media::VideoFrame> CreateTestYUVVideoFrame() {
    const int kDimension = 10;
    gfx::Size size(kDimension, kDimension);
    static uint8_t y_data[kDimension * kDimension] = {0};
    static uint8_t u_data[kDimension * kDimension / 2] = {0};
    static uint8_t v_data[kDimension * kDimension / 2] = {0};

    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::WrapExternalYuvData(
            media::PIXEL_FORMAT_YV16,  // format
            size,                      // coded_size
            gfx::Rect(size),           // visible_rect
            size,                      // natural_size
            size.width(),              // y_stride
            size.width() / 2,          // u_stride
            size.width() / 2,          // v_stride
            y_data,                    // y_data
            u_data,                    // u_data
            v_data,                    // v_data
            base::TimeDelta());        // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<media::VideoFrame> CreateWonkyTestYUVVideoFrame() {
    const int kDimension = 10;
    const int kYWidth = kDimension + 5;
    const int kUWidth = (kYWidth + 1) / 2 + 200;
    const int kVWidth = (kYWidth + 1) / 2 + 1;
    static uint8_t y_data[kYWidth * kDimension] = {0};
    static uint8_t u_data[kUWidth * kDimension] = {0};
    static uint8_t v_data[kVWidth * kDimension] = {0};

    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::WrapExternalYuvData(
            media::PIXEL_FORMAT_YV16,                 // format
            gfx::Size(kYWidth, kDimension),           // coded_size
            gfx::Rect(2, 0, kDimension, kDimension),  // visible_rect
            gfx::Size(kDimension, kDimension),        // natural_size
            -kYWidth,                                 // y_stride (negative)
            kUWidth,                                  // u_stride
            kVWidth,                                  // v_stride
            y_data + kYWidth * (kDimension - 1),      // y_data
            u_data,                                   // u_data
            v_data,                                   // v_data
            base::TimeDelta());                       // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<media::VideoFrame> CreateTestHighBitFrame() {
    const int kDimension = 10;
    gfx::Size size(kDimension, kDimension);

    scoped_refptr<media::VideoFrame> video_frame(media::VideoFrame::CreateFrame(
        media::PIXEL_FORMAT_YUV420P10, size, gfx::Rect(size), size,
        base::TimeDelta()));
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  void SetReleaseSyncToken(const gpu::SyncToken& sync_token) {
    release_sync_token_ = sync_token;
  }

  scoped_refptr<media::VideoFrame> CreateTestHardwareVideoFrame(
      media::VideoPixelFormat format,
      unsigned target) {
    const int kDimension = 10;
    gfx::Size size(kDimension, kDimension);

    gpu::Mailbox mailbox;
    mailbox.name[0] = 51;

    gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes] = {
        gpu::MailboxHolder(mailbox, kMailboxSyncToken, target)};
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::WrapNativeTextures(
            format, mailbox_holders,
            base::Bind(&VideoResourceUpdaterTest::SetReleaseSyncToken,
                       base::Unretained(this)),
            size,                // coded_size
            gfx::Rect(size),     // visible_rect
            size,                // natural_size
            base::TimeDelta());  // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  scoped_refptr<media::VideoFrame> CreateTestRGBAHardwareVideoFrame() {
    return CreateTestHardwareVideoFrame(media::PIXEL_FORMAT_ARGB,
                                        GL_TEXTURE_2D);
  }

  scoped_refptr<media::VideoFrame> CreateTestStreamTextureHardwareVideoFrame(
      bool needs_copy) {
    scoped_refptr<media::VideoFrame> video_frame = CreateTestHardwareVideoFrame(
        media::PIXEL_FORMAT_ARGB, GL_TEXTURE_EXTERNAL_OES);
    video_frame->metadata()->SetBoolean(
        media::VideoFrameMetadata::COPY_REQUIRED, needs_copy);
    return video_frame;
  }

  scoped_refptr<media::VideoFrame> CreateTestYuvHardwareVideoFrame(
      media::VideoPixelFormat format,
      size_t num_textures,
      unsigned target) {
    const int kDimension = 10;
    gfx::Size size(kDimension, kDimension);

    gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes];
    for (size_t i = 0; i < num_textures; ++i) {
      gpu::Mailbox mailbox;
      mailbox.name[0] = 50 + 1;
      mailbox_holders[i] =
          gpu::MailboxHolder(mailbox, kMailboxSyncToken, target);
    }
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::WrapNativeTextures(
            format, mailbox_holders,
            base::Bind(&VideoResourceUpdaterTest::SetReleaseSyncToken,
                       base::Unretained(this)),
            size,                // coded_size
            gfx::Rect(size),     // visible_rect
            size,                // natural_size
            base::TimeDelta());  // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

  static const gpu::SyncToken kMailboxSyncToken;

  WebGraphicsContext3DUploadCounter* context3d_;
  scoped_refptr<TestContextProvider> context_provider_;
  std::unique_ptr<SharedBitmapManagerAllocationCounter> shared_bitmap_manager_;
  std::unique_ptr<ResourceProvider> resource_provider3d_;
  std::unique_ptr<ResourceProvider> resource_provider_software_;
  gpu::SyncToken release_sync_token_;
  bool high_bit_for_testing_ = false;
};

const gpu::SyncToken VideoResourceUpdaterTest::kMailboxSyncToken =
    gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                   0,
                   gpu::CommandBufferId::FromUnsafeValue(0x123),
                   7);

TEST_F(VideoResourceUpdaterTest, SoftwareFrame) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               use_stream_video_draw_quad);
  scoped_refptr<media::VideoFrame> video_frame = CreateTestYUVVideoFrame();

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
}

TEST_F(VideoResourceUpdaterTest, HighBitFrameNoF16) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               use_stream_video_draw_quad);
  scoped_refptr<media::VideoFrame> video_frame = CreateTestHighBitFrame();

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
}

class VideoResourceUpdaterTestWithF16 : public VideoResourceUpdaterTest {
 public:
  VideoResourceUpdaterTestWithF16() : VideoResourceUpdaterTest() {
    context3d_->set_support_texture_half_float_linear(true);
  }
};

TEST_F(VideoResourceUpdaterTestWithF16, HighBitFrame) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               use_stream_video_draw_quad);
  scoped_refptr<media::VideoFrame> video_frame = CreateTestHighBitFrame();

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
  EXPECT_NEAR(resources.multiplier, 2.0, 0.1);
  EXPECT_NEAR(resources.offset, 0.5, 0.1);

  // Create the resource again, to test the path where the
  // resources are cached.
  VideoFrameExternalResources resources2 =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources2.type);
  EXPECT_NEAR(resources2.multiplier, 2.0, 0.1);
  EXPECT_NEAR(resources2.offset, 0.5, 0.1);
}

class VideoResourceUpdaterTestWithR16 : public VideoResourceUpdaterTest {
 public:
  VideoResourceUpdaterTestWithR16() : VideoResourceUpdaterTest() {
    high_bit_for_testing_ = true;
    context3d_->set_support_texture_norm16(true);
  }
};

TEST_F(VideoResourceUpdaterTestWithR16, HighBitFrame) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               use_stream_video_draw_quad);
  updater.SetUseR16ForTesting(true);
  scoped_refptr<media::VideoFrame> video_frame = CreateTestHighBitFrame();

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);

  // Max 10-bit values as read by a sampler.
  double max_10bit_value = ((1 << 10) - 1) / 65535.0;
  EXPECT_NEAR(resources.multiplier * max_10bit_value, 1.0, 0.0001);
  EXPECT_NEAR(resources.offset, 0.0, 0.1);

  // Create the resource again, to test the path where the
  // resources are cached.
  VideoFrameExternalResources resources2 =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources2.type);
  EXPECT_NEAR(resources2.multiplier * max_10bit_value, 1.0, 0.0001);
  EXPECT_NEAR(resources2.offset, 0.0, 0.1);
}

TEST_F(VideoResourceUpdaterTest, HighBitFrameSoftwareCompositor) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(nullptr, resource_provider_software_.get(),
                               use_stream_video_draw_quad);
  scoped_refptr<media::VideoFrame> video_frame = CreateTestHighBitFrame();

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::SOFTWARE_RESOURCE, resources.type);
}

TEST_F(VideoResourceUpdaterTest, WonkySoftwareFrame) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               use_stream_video_draw_quad);
  scoped_refptr<media::VideoFrame> video_frame = CreateWonkyTestYUVVideoFrame();

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
}

TEST_F(VideoResourceUpdaterTest, WonkySoftwareFrameSoftwareCompositor) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(nullptr, resource_provider_software_.get(),
                               use_stream_video_draw_quad);
  scoped_refptr<media::VideoFrame> video_frame = CreateWonkyTestYUVVideoFrame();

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::SOFTWARE_RESOURCE, resources.type);
}

TEST_F(VideoResourceUpdaterTest, ReuseResource) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               use_stream_video_draw_quad);
  scoped_refptr<media::VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::TimeDelta::FromSeconds(1234));

  // Allocate the resources for a YUV video frame.
  context3d_->ResetUploadCount();
  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
  EXPECT_EQ(size_t(3), resources.mailboxes.size());
  EXPECT_EQ(size_t(3), resources.release_callbacks.size());
  EXPECT_EQ(size_t(0), resources.software_resources.size());
  // Expect exactly three texture uploads, one for each plane.
  EXPECT_EQ(3, context3d_->UploadCount());

  // Simulate the ResourceProvider releasing the resources back to the video
  // updater.
  for (auto& release_callback : resources.release_callbacks)
    release_callback.Run(gpu::SyncToken(), false);

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
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               use_stream_video_draw_quad);
  scoped_refptr<media::VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::TimeDelta::FromSeconds(1234));

  // Allocate the resources for a YUV video frame.
  context3d_->ResetUploadCount();
  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
  EXPECT_EQ(size_t(3), resources.mailboxes.size());
  EXPECT_EQ(size_t(3), resources.release_callbacks.size());
  EXPECT_EQ(size_t(0), resources.software_resources.size());
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

TEST_F(VideoResourceUpdaterTest, SoftwareFrameSoftwareCompositor) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(nullptr, resource_provider_software_.get(),
                               use_stream_video_draw_quad);
  scoped_refptr<media::VideoFrame> video_frame = CreateTestYUVVideoFrame();

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::SOFTWARE_RESOURCE, resources.type);
}

TEST_F(VideoResourceUpdaterTest, ReuseResourceSoftwareCompositor) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(nullptr, resource_provider_software_.get(),
                               use_stream_video_draw_quad);
  scoped_refptr<media::VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::TimeDelta::FromSeconds(1234));

  // Allocate the resources for a software video frame.
  shared_bitmap_manager_->ResetAllocationCount();
  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::SOFTWARE_RESOURCE, resources.type);
  EXPECT_EQ(size_t(0), resources.mailboxes.size());
  EXPECT_EQ(size_t(0), resources.release_callbacks.size());
  EXPECT_EQ(size_t(1), resources.software_resources.size());
  // Expect exactly one allocated shared bitmap.
  EXPECT_EQ(1, shared_bitmap_manager_->AllocationCount());

  // Simulate the ResourceProvider releasing the resource back to the video
  // updater.
  resources.software_release_callback.Run(gpu::SyncToken(), false);

  // Allocate resources for the same frame.
  shared_bitmap_manager_->ResetAllocationCount();
  resources = updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::SOFTWARE_RESOURCE, resources.type);
  EXPECT_EQ(size_t(0), resources.mailboxes.size());
  EXPECT_EQ(size_t(0), resources.release_callbacks.size());
  EXPECT_EQ(size_t(1), resources.software_resources.size());
  // The data should be reused so expect no new allocations.
  EXPECT_EQ(0, shared_bitmap_manager_->AllocationCount());
}

TEST_F(VideoResourceUpdaterTest, ReuseResourceNoDeleteSoftwareCompositor) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(nullptr, resource_provider_software_.get(),
                               use_stream_video_draw_quad);
  scoped_refptr<media::VideoFrame> video_frame = CreateTestYUVVideoFrame();
  video_frame->set_timestamp(base::TimeDelta::FromSeconds(1234));

  // Allocate the resources for a software video frame.
  shared_bitmap_manager_->ResetAllocationCount();
  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::SOFTWARE_RESOURCE, resources.type);
  EXPECT_EQ(size_t(0), resources.mailboxes.size());
  EXPECT_EQ(size_t(0), resources.release_callbacks.size());
  EXPECT_EQ(size_t(1), resources.software_resources.size());
  // Expect exactly one allocated shared bitmap.
  EXPECT_EQ(1, shared_bitmap_manager_->AllocationCount());

  // Allocate resources for the same frame.
  shared_bitmap_manager_->ResetAllocationCount();
  resources = updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::SOFTWARE_RESOURCE, resources.type);
  EXPECT_EQ(size_t(0), resources.mailboxes.size());
  EXPECT_EQ(size_t(0), resources.release_callbacks.size());
  EXPECT_EQ(size_t(1), resources.software_resources.size());
  // The data should be reused so expect no new allocations.
  EXPECT_EQ(0, shared_bitmap_manager_->AllocationCount());
}

TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               use_stream_video_draw_quad);

  scoped_refptr<media::VideoFrame> video_frame =
      CreateTestRGBAHardwareVideoFrame();

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::RGBA_PREMULTIPLIED_RESOURCE,
            resources.type);
  EXPECT_EQ(1u, resources.mailboxes.size());
  EXPECT_EQ(1u, resources.release_callbacks.size());
  EXPECT_EQ(0u, resources.software_resources.size());

  video_frame = CreateTestYuvHardwareVideoFrame(media::PIXEL_FORMAT_I420, 3,
                                                GL_TEXTURE_RECTANGLE_ARB);

  resources = updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
  EXPECT_EQ(3u, resources.mailboxes.size());
  EXPECT_EQ(3u, resources.release_callbacks.size());
  EXPECT_EQ(0u, resources.software_resources.size());
  EXPECT_FALSE(resources.read_lock_fences_enabled);

  video_frame = CreateTestYuvHardwareVideoFrame(media::PIXEL_FORMAT_I420, 3,
                                                GL_TEXTURE_RECTANGLE_ARB);
  video_frame->metadata()->SetBoolean(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED, true);

  resources = updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_TRUE(resources.read_lock_fences_enabled);
}

TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_StreamTexture) {
  bool use_stream_video_draw_quad = true;
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               use_stream_video_draw_quad);
  context3d_->ResetTextureCreationCount();
  scoped_refptr<media::VideoFrame> video_frame =
      CreateTestStreamTextureHardwareVideoFrame(false);

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::STREAM_TEXTURE_RESOURCE,
            resources.type);
  EXPECT_EQ(1u, resources.mailboxes.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES, resources.mailboxes[0].target());
  EXPECT_EQ(1u, resources.release_callbacks.size());
  EXPECT_EQ(0u, resources.software_resources.size());
  EXPECT_EQ(0, context3d_->TextureCreationCount());

  // A copied stream texture should return an RGBA resource in a new
  // GL_TEXTURE_2D texture.
  context3d_->ResetTextureCreationCount();
  video_frame = CreateTestStreamTextureHardwareVideoFrame(true);
  resources = updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::RGBA_PREMULTIPLIED_RESOURCE,
            resources.type);
  EXPECT_EQ(1u, resources.mailboxes.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_2D, resources.mailboxes[0].target());
  EXPECT_EQ(1u, resources.release_callbacks.size());
  EXPECT_EQ(0u, resources.software_resources.size());
  EXPECT_EQ(1, context3d_->TextureCreationCount());
}

TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_TextureQuad) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               use_stream_video_draw_quad);
  context3d_->ResetTextureCreationCount();
  scoped_refptr<media::VideoFrame> video_frame =
      CreateTestStreamTextureHardwareVideoFrame(false);

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::RGBA_PREMULTIPLIED_RESOURCE,
            resources.type);
  EXPECT_EQ(1u, resources.mailboxes.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES, resources.mailboxes[0].target());
  EXPECT_EQ(1u, resources.release_callbacks.size());
  EXPECT_EQ(0u, resources.software_resources.size());
  EXPECT_EQ(0, context3d_->TextureCreationCount());
}

// Passthrough the sync token returned by the compositor if we don't have an
// existing release sync token.
TEST_F(VideoResourceUpdaterTest, PassReleaseSyncToken) {
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               false /* use_stream_video_draw_quad */);

  const gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO, 0,
                                  gpu::CommandBufferId::FromUnsafeValue(0x123),
                                  123);

  {
    scoped_refptr<media::VideoFrame> video_frame =
        CreateTestRGBAHardwareVideoFrame();

    VideoFrameExternalResources resources =
        updater.CreateExternalResourcesFromVideoFrame(video_frame);

    ASSERT_EQ(resources.release_callbacks.size(), 1u);
    resources.release_callbacks[0].Run(sync_token, false);
  }

  EXPECT_EQ(release_sync_token_, sync_token);
}

// Generate new sync token because video frame has an existing sync token.
TEST_F(VideoResourceUpdaterTest, GenerateReleaseSyncToken) {
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               false /* use_stream_video_draw_quad */);

  const gpu::SyncToken sync_token1(gpu::CommandBufferNamespace::GPU_IO, 0,
                                   gpu::CommandBufferId::FromUnsafeValue(0x123),
                                   123);

  const gpu::SyncToken sync_token2(gpu::CommandBufferNamespace::GPU_IO, 0,
                                   gpu::CommandBufferId::FromUnsafeValue(0x234),
                                   234);

  {
    scoped_refptr<media::VideoFrame> video_frame =
        CreateTestRGBAHardwareVideoFrame();

    VideoFrameExternalResources resources =
        updater.CreateExternalResourcesFromVideoFrame(video_frame);

    ASSERT_EQ(resources.release_callbacks.size(), 1u);
    resources.release_callbacks[0].Run(sync_token1, false);
    resources.release_callbacks[0].Run(sync_token2, false);
  }

  EXPECT_TRUE(release_sync_token_.HasData());
  EXPECT_NE(release_sync_token_, sync_token1);
  EXPECT_NE(release_sync_token_, sync_token2);
}

// Pass mailbox sync token as is if no GL operations are performed before frame
// resources are handed off to the compositor.
TEST_F(VideoResourceUpdaterTest, PassMailboxSyncToken) {
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               false /* use_stream_video_draw_quad */);

  scoped_refptr<media::VideoFrame> video_frame =
      CreateTestRGBAHardwareVideoFrame();

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);

  ASSERT_EQ(resources.mailboxes.size(), 1u);
  EXPECT_TRUE(resources.mailboxes[0].HasSyncToken());
  EXPECT_EQ(resources.mailboxes[0].sync_token(), kMailboxSyncToken);
}

// Generate new sync token for compositor when copying the texture.
TEST_F(VideoResourceUpdaterTest, GenerateSyncTokenOnTextureCopy) {
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               false /* use_stream_video_draw_quad */);

  scoped_refptr<media::VideoFrame> video_frame =
      CreateTestStreamTextureHardwareVideoFrame(true /* needs_copy */);

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);

  ASSERT_EQ(resources.mailboxes.size(), 1u);
  EXPECT_TRUE(resources.mailboxes[0].HasSyncToken());
  EXPECT_NE(resources.mailboxes[0].sync_token(), kMailboxSyncToken);
}

// NV12 VideoFrames backed by a single native texture can be sampled out
// by GL as RGB. To use them as HW overlays we need to know the format
// of the underlying buffer, that is YUV_420_BIPLANAR.
TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_SingleNV12) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               use_stream_video_draw_quad);
  context3d_->ResetTextureCreationCount();
  scoped_refptr<media::VideoFrame> video_frame = CreateTestHardwareVideoFrame(
      media::PIXEL_FORMAT_NV12, GL_TEXTURE_EXTERNAL_OES);

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::RGB_RESOURCE, resources.type);
  EXPECT_EQ(1u, resources.mailboxes.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES, resources.mailboxes[0].target());
  EXPECT_EQ(gfx::BufferFormat::YUV_420_BIPLANAR, resources.buffer_format);
  EXPECT_EQ(0, context3d_->TextureCreationCount());
}

TEST_F(VideoResourceUpdaterTest, CreateForHardwarePlanes_DualNV12) {
  bool use_stream_video_draw_quad = false;
  VideoResourceUpdater updater(context_provider_.get(),
                               resource_provider3d_.get(),
                               use_stream_video_draw_quad);
  context3d_->ResetTextureCreationCount();
  scoped_refptr<media::VideoFrame> video_frame =
      CreateTestYuvHardwareVideoFrame(media::PIXEL_FORMAT_NV12, 2,
                                      GL_TEXTURE_EXTERNAL_OES);

  VideoFrameExternalResources resources =
      updater.CreateExternalResourcesFromVideoFrame(video_frame);
  EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
  EXPECT_EQ(2u, resources.mailboxes.size());
  EXPECT_EQ(2u, resources.release_callbacks.size());
  EXPECT_EQ(0u, resources.software_resources.size());
  EXPECT_EQ((GLenum)GL_TEXTURE_EXTERNAL_OES, resources.mailboxes[0].target());
  // |updater| doesn't set |buffer_format| in this case.
  EXPECT_EQ(gfx::BufferFormat::RGBA_8888, resources.buffer_format);
  EXPECT_EQ(0, context3d_->TextureCreationCount());
}

}  // namespace
}  // namespace cc
