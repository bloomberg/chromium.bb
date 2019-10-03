// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_SENDER_CONFIG_H_
#define CAST_STREAMING_SENDER_CONFIG_H_

#include <chrono>  // NOLINT
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "cast/streaming/session_config.h"
#include "cast/streaming/video_codec_params.h"
#include "streaming/cast/rtp_defines.h"

namespace cast {
namespace streaming {

// The configuration used by frame sending classes, i.e. AudioSender
// and VideoSender. The underlying frame sending class is an implementation
// detail, however it is why video specific parameters are given here as
// optional.
struct SenderConfig : public SessionConfig {
  // The total amount of time between a frame capture and its playback on
  // the receiver. This is given as a range, so we can select a value for
  // use by the receiver.
  std::pair<std::chrono::microseconds, std::chrono::microseconds>
      playout_limits{};

  // The initial playout delay for animated content. Must be in the range
  // specified by playout_limits.
  std::chrono::microseconds animated_playout_delay{};

  // If true, use an external hardware encoder.
  bool use_external_encoder = false;

  // Bitrate limits and initial value. Note: for audio, only
  // bitrate_limits.second (max) is used.
  std::pair<int, int> bitrate_limits{};
  int initial_bitrate = 0;

  // Maximum frame rate.
  double max_frame_rate = 0;

  // Optional video only codec parameters.
  std::unique_ptr<VideoCodecParams> video_codec_params;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_SENDER_CONFIG_H_
