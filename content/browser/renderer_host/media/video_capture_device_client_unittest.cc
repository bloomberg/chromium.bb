// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_device_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::InSequence;
using ::testing::SaveArg;

namespace content {

namespace {

// This implementation of MockVideoCaptureControllerEventHandler is
// taken from video_capture_controller_unittest.cc.
// TODO(ajose): Consider moving this mock to a shared header file.
class MockVideoCaptureControllerEventHandler
    : public VideoCaptureControllerEventHandler {
 public:
  explicit MockVideoCaptureControllerEventHandler(
      VideoCaptureController* controller)
      : controller_(controller), resource_utilization_(-1.0) {}
  ~MockVideoCaptureControllerEventHandler() override {}

  // These mock methods are delegated to by our fake implementation of
  // VideoCaptureControllerEventHandler, to be used in EXPECT_CALL().
  MOCK_METHOD1(DoBufferCreated, void(VideoCaptureControllerID));
  MOCK_METHOD1(DoBufferDestroyed, void(VideoCaptureControllerID));
  MOCK_METHOD2(DoI420BufferReady,
               void(VideoCaptureControllerID, const gfx::Size&));
  MOCK_METHOD2(DoTextureBufferReady,
               void(VideoCaptureControllerID, const gfx::Size&));
  MOCK_METHOD2(DoBufferReady, void(VideoCaptureControllerID, const gfx::Size&));
  MOCK_METHOD1(DoEnded, void(VideoCaptureControllerID));
  MOCK_METHOD1(DoError, void(VideoCaptureControllerID));

  void OnError(VideoCaptureControllerID id) override { DoError(id); }

  void OnBufferCreated(VideoCaptureControllerID id,
                       base::SharedMemoryHandle handle,
                       int length,
                       int buffer_id) override {
    DoBufferCreated(id);
  }

  void OnBufferDestroyed(VideoCaptureControllerID id, int buffer_id) override {
    DoBufferDestroyed(id);
  }

  void OnBufferReady(VideoCaptureControllerID id,
                     int buffer_id,
                     const scoped_refptr<media::VideoFrame>& frame,
                     const base::TimeTicks& timestamp) override {
    DoBufferReady(id, frame->coded_size());
  }

  void OnEnded(VideoCaptureControllerID id) override { DoEnded(id); }

  VideoCaptureController* controller_;
  double resource_utilization_;
};

class VideoCaptureDeviceClientTest : public ::testing::Test {
 public:
  VideoCaptureDeviceClientTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        controller_(new VideoCaptureController(20)),
        device_client_(
            controller_->NewDeviceClient(base::ThreadTaskRunnerHandle::Get())),
        controller_client_(
            new MockVideoCaptureControllerEventHandler(controller_.get())) {}
  ~VideoCaptureDeviceClientTest() override {}

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

 protected:
  const content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<VideoCaptureController> controller_;
  scoped_ptr<media::VideoCaptureDevice::Client> device_client_;
  scoped_ptr<MockVideoCaptureControllerEventHandler> controller_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceClientTest);
};

}  // namespace

