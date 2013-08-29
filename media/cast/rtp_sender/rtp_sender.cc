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
namespace {

// January 1970, in milliseconds.
static const int64 kNtpJan1970 = 2208988800000LL;

// Magic fractional unit.
static const uint32 kMagicFractionalUnit = 4294967;

void ConvertTimeToFractions(int64 time_ms, uint32* seconds,
                            uint32* fractions) {
  *seconds = static_cast<uint32>(time_ms / 1000);
  *fractions = static_cast<uint32>((time_ms % 1000) * kMagicFractionalUnit);
}

void ConvertTimeToNtp(int64 time_ms, uint32* ntp_seconds,
                      uint32* ntp_fractions) {
  ConvertTimeToFractions(time_ms + kNtpJan1970, ntp_seconds, ntp_fractions);
}
}  // namespace

RtpSender::RtpSender(const AudioSenderConfig* audio_config,
                     const VideoSenderConfig* video_config,
                     PacedPacketSender* transport)
    : config_(),
      transport_(transport),
      default_tick_clock_(new base::DefaultTickClock()),
      clock_(default_tick_clock_.get()) {
  // Store generic cast config and create packetizer config.
  DCHECK(audio_config || video_config) << "Invalid argument";
  if (audio_config) {
    storage_.reset(new PacketStorage(audio_config->rtp_history_ms));
    config_.audio = true;
    config_.ssrc = audio_config->sender_ssrc;
    config_.payload_type = audio_config->rtp_payload_type;
    config_.frequency = audio_config->frequency;
    config_.audio_codec = audio_config->codec;
  } else {
    storage_.reset(new PacketStorage(video_config->rtp_history_ms));
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

void RtpSender::IncomingEncodedVideoFrame(const EncodedVideoFrame& video_frame,
                                          int64 capture_time) {
  packetizer_->IncomingEncodedVideoFrame(video_frame, capture_time);
}

void RtpSender::IncomingEncodedAudioFrame(const EncodedAudioFrame& audio_frame,
                                          int64 recorded_time) {
  packetizer_->IncomingEncodedAudioFrame(audio_frame, recorded_time);
}

void RtpSender::ResendPackets(
    const MissingFramesAndPackets& missing_frames_and_packets) {
  std::vector<uint8> packet;
  // Iterate over all frames in the list.
  for (std::map<uint8, std::set<uint16> >::const_iterator it =
       missing_frames_and_packets.begin();
       it != missing_frames_and_packets.end(); ++it) {
    uint8 frame_id = it->first;
    // Iterate over all of the packets in the frame.
    const std::set<uint16>& packets = it->second;
    if (packets.empty()) {
      VLOG(1) << "Missing all packets in frame " << static_cast<int>(frame_id);

      bool success = false;
      uint16 packet_id = 0;
      do {
        // Get packet from storage.
        packet.clear();
        success = storage_->GetPacket(frame_id, packet_id, &packet);

        // Resend packet to the network.
        if (success) {
          VLOG(1) << "Resend " << static_cast<int>(frame_id) << ":"
              << packet_id << " size: " << packets.size();
          // Set a unique incremental sequence number for every packet.
          UpdateSequenceNumber(&packet);
          // Set the size as correspond to each frame.
          transport_->ResendPacket(packet, packets.size());
          ++packet_id;
        }
      } while (success);


    } else {
      for (std::set<uint16>::const_iterator set_it = packets.begin();
          set_it != packets.end(); ++set_it) {
        uint16 packet_id = *set_it;
        // Get packet from storage.
        packet.clear();
        bool success = storage_->GetPacket(frame_id, packet_id, &packet);
        // Resend packet to the network.
        if (success) {
          VLOG(1) << "Resend " << static_cast<int>(frame_id) << ":"
              << packet_id << " size: " << packet.size();
          UpdateSequenceNumber(&packet);
          // Set the size as correspond to each frame.
          transport_->ResendPacket(packet, packets.size());
        } else {
          VLOG(1) << "Failed to resend " << static_cast<int>(frame_id) << ":"
              << packet_id;
        }
      }
    }
  }
}

void RtpSender::UpdateSequenceNumber(std::vector<uint8>* packet) {
  uint16 new_sequence_number = packetizer_->NextSequenceNumber();
  int index = 2;
  (*packet)[index] = (static_cast<uint8>(new_sequence_number));
  (*packet)[index + 1] =(static_cast<uint8>(new_sequence_number >> 8));
}

void RtpSender::RtpStatistics(int64 now_ms, RtcpSenderInfo* sender_info) {
  // The timestamp of this Rtcp packet should be estimated as the timestamp of
  // the frame being captured at this moment. We are calculating that
  // timestamp as the last frame's timestamp + the time since the last frame
  // was captured.
  uint32 ntp_seconds = 0;
  uint32 ntp_fraction = 0;
  ConvertTimeToNtp(now_ms, &ntp_seconds, &ntp_fraction);
  // sender_info->ntp_seconds = ntp_seconds;
  sender_info->ntp_fraction = ntp_fraction;

  int64 time_sent_ms;
  uint32 rtp_timestamp;
  if (packetizer_->LastSentTimestamp(&time_sent_ms, &rtp_timestamp)) {
    int64 time_since_last_send_ms = now_ms - time_sent_ms;
    sender_info->rtp_timestamp = rtp_timestamp +
        time_since_last_send_ms * (config_.frequency / 1000);
  } else {
    sender_info->rtp_timestamp = 0;
  }
  sender_info->send_packet_count = packetizer_->send_packets_count();
  sender_info->send_octet_count = packetizer_->send_octet_count();
}

}  //  namespace cast
}  // namespace media
