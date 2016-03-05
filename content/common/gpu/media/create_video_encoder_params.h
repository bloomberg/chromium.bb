// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_CREATE_VIDEO_ENCODER_PARAMS_H_
#define CONTENT_COMMON_GPU_MEDIA_CREATE_VIDEO_ENCODER_PARAMS_H_

#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/size.h"

namespace content {

struct CreateVideoEncoderParams {
  CreateVideoEncoderParams();
  ~CreateVideoEncoderParams();
  media::VideoPixelFormat input_format;
  gfx::Size input_visible_size;
  media::VideoCodecProfile output_profile;
  uint32_t initial_bitrate;
  int32_t encoder_route_id;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_CREATE_VIDEO_ENCODER_PARAMS_H_
