// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtp_receiver/rtp_receiver.h"

#include "base/logging.h"
#include "media/cast/rtp_common/rtp_defines.h"
#include "media/cast/rtp_receiver/receiver_stats.h"
#include "media/cast/rtp_receiver/rtp_parser/rtp_parser.h"

namespace media {
namespace cast {

RtpReceiver::RtpReceiver(const AudioReceiverConfig* audio_config,
                         const VideoReceiverConfig* video_config,
                         RtpData* incoming_payload_callback) {
  DCHECK(incoming_payload_callback) << "Invalid argument";
  DCHECK(audio_config || video_config) << "Invalid argument";
  // Configure parser.
  RtpParserConfig config;
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
  stats_.reset(new ReceiverStats(config.ssrc));
  parser_.reset(new RtpParser(incoming_payload_callback, config));
}

RtpReceiver::~RtpReceiver() {}

bool RtpReceiver::ReceivedPacket(const uint8* packet, int length) {
  RtpCastHeader rtp_header;
  if (!parser_->ParsePacket(packet, length, &rtp_header)) return false;

  stats_->UpdateStatistics(rtp_header);
  return true;
}

void RtpReceiver::GetStatistics(uint8* fraction_lost,
                                uint32* cumulative_lost,
                                uint32* extended_high_sequence_number,
                                uint32* jitter) {
  stats_->GetStatistics(fraction_lost,
                        cumulative_lost,
                        extended_high_sequence_number,
                        jitter);
}

}  // namespace cast
}  // namespace media
