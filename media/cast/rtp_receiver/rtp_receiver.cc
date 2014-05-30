// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtp_receiver/rtp_receiver.h"

#include "base/big_endian.h"
#include "base/logging.h"
#include "media/cast/rtp_receiver/receiver_stats.h"
#include "media/cast/rtp_receiver/rtp_parser/rtp_parser.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"

namespace media {
namespace cast {

RtpReceiver::RtpReceiver(base::TickClock* clock,
                         const FrameReceiverConfig* audio_config,
                         const FrameReceiverConfig* video_config) :
    packet_parser_(audio_config ? audio_config->incoming_ssrc :
                   (video_config ? video_config->incoming_ssrc : 0),
                   audio_config ? audio_config->rtp_payload_type :
                   (video_config ? video_config->rtp_payload_type : 0)),
    stats_(clock) {
  DCHECK(audio_config || video_config);
}

RtpReceiver::~RtpReceiver() {}

// static
uint32 RtpReceiver::GetSsrcOfSender(const uint8* rtcp_buffer, size_t length) {
  DCHECK_GE(length, kMinLengthOfRtp) << "Invalid RTP packet";
  uint32 ssrc_of_sender;
  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_buffer), length);
  big_endian_reader.Skip(8);  // Skip header
  big_endian_reader.ReadU32(&ssrc_of_sender);
  return ssrc_of_sender;
}

bool RtpReceiver::ReceivedPacket(const uint8* packet, size_t length) {
  RtpCastHeader rtp_header;
  const uint8* payload_data;
  size_t payload_size;
  if (!packet_parser_.ParsePacket(
          packet, length, &rtp_header, &payload_data, &payload_size)) {
    return false;
  }

  OnReceivedPayloadData(payload_data, payload_size, rtp_header);
  stats_.UpdateStatistics(rtp_header);
  return true;
}

}  // namespace cast
}  // namespace media
