// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_CAPTURE_TYPES_H_
#define MEDIA_BASE_VIDEO_CAPTURE_TYPES_H_

#include <vector>

#include "build/build_config.h"
#include "media/base/media_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// TODO(wjia): this type should be defined in a common place and
// shared with device manager.
typedef int VideoCaptureSessionId;

// Color formats from camera. This list is sorted in order of preference.
enum VideoPixelFormat {
  PIXEL_FORMAT_I420,
  PIXEL_FORMAT_YV12,
  PIXEL_FORMAT_NV12,
  PIXEL_FORMAT_NV21,
  PIXEL_FORMAT_UYVY,
  PIXEL_FORMAT_YUY2,
  PIXEL_FORMAT_RGB24,
  PIXEL_FORMAT_RGB32,
  PIXEL_FORMAT_ARGB,
  PIXEL_FORMAT_MJPEG,
  PIXEL_FORMAT_TEXTURE,  // Capture format as a GL texture.
  PIXEL_FORMAT_GPUMEMORYBUFFER,
  PIXEL_FORMAT_UNKNOWN,  // Color format not set.
  PIXEL_FORMAT_MAX,
};

// Policies for capture devices that have source content that varies in size.
// It is up to the implementation how the captured content will be transformed
// (e.g., scaling and/or letterboxing) in order to produce video frames that
// strictly adheree to one of these policies.
enum ResolutionChangePolicy {
  // Capture device outputs a fixed resolution all the time. The resolution of
  // the first frame is the resolution for all frames.
  RESOLUTION_POLICY_FIXED_RESOLUTION,

  // Capture device is allowed to output frames of varying resolutions. The
  // width and height will not exceed the maximum dimensions specified. The
  // aspect ratio of the frames will match the aspect ratio of the maximum
  // dimensions as closely as possible.
  RESOLUTION_POLICY_FIXED_ASPECT_RATIO,

  // Capture device is allowed to output frames of varying resolutions not
  // exceeding the maximum dimensions specified.
  RESOLUTION_POLICY_ANY_WITHIN_LIMIT,

  RESOLUTION_POLICY_LAST,
};

// Some drivers use rational time per frame instead of float frame rate, this
// constant k is used to convert between both: A fps -> [k/k*A] seconds/frame.
const int kFrameRatePrecision = 10000;

// Video capture format specification.
// This class is used by the video capture device to specify the format of every
// frame captured and returned to a client. It is also used to specify a
// supported capture format by a device.
struct MEDIA_EXPORT VideoCaptureFormat {
  VideoCaptureFormat();
  VideoCaptureFormat(const gfx::Size& frame_size,
                     float frame_rate,
                     VideoPixelFormat pixel_format);

  std::string ToString() const;
  static std::string PixelFormatToString(VideoPixelFormat format);

  // Returns the required buffer size to hold an image of a given
  // VideoCaptureFormat with no padding and tightly packed.
  size_t ImageAllocationSize() const;

  // Checks that all values are in the expected range. All limits are specified
  // in media::Limits.
  bool IsValid() const;

  bool operator==(const VideoCaptureFormat& other) const {
    return frame_size == other.frame_size &&
        frame_rate == other.frame_rate &&
        pixel_format == other.pixel_format;
  }

  gfx::Size frame_size;
  float frame_rate;
  VideoPixelFormat pixel_format;
};

typedef std::vector<VideoCaptureFormat> VideoCaptureFormats;

// Parameters for starting video capture.
// This class is used by the client of a video capture device to specify the
// format of frames in which the client would like to have captured frames
// returned.
struct MEDIA_EXPORT VideoCaptureParams {
  VideoCaptureParams();

  bool operator==(const VideoCaptureParams& other) const {
    return requested_format == other.requested_format &&
#if defined(OS_LINUX)
        use_native_gpu_memory_buffers == other.use_native_gpu_memory_buffers &&
#endif
        resolution_change_policy == other.resolution_change_policy;
  }

  // Requests a resolution and format at which the capture will occur.
  VideoCaptureFormat requested_format;

  // Policy for resolution change.
  ResolutionChangePolicy resolution_change_policy;

#if defined(OS_LINUX)
  // Indication to the Driver to try to use native GpuMemoryBuffers.
  bool use_native_gpu_memory_buffers;
#endif
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_CAPTURE_TYPES_H_
