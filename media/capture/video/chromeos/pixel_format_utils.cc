// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/pixel_format_utils.h"

#include <libdrm/drm_fourcc.h>

#include <algorithm>

namespace media {

namespace {

struct SupportedFormat {
  VideoPixelFormat chromium_format;
  arc::mojom::HalPixelFormat hal_format;
  uint32_t drm_format;
} const kSupportedFormats[] = {
    // The Android camera HAL v3 has three types of mandatory pixel formats:
    //
    //   1. HAL_PIXEL_FORMAT_YCbCr_420_888 (YUV flexible format).
    //   2. HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED (platform-specific format).
    //   3. HAL_PIXEL_FORMAT_BLOB (for JPEG).
    //
    // We can't use HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED as it is highly
    // platform specific and there is no way for Chrome to query the exact
    // pixel layout of the implementation-defined buffer.
    //
    // On Android the framework requests the preview stream with the
    // implementation-defined format, and as a result some camera HALs support
    // only implementation-defined preview buffers.  We should use the video
    // capture stream in Chrome VCD as it is mandatory for the camera HAL to
    // support YUV flexbile format video streams.

    // TODO(jcliang): Change NV12 to I420 after the camera HAL supports handling
    //                I420 buffers.
    {PIXEL_FORMAT_NV12,
     arc::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888,
     DRM_FORMAT_NV12},
};

}  // namespace

VideoPixelFormat PixFormatHalToChromium(arc::mojom::HalPixelFormat from) {
  auto* it =
      std::find_if(std::begin(kSupportedFormats), std::end(kSupportedFormats),
                   [from](SupportedFormat f) { return f.hal_format == from; });
  if (it == std::end(kSupportedFormats)) {
    return PIXEL_FORMAT_UNKNOWN;
  }
  return it->chromium_format;
}

uint32_t PixFormatChromiumToDrm(VideoPixelFormat from) {
  auto* it = std::find_if(
      std::begin(kSupportedFormats), std::end(kSupportedFormats),
      [from](SupportedFormat f) { return f.chromium_format == from; });
  if (it == std::end(kSupportedFormats)) {
    return 0;
  }
  return it->drm_format;
}

}  // namespace media