// Tests that buffer-based capture API accepts all memory-backed pixel formats.
TEST_F(VideoCaptureDeviceClientTest, DataCaptureInEachVideoFormatInSequence) {
  // The usual ReserveOutputBuffer() -> OnIncomingCapturedVideoFrame() cannot
  // be used since it does not accept all pixel formats. The memory backed
  // buffer OnIncomingCapturedData() is used instead, with a dummy scratchpad
  // buffer.
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};
  const gfx::Size capture_resolution(10, 10);
  ASSERT_GE(kScratchpadSizeInBytes, capture_resolution.GetArea() * 4u)
      << "Scratchpad is too small to hold the largest pixel format (ARGB).";

  const int kSessionId = 100;
  for (int format = 0; format < media::VIDEO_CAPTURE_PIXEL_FORMAT_MAX;
       ++format) {
    // Convertion from MJPEG to I420 seems to be unsupported.
    if (format == media::VIDEO_CAPTURE_PIXEL_FORMAT_UNKNOWN ||
        format == media::VIDEO_CAPTURE_PIXEL_FORMAT_MJPEG) {
      continue;
    }
#if !defined(OS_LINUX) && !defined(OS_WIN)
    if (format == media::VIDEO_CAPTURE_PIXEL_FORMAT_RGB24){
      continue;
    }
#endif
    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        capture_resolution, 30, media::VideoCapturePixelFormat(format));

    const VideoCaptureControllerID kRouteId(0x99);
    controller_->AddClient(kRouteId, controller_client_.get(),
                           base::kNullProcessHandle, kSessionId, params);
    ASSERT_EQ(1, controller_->GetClientCount());
    {
      InSequence s;
      EXPECT_CALL(*controller_client_, DoBufferCreated(kRouteId)).Times(1);
      EXPECT_CALL(*controller_client_, DoBufferReady(kRouteId, _)).Times(1);
    }
    device_client_->OnIncomingCapturedData(
        data, params.requested_format.ImageAllocationSize(),
        params.requested_format, 0 /* clockwise_rotation */, base::TimeTicks());
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(kSessionId,
              controller_->RemoveClient(kRouteId, controller_client_.get()));
    Mock::VerifyAndClearExpectations(controller_client_.get());
  }
}

// Test that we receive the expected resolution for a given captured frame
// resolution and rotation. Odd resolutions are also cropped.
TEST_F(VideoCaptureDeviceClientTest, CheckRotationsAndCrops) {
  const int kSessionId = 100;
  const struct SizeAndRotation {
    gfx::Size input_resolution;
    int rotation;
    gfx::Size output_resolution;
  } kSizeAndRotations[] = {{{6, 4}, 0, {6, 4}},
                           {{6, 4}, 90, {4, 6}},
                           {{6, 4}, 180, {6, 4}},
                           {{6, 4}, 270, {4, 6}},
                           {{7, 4}, 0, {6, 4}},
                           {{7, 4}, 90, {4, 6}},
                           {{7, 4}, 180, {6, 4}},
                           {{7, 4}, 270, {4, 6}}};

  // The usual ReserveOutputBuffer() -> OnIncomingCapturedVideoFrame() cannot
  // be used since it does not resolve rotations or crops. The memory backed
  // buffer OnIncomingCapturedData() is used instead, with a dummy scratchpad
  // buffer.
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};

  media::VideoCaptureParams params;
  for (const auto& size_and_rotation : kSizeAndRotations) {
    ASSERT_GE(kScratchpadSizeInBytes,
              size_and_rotation.input_resolution.GetArea() * 4u)
        << "Scratchpad is too small to hold the largest pixel format (ARGB).";

    params.requested_format =
        media::VideoCaptureFormat(size_and_rotation.input_resolution, 30,
                                  media::VIDEO_CAPTURE_PIXEL_FORMAT_ARGB);

    const VideoCaptureControllerID kRouteId(0x99);
    controller_->AddClient(kRouteId, controller_client_.get(),
                           base::kNullProcessHandle, kSessionId, params);
    ASSERT_EQ(1, controller_->GetClientCount());

    gfx::Size coded_size;
    {
      InSequence s;
      EXPECT_CALL(*controller_client_, DoBufferCreated(kRouteId)).Times(1);
      EXPECT_CALL(*controller_client_, DoBufferReady(kRouteId, _))
          .Times(1)
          .WillOnce(SaveArg<1>(&coded_size));
    }
    device_client_->OnIncomingCapturedData(
        data, params.requested_format.ImageAllocationSize(),
        params.requested_format, size_and_rotation.rotation, base::TimeTicks());
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(coded_size.width(), size_and_rotation.output_resolution.width());
    EXPECT_EQ(coded_size.height(),
              size_and_rotation.output_resolution.height());

    EXPECT_EQ(kSessionId,
              controller_->RemoveClient(kRouteId, controller_client_.get()));
    Mock::VerifyAndClearExpectations(controller_client_.get());
  }
}

}  // namespace content
