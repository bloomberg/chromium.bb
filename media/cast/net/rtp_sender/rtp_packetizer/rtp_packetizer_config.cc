// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp_sender/rtp_packetizer/rtp_packetizer_config.h"

namespace media {
namespace cast {

RtpPacketizerConfig::RtpPacketizerConfig()
    : ssrc(0),
      max_payload_length(kIpPacketSize - 28),  // Default is IP-v4/UDP.
      audio(false),
      frequency(8000),
      payload_type(-1),
      sequence_number(0),
      rtp_timestamp(0) {
}

}  // namespace cast
}  // namespace media
