// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_decode_engine.h"

namespace media {

VideoCodecConfig::VideoCodecConfig()
    : codec(kCodecH264),
      profile(kProfileDoNotCare),
      level(kLevelDoNotCare),
      width(0),
      height(0),
      opaque_context(NULL) {
}

}  // namespace media
