// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_encoder.h"
#include "media/cast/sender/video_frame_factory.h"

namespace media {
namespace cast {

scoped_ptr<VideoFrameFactory> VideoEncoder::CreateVideoFrameFactory() {
  return nullptr;
}

}  // namespace cast
}  // namespace media
