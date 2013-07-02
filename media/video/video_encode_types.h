// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_VIDEO_VIDEO_ENCODE_TYPES_H_
#define MEDIA_VIDEO_VIDEO_ENCODE_TYPES_H_

#include <map>
#include <ostream>
#include <vector>

#include "base/time/time.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/size.h"

namespace media {

// Data to represent limitations for a particular encoder config.
struct VideoEncodingConfig {
  VideoCodec codec_type;
  std::string codec_name;
  gfx::Size max_resolution;
  int max_frames_per_second;
  int max_bitrate;
};

typedef std::vector<VideoEncodingConfig> VideoEncodingCapabilities;

// Encoding parameters that can be configured during streaming without removing
// the bitstream first.
struct RuntimeVideoEncodingParameters {
  int target_bitrate;
  int max_bitrate;
  int frames_per_second;
};

// Generic video encoding parameters to be configured during initialization
// time.
struct VideoEncodingParameters {
  std::string codec_name;
  gfx::Size resolution;
  RuntimeVideoEncodingParameters runtime_params;
};

struct BufferEncodingMetadata {
  base::Time timestamp;
  bool key_frame;
};

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_ENCODE_TYPES_H_
