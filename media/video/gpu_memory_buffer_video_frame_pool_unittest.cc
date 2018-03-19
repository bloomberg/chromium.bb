// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "media/base/video_frame.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

namespace {
class TestGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
 public:
  void GenTextures(GLsizei n, GLuint* textures) override {
    DCHECK_EQ(1, n);
    *textures = ++gen_textures_count_;
  }

  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override {
    gpu::SyncToken sync_token_data;
    sync_token_data.Set(gpu::CommandBufferNamespace::GPU_IO,
                        gpu::CommandBufferId(), next_fence_sync_++);
    sync_token_data.SetVerifyFlush();
    memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
  }

  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override {
    gpu::SyncToken sync_token_data;
    sync_token_data.Set(gpu::CommandBufferNamespace::GPU_IO,
                        gpu::CommandBufferId(), next_fence_sync_++);
    memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
  }

  void GenMailboxCHROMIUM(GLbyte* mailbox) override {
    *reinterpret_cast<unsigned*>(mailbox) = ++mailbox_;
  }

  void DeleteTextures(GLsizei n, const GLuint* textures) override {
    ++deleted_textures_;
    DCHECK_LE(deleted_textures_, gen_textures_count_);
  }

  unsigned gen_textures_count() const { return gen_textures_count_; }
  unsigned deleted_textures_count() const { return deleted_textures_; }

 private:
  uint64_t next_fence_sync_ = 1u;
  unsigned mailbox_ = 0u;
  unsigned gen_textures_count_ = 0u;
  unsigned deleted_textures_ = 0u;
};

}  // unnamed namespace

class GpuMemoryBufferVideoFramePoolTest : public ::testing::Test {
 public:
  GpuMemoryBufferVideoFramePoolTest() = default;
  void SetUp() override {
    // Seed test clock with some dummy non-zero value to avoid confusion with
    // empty base::TimeTicks values.
    test_clock_.Advance(base::TimeDelta::FromSeconds(1234));

    gles2_.reset(new TestGLES2Interface);
    media_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    copy_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    media_task_runner_handle_.reset(
        new base::ThreadTaskRunnerHandle(media_task_runner_));
    mock_gpu_factories_.reset(
        new MockGpuVideoAcceleratorFactories(gles2_.get()));
    gpu_memory_buffer_pool_.reset(new GpuMemoryBufferVideoFramePool(
        media_task_runner_, copy_task_runner_.get(),
        mock_gpu_factories_.get()));
    gpu_memory_buffer_pool_->SetTickClockForTesting(&test_clock_);
  }

  void TearDown() override {
    gpu_memory_buffer_pool_.reset();
    RunUntilIdle();
    mock_gpu_factories_.reset();
  }

  void RunUntilIdle() {
    media_task_runner_->RunUntilIdle();
    copy_task_runner_->RunUntilIdle();
    media_task_runner_->RunUntilIdle();
  }

