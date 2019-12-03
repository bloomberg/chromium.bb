// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_ANSWER_MESSAGES_H_
#define CAST_STREAMING_ANSWER_MESSAGES_H_

#include <array>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "cast/streaming/ssrc.h"
#include "json/value.h"
#include "platform/base/error.h"

namespace cast {
namespace streaming {

struct AudioConstraints {
  int max_sample_rate;
  int max_channels;
  // Technically optional, sender will assume 32kbps if omitted.
  int min_bit_rate;
  int max_bit_rate;
  std::chrono::milliseconds max_delay;

  openscreen::ErrorOr<Json::Value> ToJson() const;
};

struct Dimensions {
  int width;
  int height;
  int frame_rate_numerator;
  int frame_rate_denominator;

  openscreen::ErrorOr<Json::Value> ToJson() const;
};

struct VideoConstraints {
  double max_pixels_per_second;
  Dimensions min_dimensions;
  Dimensions max_dimensions;
  // Technically optional, sender will assume 300kbps if omitted.
  int min_bit_rate;
  int max_bit_rate;
  std::chrono::milliseconds max_delay;

  openscreen::ErrorOr<Json::Value> ToJson() const;
};

struct Constraints {
  AudioConstraints audio;
  VideoConstraints video;

  openscreen::ErrorOr<Json::Value> ToJson() const;
};

// Decides whether the Sender scales and letterboxes content, or if
// it may send video frames of any arbitrary size and the Receiver must
// handle the presentation details.
enum class Scaling : uint8_t { kSender = 0, kReceiver };

struct DisplayDescription {
  // May exceed, be the same, or less than those mentioned in constraints.
  Dimensions dimensions;
  std::string aspect_ratio;
  Scaling scaling;

  openscreen::ErrorOr<Json::Value> ToJson() const;
};

struct Answer {
  int udp_port;
  std::vector<int> send_indexes;
  std::vector<Ssrc> ssrcs;

  Constraints constraints;
  DisplayDescription display;
  std::vector<int> receiver_rtcp_event_log;
  std::vector<int> receiver_rtcp_dscp;
  bool supports_wifi_status_reporting;
  std::vector<std::string> rtp_extensions;

  openscreen::ErrorOr<Json::Value> ToJson() const;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_ANSWER_MESSAGES_H_
