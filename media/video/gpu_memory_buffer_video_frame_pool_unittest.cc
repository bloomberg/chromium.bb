// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "media/base/video_frame.h"
#include "media/renderers/mock_gpu_video_accelerator_factories.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

namespace {
class TestGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
 public:
  unsigned gen_textures = 0u;
  void GenTextures(GLsizei n, GLuint* textures) override {
    DCHECK_EQ(1, n);
    *textures = ++gen_textures;
  }

  GLuint InsertSyncPointCHROMIUM() override { return ++sync_point; }

  void GenMailboxCHROMIUM(GLbyte* mailbox) override {
    *reinterpret_cast<unsigned*>(mailbox) = ++this->mailbox;
  }

 private:
  unsigned sync_point = 0u;
  unsigned mailbox = 0u;
};

}  // unnamed namespace

class GpuMemoryBufferVideoFramePoolTest : public ::testing::Test {
 public:
  GpuMemoryBufferVideoFramePoolTest() {}
  void SetUp() override { gles2_.reset(new TestGLES2Interface); }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  static scoped_refptr<media::VideoFrame> CreateTestYUVVideoFrame(
      int dimension) {
    const int kDimension = 10;
    static uint8 y_data[kDimension * kDimension] = {0};
    static uint8 u_data[kDimension * kDimension / 2] = {0};
    static uint8 v_data[kDimension * kDimension / 2] = {0};

    DCHECK_LE(dimension, kDimension);
    gfx::Size size(dimension, dimension);

    return media::VideoFrame::WrapExternalYuvData(
        media::VideoFrame::YV12,  // format
        size,                     // coded_size
        gfx::Rect(size),          // visible_rect
        size,                     // natural_size
        size.width(),             // y_stride
        size.width() / 2,         // u_stride
        size.width() / 2,         // v_stride
        y_data,                   // y_data
        u_data,                   // u_data
        v_data,                   // v_data
        base::TimeDelta());       // timestamp
  }

 protected:
  base::MessageLoop media_message_loop_;
  scoped_ptr<TestGLES2Interface> gles2_;
};

TEST_F(GpuMemoryBufferVideoFramePoolTest, NoGpuFactoryNoHardwareVideoFrame) {
  scoped_refptr<VideoFrame> frame = CreateTestYUVVideoFrame(10);
  scoped_ptr<GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool_ =
      make_scoped_ptr(new GpuMemoryBufferVideoFramePool(
          media_message_loop_.task_runner(), nullptr));

  scoped_refptr<VideoFrame> frame2 =
      gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(frame);
  EXPECT_EQ(frame.get(), frame2.get());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, NoTextureRGNoHardwareVideoFrame) {
  scoped_refptr<VideoFrame> frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<MockGpuVideoAcceleratorFactories> mock_gpu_factories(
      new MockGpuVideoAcceleratorFactories);
  scoped_ptr<GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool_ =
      make_scoped_ptr(new GpuMemoryBufferVideoFramePool(
          media_message_loop_.task_runner(), mock_gpu_factories));

  EXPECT_CALL(*mock_gpu_factories.get(), IsTextureRGSupported())
      .WillRepeatedly(testing::Return(false));
  scoped_refptr<VideoFrame> frame2 =
      gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(frame);
  EXPECT_EQ(frame.get(), frame2.get());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareFrame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<MockGpuVideoAcceleratorFactories> mock_gpu_factories(
      new MockGpuVideoAcceleratorFactories);
  scoped_ptr<GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool_ =
      make_scoped_ptr(new GpuMemoryBufferVideoFramePool(
          media_message_loop_.task_runner(), mock_gpu_factories));

  EXPECT_CALL(*mock_gpu_factories.get(), GetGLES2Interface())
      .WillRepeatedly(testing::Return(gles2_.get()));
  EXPECT_CALL(*mock_gpu_factories.get(), IsTextureRGSupported())
      .WillRepeatedly(testing::Return(true));

  scoped_refptr<VideoFrame> frame =
      gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(software_frame);
  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(3u, gles2_->gen_textures);
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, ReuseFirstResource) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<MockGpuVideoAcceleratorFactories> mock_gpu_factories(
      new MockGpuVideoAcceleratorFactories);
  scoped_ptr<GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool_ =
      make_scoped_ptr(new GpuMemoryBufferVideoFramePool(
          media_message_loop_.task_runner(), mock_gpu_factories));

  EXPECT_CALL(*mock_gpu_factories.get(), GetGLES2Interface())
      .WillRepeatedly(testing::Return(gles2_.get()));
  EXPECT_CALL(*mock_gpu_factories.get(), IsTextureRGSupported())
      .WillRepeatedly(testing::Return(true));

  scoped_refptr<VideoFrame> frame =
      gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(software_frame);
  EXPECT_NE(software_frame.get(), frame.get());
  gpu::Mailbox mailbox = frame->mailbox_holder(0).mailbox;
  unsigned sync_point = frame->mailbox_holder(0).sync_point;
  EXPECT_EQ(3u, gles2_->gen_textures);

  scoped_refptr<VideoFrame> frame2 =
      gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(software_frame);
  EXPECT_NE(software_frame.get(), frame2.get());
  EXPECT_NE(mailbox, frame2->mailbox_holder(0).mailbox);
  EXPECT_EQ(6u, gles2_->gen_textures);

  frame = nullptr;
  frame2 = nullptr;
  base::RunLoop().RunUntilIdle();  // Run posted closures.
  frame = gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(software_frame);
  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(6u, gles2_->gen_textures);
  EXPECT_EQ(frame->mailbox_holder(0).mailbox, mailbox);
  EXPECT_NE(frame->mailbox_holder(0).sync_point, sync_point);
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, DropResourceWhenSizeIsDifferent) {
  scoped_refptr<MockGpuVideoAcceleratorFactories> mock_gpu_factories(
      new MockGpuVideoAcceleratorFactories);
  scoped_ptr<GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool_ =
      make_scoped_ptr(new GpuMemoryBufferVideoFramePool(
          media_message_loop_.task_runner(), mock_gpu_factories));

  EXPECT_CALL(*mock_gpu_factories.get(), GetGLES2Interface())
      .WillRepeatedly(testing::Return(gles2_.get()));
  EXPECT_CALL(*mock_gpu_factories.get(), IsTextureRGSupported())
      .WillRepeatedly(testing::Return(true));

  scoped_refptr<VideoFrame> frame =
      gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
          CreateTestYUVVideoFrame(10));
  EXPECT_EQ(3u, gles2_->gen_textures);

  frame = nullptr;
  base::RunLoop().RunUntilIdle();  // Run posted closures.
  frame = gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      CreateTestYUVVideoFrame(4));
  EXPECT_EQ(6u, gles2_->gen_textures);
}

}  // namespace media
