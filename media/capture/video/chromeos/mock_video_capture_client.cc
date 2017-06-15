// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/mock_video_capture_client.h"

#include <string>
#include <utility>

using testing::_;
using testing::Invoke;

namespace media {
namespace unittest_internal {

MockVideoCaptureClient::MockVideoCaptureClient() {
  ON_CALL(*this, OnError(_, _))
      .WillByDefault(Invoke(this, &MockVideoCaptureClient::DumpError));
}

MockVideoCaptureClient::~MockVideoCaptureClient() {
  if (quit_cb_) {
    std::move(quit_cb_).Run();
  }
}

void MockVideoCaptureClient::SetFrameCb(base::OnceClosure frame_cb) {
  frame_cb_ = std::move(frame_cb);
}

void MockVideoCaptureClient::SetQuitCb(base::OnceClosure quit_cb) {
  quit_cb_ = std::move(quit_cb);
}

void MockVideoCaptureClient::DumpError(
    const tracked_objects::Location& location,
    const std::string& message) {
  DPLOG(ERROR) << location.ToString() << " " << message;
}

void MockVideoCaptureClient::OnIncomingCapturedData(
    const uint8_t* data,
    int length,
    const VideoCaptureFormat& format,
    int rotation,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    int frame_feedback_id) {
  ASSERT_GT(length, 0);
  ASSERT_TRUE(data);
  if (frame_cb_) {
    std::move(frame_cb_).Run();
  }
}

// Trampoline methods to workaround GMOCK problems with std::unique_ptr<>.
VideoCaptureDevice::Client::Buffer MockVideoCaptureClient::ReserveOutputBuffer(
    const gfx::Size& dimensions,
    media::VideoPixelFormat format,
    media::VideoPixelStorage storage,
    int frame_feedback_id) {
  DoReserveOutputBuffer();
  NOTREACHED() << "This should never be called";
  return Buffer();
}

void MockVideoCaptureClient::OnIncomingCapturedBuffer(
    Buffer buffer,
    const VideoCaptureFormat& format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  DoOnIncomingCapturedBuffer();
}

void MockVideoCaptureClient::OnIncomingCapturedBufferExt(
    Buffer buffer,
    const VideoCaptureFormat& format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    gfx::Rect visible_rect,
    const VideoFrameMetadata& additional_metadata) {
  DoOnIncomingCapturedVideoFrame();
}

VideoCaptureDevice::Client::Buffer
MockVideoCaptureClient::ResurrectLastOutputBuffer(
    const gfx::Size& dimensions,
    media::VideoPixelFormat format,
    media::VideoPixelStorage storage,
    int frame_feedback_id) {
  DoResurrectLastOutputBuffer();
  NOTREACHED() << "This should never be called";
  return Buffer();
}

}  // namespace unittest_internal
}  // namespace media
