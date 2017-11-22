// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dispatcher.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/common/media_stream_request.h"
#include "content/renderer/media/mock_mojo_media_stream_dispatcher_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MediaStreamDispatcherTest : public ::testing::Test {
 public:
  MediaStreamDispatcherTest()
      : dispatcher_(std::make_unique<MediaStreamDispatcher>(nullptr)) {}

  void OnDeviceOpened(base::Closure quit_closure,
                      bool success,
                      const std::string& label,
                      const MediaStreamDevice& device) {
    if (success) {
      stream_label_ = label;
      dispatcher_->AddStream(label, device);
    }

    quit_closure.Run();
  }

 protected:
  std::string stream_label_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host_;
  std::unique_ptr<MediaStreamDispatcher> dispatcher_;
};

TEST_F(MediaStreamDispatcherTest, GetNonScreenCaptureDevices) {
  const int kRenderId = 3;
  const int kRequestId1 = 5;
  const int kRequestId2 = 7;

  EXPECT_EQ(dispatcher_->label_stream_map_.size(), 0u);

  // OpenDevice request 1
  base::RunLoop run_loop1;
  mock_dispatcher_host_.OpenDevice(
      kRenderId, kRequestId1, "device_path", MEDIA_DEVICE_VIDEO_CAPTURE,
      base::BindOnce(&MediaStreamDispatcherTest::OnDeviceOpened,
                     base::Unretained(this), run_loop1.QuitClosure()));
  run_loop1.Run();
  std::string stream_label1 = stream_label_;

  // OpenDevice request 2
  base::RunLoop run_loop2;
  mock_dispatcher_host_.OpenDevice(
      kRenderId, kRequestId2, "screen_capture", MEDIA_DESKTOP_VIDEO_CAPTURE,
      base::BindOnce(&MediaStreamDispatcherTest::OnDeviceOpened,
                     base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();
  std::string stream_label2 = stream_label_;

  EXPECT_EQ(dispatcher_->label_stream_map_.size(), 2u);

  // Only the device with type MEDIA_DEVICE_VIDEO_CAPTURE will be returned.
  MediaStreamDevices video_devices = dispatcher_->GetNonScreenCaptureDevices();
  EXPECT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].type, MEDIA_DEVICE_VIDEO_CAPTURE);

  // Close the device from request 2.
  dispatcher_->RemoveStream(stream_label2);
  EXPECT_EQ(dispatcher_->video_session_id(stream_label2),
            MediaStreamDevice::kNoId);

  // Close the device from request 1.
  dispatcher_->RemoveStream(stream_label1);
  EXPECT_EQ(dispatcher_->video_session_id(stream_label1),
            MediaStreamDevice::kNoId);

  // Verify that the request have been completed.
  EXPECT_EQ(dispatcher_->label_stream_map_.size(), 0u);
}

}  // namespace content
