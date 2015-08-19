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

// TODO(dshwang): replace it with media::VideoPixelFormat. crbug.com/489744
// Color formats from camera. This list is sorted in order of preference.
// TODO(emircan): http://crbug.com/521068 Consider if this list can be merged
// with media::Format.
// TODO(mcasas): http://crbug.com/504160 Consider making this an enum class.
enum VideoCapturePixelFormat {
  VIDEO_CAPTURE_PIXEL_FORMAT_I420,
  VIDEO_CAPTURE_PIXEL_FORMAT_YV12,
  VIDEO_CAPTURE_PIXEL_FORMAT_NV12,
  VIDEO_CAPTURE_PIXEL_FORMAT_NV21,
  VIDEO_CAPTURE_PIXEL_FORMAT_UYVY,
  VIDEO_CAPTURE_PIXEL_FORMAT_YUY2,
  VIDEO_CAPTURE_PIXEL_FORMAT_RGB24,
  VIDEO_CAPTURE_PIXEL_FORMAT_RGB32,
  VIDEO_CAPTURE_PIXEL_FORMAT_ARGB,
  VIDEO_CAPTURE_PIXEL_FORMAT_MJPEG,
  VIDEO_CAPTURE_PIXEL_FORMAT_UNKNOWN,  // Color format not set.
  VIDEO_CAPTURE_PIXEL_FORMAT_MAX = VIDEO_CAPTURE_PIXEL_FORMAT_UNKNOWN,
};

// Storage type for the pixels. In principle, all combinations of Storage and
// Format are possible, though some are very typical, such as texture + ARGB,
// and others are only available if the platform allows it e.g. GpuMemoryBuffer.
// TODO(mcasas): http://crbug.com/504160 Consider making this an enum class.
enum VideoPixelStorage {
  PIXEL_STORAGE_CPU,
  PIXEL_STORAGE_TEXTURE,
  PIXEL_STORAGE_GPUMEMORYBUFFER,
  PIXEL_STORAGE_MAX = PIXEL_STORAGE_GPUMEMORYBUFFER,
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

  // Must always be equal to largest entry in the enum.
  RESOLUTION_POLICY_LAST = RESOLUTION_POLICY_ANY_WITHIN_LIMIT,
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
                     VideoCapturePixelFormat pixel_format);
  VideoCaptureFormat(const gfx::Size& frame_size,
                     float frame_rate,
                     VideoCapturePixelFormat pixel_format,
                     VideoPixelStorage pixel_storage);

  static std::string ToString(const VideoCaptureFormat& format);
  static std::string PixelFormatToString(VideoCapturePixelFormat format);
  static std::string PixelStorageToString(VideoPixelStorage storage);

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
  VideoCapturePixelFormat pixel_format;
  VideoPixelStorage pixel_storage;
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
           resolution_change_policy == other.resolution_change_policy;
  }

  // Requests a resolution and format at which the capture will occur.
  VideoCaptureFormat requested_format;

  // Policy for resolution change.
  ResolutionChangePolicy resolution_change_policy;
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_CAPTURE_TYPES_H_
