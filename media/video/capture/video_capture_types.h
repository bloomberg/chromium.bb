// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_

#include "media/base/video_frame.h"

namespace media {

// TODO(wjia): this type should be defined in a common place and
// shared with device manager.
typedef int VideoCaptureSessionId;

enum VideoCaptureResolutionType {
  ConstantResolutionVideoCaptureDevice = 0,
  VariableResolutionVideoCaptureDevice,
  MaxVideoCaptureResolutionType,  // Must be last.
};

// Parameters for starting video capture and device information.
struct VideoCaptureParams {
  VideoCaptureParams()
      : width(0),
        height(0),
        frame_per_second(0),
        session_id(0),
        frame_size_type(ConstantResolutionVideoCaptureDevice) {};
  int width;
  int height;
  int frame_per_second;
  VideoCaptureSessionId session_id;
  VideoCaptureResolutionType frame_size_type;
};

// Capabilities describe the format a camera capture video in.
struct VideoCaptureCapability {
  // Color formats from camera.
  enum Format {
    kColorUnknown,  // Color format not set.
    kI420,
    kYUY2,
    kUYVY,
    kRGB24,
    kARGB,
    kMJPEG,
    kNV21,
    kYV12,
  };

  VideoCaptureCapability()
      : width(0),
        height(0),
        frame_rate(0),
        color(kColorUnknown),
        expected_capture_delay(0),
        interlaced(false),
        frame_size_type(ConstantResolutionVideoCaptureDevice),
        session_id(0) {};
  VideoCaptureCapability(int width,
                         int height,
                         int frame_rate,
                         Format color,
                         int delay,
                         bool interlaced,
                         VideoCaptureResolutionType frame_size_type)
      : width(width),
        height(height),
        frame_rate(frame_rate),
        color(color),
        expected_capture_delay(delay),
        interlaced(interlaced),
        frame_size_type(frame_size_type),
        session_id(0) {};

  int width;                   // Desired width.
  int height;                  // Desired height.
  int frame_rate;              // Desired frame rate.
  Format color;                // Desired video type.
  int expected_capture_delay;  // Expected delay in millisecond.
  bool interlaced;             // Need interlace format.
  VideoCaptureResolutionType frame_size_type;
  VideoCaptureSessionId session_id;
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_
