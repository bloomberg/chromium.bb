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

// Color formats from camera.
enum VideoPixelFormat {
  PIXEL_FORMAT_UNKNOWN,  // Color format not set.
  PIXEL_FORMAT_I420,
  PIXEL_FORMAT_YUY2,
  PIXEL_FORMAT_UYVY,
  PIXEL_FORMAT_RGB24,
  PIXEL_FORMAT_ARGB,
  PIXEL_FORMAT_MJPEG,
  PIXEL_FORMAT_NV21,
  PIXEL_FORMAT_YV12,
};

// Video capture format specification.
class MEDIA_EXPORT VideoCaptureFormat {
 public:
  VideoCaptureFormat();
  VideoCaptureFormat(int width,
                     int height,
                     int frame_rate,
                     VideoCaptureResolutionType frame_size_type);

  // Checks that all values are in the expected range. All limits are specified
  // in media::Limits.
  bool IsValid() const;

  int width;
  int height;
  int frame_rate;
  VideoCaptureResolutionType frame_size_type;
};

// Parameters for starting video capture.
class MEDIA_EXPORT VideoCaptureParams {
 public:
  VideoCaptureParams();
  // Identifies which device is to be started.
  VideoCaptureSessionId session_id;

  // Requests a resolution and format at which the capture will occur.
  VideoCaptureFormat requested_format;
};

// Capabilities describe the format a camera captures video in.
class MEDIA_EXPORT VideoCaptureCapability : public VideoCaptureFormat {
 public:
  VideoCaptureCapability();
  VideoCaptureCapability(int width,
                         int height,
                         int frame_rate,
                         VideoPixelFormat color,
                         VideoCaptureResolutionType frame_size_type);

  VideoPixelFormat color;      // Desired video type.
};

typedef std::vector<VideoCaptureCapability> VideoCaptureCapabilities;

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_
