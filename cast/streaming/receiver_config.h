// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RECEIVER_CONFIG_H_
#define CAST_STREAMING_RECEIVER_CONFIG_H_

#include <chrono>  // NOLINT

#include "cast/streaming/session_config.h"

namespace cast {
namespace streaming {

// The configuration used by the Receiver. Most of the values are shared
// in the underlying SessionConfig, however some settings must be configured
// specifically for the receiver.
struct ReceiverConfig : public SessionConfig {
  // The total amount of time between a frame capture and its playback on
  // the receiver.
  std::chrono::microseconds target_playout_delay{};

  // The target frame rate, in frames per second.
  double target_frame_rate = 0;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_RECEIVER_CONFIG_H_
