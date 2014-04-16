// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/rtp_sender/rtp_sender.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/pacing/paced_sender.h"

namespace media {
namespace cast {
namespace transport {

// Schedule the RTP statistics callback every 33mS. As this interval affects the
// time offset of the render and playout times, we want it in the same ball park
// as the frame rate.
static const int kStatsCallbackIntervalMs = 33;

RtpSender::RtpSender(
    base::TickClock* clock,
    LoggingImpl* logging,
    const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner,
    PacedSender* const transport)
    : clock_(clock),
      logging_(logging),
      transport_(transport),
      stats_callback_(),
      transport_task_runner_(transport_task_runner),
      weak_factory_(this) {
  // Randomly set sequence number start value.
  config_.sequence_number = base::RandInt(0, 65535);
}

RtpSender::~RtpSender() {}

void RtpSender::InitializeAudio(const CastTransportAudioConfig& config) {
  storage_.reset(new PacketStorage(clock_, config.base.rtp_config.history_ms));
  config_.audio = true;
  config_.ssrc = config.base.ssrc;
  config_.payload_type = config.base.rtp_config.payload_type;
  config_.frequency = config.frequency;
  config_.audio_codec = config.codec;
  packetizer_.reset(
      new RtpPacketizer(transport_, storage_.get(), config_, clock_, logging_));
}

void RtpSender::InitializeVideo(const CastTransportVideoConfig& config) {
  storage_.reset(new PacketStorage(clock_, config.base.rtp_config.history_ms));
  config_.audio = false;
  config_.ssrc = config.base.ssrc;
  config_.payload_type = config.base.rtp_config.payload_type;
  config_.frequency = kVideoFrequency;
  config_.video_codec = config.codec;
  packetizer_.reset(
      new RtpPacketizer(transport_, storage_.get(), config_, clock_, logging_));
}

void RtpSender::IncomingEncodedVideoFrame(const EncodedVideoFrame* video_frame,
                                          const base::TimeTicks& capture_time) {
  DCHECK(packetizer_);
  packetizer_->IncomingEncodedVideoFrame(video_frame, capture_time);
}

void RtpSender::IncomingEncodedAudioFrame(
    const EncodedAudioFrame* audio_frame,
    const base::TimeTicks& recorded_time) {
  DCHECK(packetizer_);
  packetizer_->IncomingEncodedAudioFrame(audio_frame, recorded_time);
}

void RtpSender::ResendPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets) {
  DCHECK(storage_);
  // Iterate over all frames in the list.
  for (MissingFramesAndPacketsMap::const_iterator it =
           missing_frames_and_packets.begin();
       it != missing_frames_and_packets.end();
       ++it) {
    PacketList packets_to_resend;
    uint8 frame_id = it->first;
    const PacketIdSet& packets_set = it->second;
    bool success = false;

    if (packets_set.empty()) {
      VLOG(3) << "Missing all packets in frame " << static_cast<int>(frame_id);

      uint16 packet_id = 0;
      do {
        // Get packet from storage.
        success = storage_->GetPacket(frame_id, packet_id, &packets_to_resend);

        // Resend packet to the network.
        if (success) {
          VLOG(3) << "Resend " << static_cast<int>(frame_id) << ":"
                  << packet_id;
          // Set a unique incremental sequence number for every packet.
          PacketRef packet = packets_to_resend.back();
          UpdateSequenceNumber(&packet->data);
          // Set the size as correspond to each frame.
          ++packet_id;
        }
      } while (success);
    } else {
      // Iterate over all of the packets in the frame.
      for (PacketIdSet::const_iterator set_it = packets_set.begin();
           set_it != packets_set.end();
           ++set_it) {
        uint16 packet_id = *set_it;
        success = storage_->GetPacket(frame_id, packet_id, &packets_to_resend);

        // Resend packet to the network.
        if (success) {
          VLOG(3) << "Resend " << static_cast<int>(frame_id) << ":"
                  << packet_id;
          PacketRef packet = packets_to_resend.back();
          UpdateSequenceNumber(&packet->data);
        }
      }
    }
    transport_->ResendPackets(packets_to_resend);
  }
}

void RtpSender::UpdateSequenceNumber(Packet* packet) {
  uint16 new_sequence_number = packetizer_->NextSequenceNumber();
  int index = 2;
  (*packet)[index] = (static_cast<uint8>(new_sequence_number));
  (*packet)[index + 1] = (static_cast<uint8>(new_sequence_number >> 8));
}

void RtpSender::SubscribeRtpStatsCallback(
    const CastTransportRtpStatistics& callback) {
  stats_callback_ = callback;
  ScheduleNextStatsReport();
}

void RtpSender::ScheduleNextStatsReport() {
  transport_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&RtpSender::RtpStatistics, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kStatsCallbackIntervalMs));
}

void RtpSender::RtpStatistics() {
  RtcpSenderInfo sender_info;
  base::TimeTicks time_sent;
  uint32 rtp_timestamp = 0;
  packetizer_->LastSentTimestamp(&time_sent, &rtp_timestamp);
  sender_info.send_packet_count = packetizer_->send_packets_count();
  sender_info.send_octet_count = packetizer_->send_octet_count();
  stats_callback_.Run(sender_info, time_sent, rtp_timestamp);
  ScheduleNextStatsReport();
}

}  // namespace transport
}  //  namespace cast
}  // namespace media
