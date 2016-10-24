// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_VIDEO_CAPTURE_STRUCT_TRAITS_H_
#define CONTENT_COMMON_VIDEO_CAPTURE_STRUCT_TRAITS_H_

#include "content/common/video_capture.mojom.h"
#include "media/capture/video_capture_types.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "services/video_capture/public/interfaces/video_capture_format.mojom.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::VideoCaptureParamsDataView,
                    media::VideoCaptureParams> {
  static media::VideoCaptureFormat requested_format(
      const media::VideoCaptureParams& params) {
    return params.requested_format;
  }

  static media::ResolutionChangePolicy resolution_change_policy(
      const media::VideoCaptureParams& params) {
    return params.resolution_change_policy;
  }

  static media::PowerLineFrequency power_line_frequency(
      const media::VideoCaptureParams& params) {
    return params.power_line_frequency;
  }

  static bool Read(content::mojom::VideoCaptureParamsDataView data,
                   media::VideoCaptureParams* out);
};
}

#endif  // CONTENT_COMMON_VIDEO_CAPTURE_STRUCT_TRAITS_H_