  static scoped_refptr<VideoFrame> CreateTestYUVVideoFrame(
      int dimension,
      size_t bit_depth = 8,
      int visible_rect_crop = 0) {
    const int kDimension = 10;
    // Data buffers are overdimensioned to acommodate up to 16bpc samples.
    static uint8_t y_data[2 * kDimension * kDimension] = {0};
    static uint8_t u_data[2 * kDimension * kDimension / 2] = {0};
    static uint8_t v_data[2 * kDimension * kDimension / 2] = {0};

    const VideoPixelFormat format =
        (bit_depth > 8) ? PIXEL_FORMAT_YUV420P10 : PIXEL_FORMAT_I420;
    DCHECK_LE(dimension, kDimension);
    const gfx::Size size(dimension, dimension);

    scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalYuvData(
        format,  // format
        size,    // coded_size
        gfx::Rect(visible_rect_crop, visible_rect_crop,
                  size.width() - visible_rect_crop,
                  size.height() - visible_rect_crop),  // visible_rect
        size,                                          // natural_size
        size.width(),                                  // y_stride
        size.width() / 2,                              // u_stride
        size.width() / 2,                              // v_stride
        y_data,                                        // y_data
        u_data,                                        // u_data
        v_data,                                        // v_data
        base::TimeDelta());                            // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

 protected:
  base::SimpleTestTickClock test_clock_;
  std::unique_ptr<MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  std::unique_ptr<GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool_;
  scoped_refptr<base::TestSimpleTaskRunner> media_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> copy_task_runner_;
  // GpuMemoryBufferVideoFramePool uses BindToCurrentLoop(), which requires
  // ThreadTaskRunnerHandle initialization.
  std::unique_ptr<base::ThreadTaskRunnerHandle> media_task_runner_handle_;
  std::unique_ptr<TestGLES2Interface> gles2_;
};

void MaybeCreateHardwareFrameCallback(
    scoped_refptr<VideoFrame>* video_frame_output,
    const scoped_refptr<VideoFrame>& video_frame) {
  *video_frame_output = video_frame;
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, VideoFrameOutputFormatUnknown) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::UNDEFINED);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_EQ(software_frame.get(), frame.get());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareFrame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
  EXPECT_EQ(3u, frame->NumTextures());
  EXPECT_EQ(3u, gles2_->gen_textures_count());
}

// Tests the current workaround for odd positioned video frame input. Once
// https://crbug.com/638906 is fixed, output should be different.
TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareFrameWithOddOrigin) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(9, 8, 1);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_EQ(software_frame.get(), frame.get());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOne10BppHardwareFrame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10, 10);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_I420, frame->format());
  EXPECT_EQ(3u, frame->NumTextures());
  EXPECT_EQ(3u, gles2_->gen_textures_count());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, ReuseFirstResource) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  gpu::Mailbox mailbox = frame->mailbox_holder(0).mailbox;
  const gpu::SyncToken sync_token = frame->mailbox_holder(0).sync_token;
  EXPECT_EQ(3u, gles2_->gen_textures_count());

  scoped_refptr<VideoFrame> frame2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame2));
  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame2.get());
  EXPECT_NE(mailbox, frame2->mailbox_holder(0).mailbox);
  EXPECT_EQ(6u, gles2_->gen_textures_count());

  frame = nullptr;
  frame2 = nullptr;
  RunUntilIdle();

  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(6u, gles2_->gen_textures_count());
  EXPECT_EQ(frame->mailbox_holder(0).mailbox, mailbox);
  EXPECT_NE(frame->mailbox_holder(0).sync_token, sync_token);
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, DropResourceWhenSizeIsDifferent) {
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      CreateTestYUVVideoFrame(10),
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_EQ(3u, gles2_->gen_textures_count());

  frame = nullptr;
  RunUntilIdle();
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      CreateTestYUVVideoFrame(4),
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();
  EXPECT_EQ(6u, gles2_->gen_textures_count());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareUYUVFrame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::UYVY);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_UYVY, frame->format());
  EXPECT_EQ(1u, frame->NumTextures());
  EXPECT_EQ(1u, gles2_->gen_textures_count());
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareNV12Frame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::NV12_SINGLE_GMB);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_NV12, frame->format());
  EXPECT_EQ(1u, frame->NumTextures());
  EXPECT_EQ(1u, gles2_->gen_textures_count());
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareNV12Frame2) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::NV12_DUAL_GMB);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_NV12, frame->format());
  EXPECT_EQ(2u, frame->NumTextures());
  EXPECT_EQ(2u, gles2_->gen_textures_count());
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareXR30Frame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10, 10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::XR30);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_ARGB, frame->format());
  EXPECT_EQ(1u, frame->NumTextures());
  EXPECT_EQ(1u, gles2_->gen_textures_count());
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareXB30Frame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10, 10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::XB30);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(PIXEL_FORMAT_RGB32, frame->format());
  EXPECT_EQ(1u, frame->NumTextures());
  EXPECT_EQ(1u, gles2_->gen_textures_count());
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));
}

