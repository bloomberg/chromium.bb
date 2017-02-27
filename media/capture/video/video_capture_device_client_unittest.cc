// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device_client.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "media/base/limits.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_jpeg_decoder.h"
#include "media/capture/video/video_frame_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::SaveArg;

namespace media {

namespace {

class MockVideoCaptureController : public VideoFrameReceiver {
 public:
  MOCK_METHOD1(MockOnNewBufferHandle, void(int buffer_id));
  MOCK_METHOD3(
      MockOnFrameReadyInBuffer,
      void(int buffer_id,
           std::unique_ptr<media::VideoCaptureDevice::Client::Buffer::
                               ScopedAccessPermission>* buffer_read_permission,
           const gfx::Size&));
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnLog, void(const std::string& message));
  MOCK_METHOD1(OnBufferRetired, void(int buffer_id));
  MOCK_METHOD0(OnStarted, void());

  void OnNewBufferHandle(
      int buffer_id,
      std::unique_ptr<media::VideoCaptureDevice::Client::Buffer::HandleProvider>
          handle_provider) override {
    MockOnNewBufferHandle(buffer_id);
  }

  void OnFrameReadyInBuffer(
      int32_t buffer_id,
      int frame_feedback_id,
      std::unique_ptr<
          media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
          buffer_read_permission,
      media::mojom::VideoFrameInfoPtr frame_info) override {
    MockOnFrameReadyInBuffer(buffer_id, &buffer_read_permission,
                             frame_info->coded_size);
  }
};

std::unique_ptr<media::VideoCaptureJpegDecoder> ReturnNullPtrAsJpecDecoder() {
  return nullptr;
}

// Test fixture for testing a unit consisting of an instance of
// VideoCaptureDeviceClient connected to a partly-mocked instance of
// VideoCaptureController, and an instance of VideoCaptureBufferPoolImpl
// as well as related threading glue that replicates how it is used in
// production.
class VideoCaptureDeviceClientTest : public ::testing::Test {
 public:
  VideoCaptureDeviceClientTest() {
    scoped_refptr<media::VideoCaptureBufferPoolImpl> buffer_pool(
        new media::VideoCaptureBufferPoolImpl(
            base::MakeUnique<media::VideoCaptureBufferTrackerFactoryImpl>(),
            1));
    auto controller = base::MakeUnique<MockVideoCaptureController>();
    controller_ = controller.get();
    device_client_ = base::MakeUnique<media::VideoCaptureDeviceClient>(
        std::move(controller), buffer_pool,
        base::Bind(&ReturnNullPtrAsJpecDecoder));
  }
  ~VideoCaptureDeviceClientTest() override {}

 protected:
  MockVideoCaptureController* controller_;
  std::unique_ptr<media::VideoCaptureDeviceClient> device_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceClientTest);
};

}  // namespace

// A small test for reference and to verify VideoCaptureDeviceClient is
// minimally functional.
TEST_F(VideoCaptureDeviceClientTest, Minimal) {
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  const media::VideoCaptureFormat kFrameFormat(
      gfx::Size(10, 10), 30.0f /*frame_rate*/, media::PIXEL_FORMAT_I420,
      media::PIXEL_STORAGE_CPU);
  DCHECK(device_client_.get());
  {
    InSequence s;
    const int expected_buffer_id = 0;
    EXPECT_CALL(*controller_, OnLog(_));
    EXPECT_CALL(*controller_, MockOnNewBufferHandle(expected_buffer_id));
    EXPECT_CALL(*controller_,
                MockOnFrameReadyInBuffer(expected_buffer_id, _, _));
    EXPECT_CALL(*controller_, OnBufferRetired(expected_buffer_id));
  }
  device_client_->OnIncomingCapturedData(data, kScratchpadSizeInBytes,
                                         kFrameFormat, 0 /*clockwise rotation*/,
                                         base::TimeTicks(), base::TimeDelta());
  // Releasing |device_client_| will also release |controller_|.
  device_client_.reset();
}

// Tests that we don't try to pass on frames with an invalid frame format.
TEST_F(VideoCaptureDeviceClientTest, FailsSilentlyGivenInvalidFrameFormat) {
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  // kFrameFormat is invalid in a number of ways.
  const media::VideoCaptureFormat kFrameFormat(
      gfx::Size(media::limits::kMaxDimension + 1, media::limits::kMaxDimension),
      media::limits::kMaxFramesPerSecond + 1,
      media::VideoPixelFormat::PIXEL_FORMAT_I420,
      media::VideoPixelStorage::PIXEL_STORAGE_CPU);
  DCHECK(device_client_.get());
  // Expect the the call to fail silently inside the VideoCaptureDeviceClient.
  EXPECT_CALL(*controller_, OnLog(_)).Times(1);
  EXPECT_CALL(*controller_, MockOnFrameReadyInBuffer(_, _, _)).Times(0);
  device_client_->OnIncomingCapturedData(data, kScratchpadSizeInBytes,
                                         kFrameFormat, 0 /*clockwise rotation*/,
                                         base::TimeTicks(), base::TimeDelta());
  Mock::VerifyAndClearExpectations(controller_);
}

