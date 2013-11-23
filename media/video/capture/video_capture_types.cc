// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_types.h"

#include "media/base/limits.h"

namespace media {

VideoCaptureFormat::VideoCaptureFormat()
    : width(0),
      height(0),
      frame_rate(0),
      frame_size_type(ConstantResolutionVideoCaptureDevice) {}

VideoCaptureFormat::VideoCaptureFormat(
    int width,
    int height,
    int frame_rate,
    VideoCaptureResolutionType frame_size_type)
    : width(width),
      height(height),
      frame_rate(frame_rate),
      frame_size_type(frame_size_type) {}

bool VideoCaptureFormat::IsValid() const {
  return (width > 0) && (height > 0) && (frame_rate > 0) &&
         (frame_rate < media::limits::kMaxFramesPerSecond) &&
         (width < media::limits::kMaxDimension) &&
         (height < media::limits::kMaxDimension) &&
         (width * height < media::limits::kMaxCanvas) &&
         (frame_size_type >= 0) &&
         (frame_size_type < media::MaxVideoCaptureResolutionType);
}

VideoCaptureParams::VideoCaptureParams()
    : session_id(0) {}

VideoCaptureCapability::VideoCaptureCapability()
    : color(PIXEL_FORMAT_UNKNOWN) {}

VideoCaptureCapability::VideoCaptureCapability(
    int width,
    int height,
    int frame_rate,
    VideoPixelFormat color,
    VideoCaptureResolutionType frame_size_type)
    : VideoCaptureFormat(width, height, frame_rate, frame_size_type),
      color(color) {}

}  // namespace media
