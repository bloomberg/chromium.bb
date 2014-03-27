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

namespace {

static RtpParserConfig GetRtpParserConfig(
    const AudioReceiverConfig* audio_config,
    const VideoReceiverConfig* video_config) {
  RtpParserConfig config;
  DCHECK(audio_config || video_config) << "Invalid argument";
  if (audio_config) {
    config.ssrc = audio_config->incoming_ssrc;
    config.payload_type = audio_config->rtp_payload_type;
    config.audio_codec = audio_config->codec;
    config.audio_channels = audio_config->channels;
  } else {
    config.ssrc = video_config->incoming_ssrc;
    config.payload_type = video_config->rtp_payload_type;
    config.video_codec = video_config->codec;
  }
  return config;
}

}  // namespace

RtpReceiver::RtpReceiver(base::TickClock* clock,
                         const AudioReceiverConfig* audio_config,
                         const VideoReceiverConfig* video_config) :
    RtpParser(GetRtpParserConfig(audio_config, video_config)),
    stats_(clock) {
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
  if (!ParsePacket(packet, length, &rtp_header))
    return false;

  stats_.UpdateStatistics(rtp_header);
  return true;
}

}  // namespace cast
}  // namespace media
