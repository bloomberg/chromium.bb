// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_

#include <vector>

#include "media/base/media_export.h"
#include "ui/gfx/size.h"

namespace media {

// TODO(wjia): this type should be defined in a common place and
// shared with device manager.
typedef int VideoCaptureSessionId;

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
  PIXEL_FORMAT_TEXTURE,  // Capture format as a GL texture.
  PIXEL_FORMAT_MAX,
};

// Policies for capture devices that has source content with dynamic resolution.
enum ResolutionChangePolicy {
  // Capture device outputs a fixed resolution all the time. The resolution of
  // the first frame is the resolution for all frames.
  // It is implementation specific for the capture device to scale, letter-box
  // and pillar-box. The only gurantee is that resolution will never change.
  RESOLUTION_POLICY_FIXED,

  // Capture device outputs frames with dynamic resolution. The width and height
  // will not exceed the maximum dimensions specified. The typical scenario is
  // the frames will have the same aspect ratio as the original content and
  // scaled down to fit inside the limit.
  RESOLUTION_POLICY_DYNAMIC_WITHIN_LIMIT,

  RESOLUTION_POLICY_LAST,
};

// Some drivers use rational time per frame instead of float frame rate, this
// constant k is used to convert between both: A fps -> [k/k*A] seconds/frame.
const int kFrameRatePrecision = 10000;

// Video capture format specification.
// This class is used by the video capture device to specify the format of every
// frame captured and returned to a client. It is also used to specify a
// supported capture format by a device.
class MEDIA_EXPORT VideoCaptureFormat {
 public:
  VideoCaptureFormat();
  VideoCaptureFormat(const gfx::Size& frame_size,
                     float frame_rate,
                     VideoPixelFormat pixel_format);

  std::string ToString() const;
  static std::string PixelFormatToString(VideoPixelFormat format);

  // Checks that all values are in the expected range. All limits are specified
  // in media::Limits.
  bool IsValid() const;

  gfx::Size frame_size;
  float frame_rate;
  VideoPixelFormat pixel_format;
};

typedef std::vector<VideoCaptureFormat> VideoCaptureFormats;

// Parameters for starting video capture.
// This class is used by the client of a video capture device to specify the
// format of frames in which the client would like to have captured frames
// returned.
class MEDIA_EXPORT VideoCaptureParams {
 public:
  VideoCaptureParams();

  // Requests a resolution and format at which the capture will occur.
  VideoCaptureFormat requested_format;

  // Policy for resolution change.
  ResolutionChangePolicy resolution_change_policy;
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_TYPES_H_
