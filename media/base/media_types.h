// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_TYPES_H_
#define MEDIA_BASE_MEDIA_TYPES_H_

#include "media/base/audio_codecs.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"

namespace media {

// These structures represent parsed audio/video content types (mime strings).
// These are a subset of {Audio|Video}DecoderConfig classes, which can only be
// created after demuxing.

struct MEDIA_EXPORT AudioType {
  AudioCodec codec;
};

struct MEDIA_EXPORT VideoType {
  VideoCodec codec;
  VideoCodecProfile profile;
  int level;
  VideoColorSpace color_space;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_TYPES_H_
