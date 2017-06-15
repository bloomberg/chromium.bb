// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_PIXEL_FORMAT_UTILS_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_PIXEL_FORMAT_UTILS_H_

#include "media/capture/video/chromeos/mojo/arc_camera3.mojom.h"
#include "media/capture/video_capture_types.h"

namespace media {

// Converts the HAL pixel format |from| to Chromium pixel format.  Returns
// PIXEL_FORMAT_UNKNOWN if |from| is not supported.
VideoPixelFormat PixFormatHalToChromium(arc::mojom::HalPixelFormat from);

// Converts the Chromium pixel format |from| to DRM pixel format.  Returns 0
// if |from| is not supported.
uint32_t PixFormatChromiumToDrm(VideoPixelFormat from);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_PIXEL_FORMAT_UTILS_H_
