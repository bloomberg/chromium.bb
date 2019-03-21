// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mock_video_capture_device_client.h"

namespace media {

MockVideoCaptureDeviceClient::MockVideoCaptureDeviceClient() = default;
MockVideoCaptureDeviceClient::~MockVideoCaptureDeviceClient() = default;

void MockVideoCaptureDeviceClient::OnIncomingCapturedBuffer(
    Buffer buffer,
    const media::VideoCaptureFormat& format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  DoOnIncomingCapturedBuffer(buffer, format, reference_time, timestamp);
}
void MockVideoCaptureDeviceClient::OnIncomingCapturedBufferExt(
    Buffer buffer,
    const media::VideoCaptureFormat& format,
    const gfx::ColorSpace& color_space,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    gfx::Rect visible_rect,
    const media::VideoFrameMetadata& additional_metadata) {
  DoOnIncomingCapturedBufferExt(buffer, format, color_space, reference_time,
                                timestamp, visible_rect, additional_metadata);
}

}  // namespace media
