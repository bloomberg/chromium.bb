// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_H_
#define MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "media/capture/video/video_capture_device.h"

namespace media {

// Encapsulates factory logic to make a working FakeVideoCaptureDevice based
// on a given target OutputMode and frame rate.
class CAPTURE_EXPORT FakeVideoCaptureDeviceMaker {
 public:
  enum class DeliveryMode {
    USE_DEVICE_INTERNAL_BUFFERS,
    USE_CLIENT_PROVIDED_BUFFERS
  };

  static void GetSupportedSizes(std::vector<gfx::Size>* supported_sizes);

  static std::unique_ptr<VideoCaptureDevice> MakeInstance(
      VideoPixelFormat pixel_format,
      DeliveryMode delivery_mode,
      float frame_rate);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_H_
