// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtp_sender/rtp_sender.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "media/cast/cast_defines.h"
#include "media/cast/pacing/paced_sender.h"
#include "media/cast/rtcp/rtcp_defines.h"

namespace media {
namespace cast {

RtpSender::RtpSender(base::TickClock* clock,
                     const AudioSenderConfig* audio_config,
                     const VideoSenderConfig* video_config,
                     PacedPacketSender* transport)
    : config_(),
      transport_(transport),
      clock_(clock) {
  // Store generic cast config and create packetizer config.
  DCHECK(audio_config || video_config) << "Invalid argument";
  if (audio_config) {
    storage_.reset(new PacketStorage(clock, audio_config->rtp_history_ms));
    config_.audio = true;
    config_.ssrc = audio_config->sender_ssrc;
    config_.payload_type = audio_config->rtp_payload_type;
    config_.frequency = audio_config->frequency;
    config_.audio_codec = audio_config->codec;
  } else {
    storage_.reset(new PacketStorage(clock, video_config->rtp_history_ms));
    config_.audio = false;
    config_.ssrc = video_config->sender_ssrc;
    config_.payload_type = video_config->rtp_payload_type;
    config_.frequency = kVideoFrequency;
    config_.video_codec = video_config->codec;
  }
  // Randomly set start values.
  config_.sequence_number = base::RandInt(0, 65535);
  config_.rtp_timestamp = base::RandInt(0, 65535);
  config_.rtp_timestamp += base::RandInt(0, 65535) << 16;
  packetizer_.reset(new RtpPacketizer(transport, storage_.get(), config_));
}

RtpSender::~RtpSender() {}

void RtpSender::IncomingEncodedVideoFrame(const EncodedVideoFrame* video_frame,
    const base::TimeTicks& capture_time) {
  packetizer_->IncomingEncodedVideoFrame(video_frame, capture_time);
}

void RtpSender::IncomingEncodedAudioFrame(const EncodedAudioFrame* audio_frame,
    const base::TimeTicks& recorded_time) {
  packetizer_->IncomingEncodedAudioFrame(audio_frame, recorded_time);
}

void RtpSender::ResendPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets) {
  PacketList packets_to_resend =
      storage_->GetPackets(missing_frames_and_packets);

  PacketList::iterator it = packets_to_resend.begin();
  for (; it != packets_to_resend.end(); ++it) {
    Packet& packet = *it;
    UpdateSequenceNumber(&packet);
  }
  transport_->ResendPackets(packets_to_resend);
}

void RtpSender::UpdateSequenceNumber(Packet* packet) {
  uint16 new_sequence_number = packetizer_->NextSequenceNumber();
  int index = 2;
  (*packet)[index] = (static_cast<uint8>(new_sequence_number));
  (*packet)[index + 1] =(static_cast<uint8>(new_sequence_number >> 8));
}

void RtpSender::RtpStatistics(const base::TimeTicks& now,
                              RtcpSenderInfo* sender_info) {
  // The timestamp of this Rtcp packet should be estimated as the timestamp of
  // the frame being captured at this moment. We are calculating that
  // timestamp as the last frame's timestamp + the time since the last frame
  // was captured.
  uint32 ntp_seconds = 0;
  uint32 ntp_fraction = 0;
  ConvertTimeTicksToNtp(now, &ntp_seconds, &ntp_fraction);
  sender_info->ntp_seconds = ntp_seconds;
  sender_info->ntp_fraction = ntp_fraction;

  base::TimeTicks time_sent;
  uint32 rtp_timestamp;
  if (packetizer_->LastSentTimestamp(&time_sent, &rtp_timestamp)) {
    base::TimeDelta time_since_last_send = now - time_sent;
    sender_info->rtp_timestamp = rtp_timestamp +
        time_since_last_send.InMilliseconds() * (config_.frequency / 1000);
  } else {
    sender_info->rtp_timestamp = 0;
  }
  sender_info->send_packet_count = packetizer_->send_packets_count();
  sender_info->send_octet_count = packetizer_->send_octet_count();
}

}  //  namespace cast
}  // namespace media