// Tests that we fail silently if no available buffers to use.
TEST_F(VideoCaptureDeviceClientTest, DropsFrameIfNoBuffer) {
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  const media::VideoCaptureFormat kFrameFormat(
      gfx::Size(10, 10), 30.0f /*frame_rate*/, media::PIXEL_FORMAT_I420,
      media::PIXEL_STORAGE_CPU);
  EXPECT_CALL(*controller_, OnLog(_)).Times(1);
  // Simulate that receiver still holds |buffer_read_permission| for the first
  // buffer when the second call to OnIncomingCapturedData comes in.
  // Since we set up the buffer pool to max out at 1 buffer, this should cause
  // |device_client_| to drop the frame.
  std::unique_ptr<VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
      read_permission;
  EXPECT_CALL(*controller_, MockOnFrameReadyInBuffer(_, _, _))
      .WillOnce(Invoke([&read_permission](
          int buffer_id,
          std::unique_ptr<media::VideoCaptureDevice::Client::Buffer::
                              ScopedAccessPermission>* buffer_read_permission,
          const gfx::Size&) {
        read_permission = std::move(*buffer_read_permission);
      }));
  // Pass two frames. The second will be dropped.
  device_client_->OnIncomingCapturedData(data, kScratchpadSizeInBytes,
                                         kFrameFormat, 0 /*clockwise rotation*/,
                                         base::TimeTicks(), base::TimeDelta());
  device_client_->OnIncomingCapturedData(data, kScratchpadSizeInBytes,
                                         kFrameFormat, 0 /*clockwise rotation*/,
                                         base::TimeTicks(), base::TimeDelta());
  Mock::VerifyAndClearExpectations(controller_);
}

// Tests that buffer-based capture API accepts some memory-backed pixel formats.
TEST_F(VideoCaptureDeviceClientTest, DataCaptureGoodPixelFormats) {
  // The usual ReserveOutputBuffer() -> OnIncomingCapturedVideoFrame() cannot
  // be used since it does not accept all pixel formats. The memory backed
  // buffer OnIncomingCapturedData() is used instead, with a dummy scratchpad
  // buffer.
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  const gfx::Size kCaptureResolution(10, 10);
  ASSERT_GE(kScratchpadSizeInBytes, kCaptureResolution.GetArea() * 4u)
      << "Scratchpad is too small to hold the largest pixel format (ARGB).";

  media::VideoCaptureParams params;
  params.requested_format = media::VideoCaptureFormat(
      kCaptureResolution, 30.0f, media::PIXEL_FORMAT_UNKNOWN);

  // Only use the VideoPixelFormats that we know supported. Do not add
  // PIXEL_FORMAT_MJPEG since it would need a real JPEG header.
  const media::VideoPixelFormat kSupportedFormats[] = {
    media::PIXEL_FORMAT_I420,
    media::PIXEL_FORMAT_YV12,
    media::PIXEL_FORMAT_NV12,
    media::PIXEL_FORMAT_NV21,
    media::PIXEL_FORMAT_YUY2,
    media::PIXEL_FORMAT_UYVY,
#if defined(OS_WIN) || defined(OS_LINUX)
    media::PIXEL_FORMAT_RGB24,
#endif
    media::PIXEL_FORMAT_RGB32,
    media::PIXEL_FORMAT_ARGB,
    media::PIXEL_FORMAT_Y16,
  };

  for (media::VideoPixelFormat format : kSupportedFormats) {
    params.requested_format.pixel_format = format;

    EXPECT_CALL(*controller_, OnLog(_)).Times(1);
    EXPECT_CALL(*controller_, MockOnFrameReadyInBuffer(_, _, _)).Times(1);
    device_client_->OnIncomingCapturedData(
        data, params.requested_format.ImageAllocationSize(),
        params.requested_format, 0 /* clockwise_rotation */, base::TimeTicks(),
        base::TimeDelta());
    Mock::VerifyAndClearExpectations(controller_);
  }
}

// Test that we receive the expected resolution for a given captured frame
// resolution and rotation. Odd resolutions are also cropped.
TEST_F(VideoCaptureDeviceClientTest, CheckRotationsAndCrops) {
  const struct SizeAndRotation {
    gfx::Size input_resolution;
    int rotation;
    gfx::Size output_resolution;
  } kSizeAndRotations[] = {{{6, 4}, 0, {6, 4}},   {{6, 4}, 90, {4, 6}},
                           {{6, 4}, 180, {6, 4}}, {{6, 4}, 270, {4, 6}},
                           {{7, 4}, 0, {6, 4}},   {{7, 4}, 90, {4, 6}},
                           {{7, 4}, 180, {6, 4}}, {{7, 4}, 270, {4, 6}}};

  // The usual ReserveOutputBuffer() -> OnIncomingCapturedVideoFrame() cannot
  // be used since it does not resolve rotations or crops. The memory backed
  // buffer OnIncomingCapturedData() is used instead, with a dummy scratchpad
  // buffer.
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};

  EXPECT_CALL(*controller_, OnLog(_)).Times(1);

  media::VideoCaptureParams params;
  for (const auto& size_and_rotation : kSizeAndRotations) {
    ASSERT_GE(kScratchpadSizeInBytes,
              size_and_rotation.input_resolution.GetArea() * 4u)
        << "Scratchpad is too small to hold the largest pixel format (ARGB).";
    params.requested_format = media::VideoCaptureFormat(
        size_and_rotation.input_resolution, 30.0f, media::PIXEL_FORMAT_ARGB);
    gfx::Size coded_size;
    EXPECT_CALL(*controller_, MockOnFrameReadyInBuffer(_, _, _))
        .Times(1)
        .WillOnce(SaveArg<2>(&coded_size));
    device_client_->OnIncomingCapturedData(
        data, params.requested_format.ImageAllocationSize(),
        params.requested_format, size_and_rotation.rotation, base::TimeTicks(),
        base::TimeDelta());

    EXPECT_EQ(coded_size.width(), size_and_rotation.output_resolution.width());
    EXPECT_EQ(coded_size.height(),
              size_and_rotation.output_resolution.height());

    Mock::VerifyAndClearExpectations(controller_);
  }
}

}  // namespace media