// CreateGpuMemoryBuffer can return null (e.g: when the GPU process is down).
// This test checks that in that case we don't crash and still create the
// textures.
TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateGpuMemoryBufferFail) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetFailToAllocateGpuMemoryBufferForTesting(true);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::BindOnce(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(3u, gles2_->gen_textures_count());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, ShutdownReleasesUnusedResources) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_1;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_1));

  RunUntilIdle();
  EXPECT_NE(software_frame.get(), frame_1.get());

  scoped_refptr<VideoFrame> frame_2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_2));
  RunUntilIdle();
  EXPECT_NE(software_frame.get(), frame_2.get());
  EXPECT_NE(frame_1.get(), frame_2.get());

  EXPECT_EQ(6u, gles2_->gen_textures_count());
  EXPECT_EQ(0u, gles2_->deleted_textures_count());

  // Drop frame and verify that resources are still available for reuse.
  frame_1 = nullptr;
  RunUntilIdle();
  EXPECT_EQ(6u, gles2_->gen_textures_count());
  EXPECT_EQ(0u, gles2_->deleted_textures_count());

  // While still holding onto the second frame, destruct the frame pool and
  // verify that the inner pool releases the resources for the first frame.
  gpu_memory_buffer_pool_.reset();
  RunUntilIdle();

  EXPECT_EQ(6u, gles2_->gen_textures_count());
  EXPECT_EQ(3u, gles2_->deleted_textures_count());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, StaleFramesAreExpired) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_1;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_1));

  RunUntilIdle();
  EXPECT_NE(software_frame.get(), frame_1.get());

  scoped_refptr<VideoFrame> frame_2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_2));
  RunUntilIdle();
  EXPECT_NE(software_frame.get(), frame_2.get());
  EXPECT_NE(frame_1.get(), frame_2.get());

  EXPECT_EQ(6u, gles2_->gen_textures_count());
  EXPECT_EQ(0u, gles2_->deleted_textures_count());

  // Drop frame and verify that resources are still available for reuse.
  frame_1 = nullptr;
  RunUntilIdle();
  EXPECT_EQ(6u, gles2_->gen_textures_count());
  EXPECT_EQ(0u, gles2_->deleted_textures_count());

  // Advance clock far enough to hit stale timer; ensure only frame_1 has its
  // resources released.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  frame_2 = nullptr;
  RunUntilIdle();
  EXPECT_EQ(6u, gles2_->gen_textures_count());
  EXPECT_EQ(3u, gles2_->deleted_textures_count());
}

// Test when we request two copies in a row, there should be at most one frame
// copy in flight at any time.
TEST_F(GpuMemoryBufferVideoFramePoolTest, AtMostOneCopyInFlight) {
  mock_gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::UYVY);

  scoped_refptr<VideoFrame> software_frame_1 = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_1;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_1,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_1));

  scoped_refptr<VideoFrame> software_frame_2 = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_2,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_2));

  media_task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, copy_task_runner_->NumPendingTasks());
  copy_task_runner_->RunUntilIdle();
  media_task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, copy_task_runner_->NumPendingTasks());
  RunUntilIdle();
}

// Test that Abort() stops any pending copies.
TEST_F(GpuMemoryBufferVideoFramePoolTest, AbortCopies) {
  scoped_refptr<VideoFrame> software_frame_1 = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_1;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_1,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_1));

  scoped_refptr<VideoFrame> software_frame_2 = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame_2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame_2,
      base::BindOnce(MaybeCreateHardwareFrameCallback, &frame_2));

  media_task_runner_->RunUntilIdle();
  EXPECT_GE(1u, copy_task_runner_->NumPendingTasks());
  copy_task_runner_->RunUntilIdle();

  gpu_memory_buffer_pool_->Abort();
  media_task_runner_->RunUntilIdle();
  EXPECT_EQ(0u, copy_task_runner_->NumPendingTasks());
  RunUntilIdle();
  ASSERT_FALSE(frame_2);
}

}  // namespace media
