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

RtpSender::RtpSender(
    base::TickClock* clock,
    const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner,
    PacedSender* const transport)
    : clock_(clock),
      transport_(transport),
      transport_task_runner_(transport_task_runner),
      weak_factory_(this) {
  // Randomly set sequence number start value.
  config_.sequence_number = base::RandInt(0, 65535);
}

RtpSender::~RtpSender() {}

bool RtpSender::InitializeAudio(const CastTransportAudioConfig& config) {
  storage_.reset(new PacketStorage(config.rtp.max_outstanding_frames));
  if (!storage_->IsValid()) {
    return false;
  }
  config_.audio = true;
  config_.ssrc = config.rtp.config.ssrc;
  config_.payload_type = config.rtp.config.payload_type;
  config_.frequency = config.frequency;
  config_.audio_codec = config.codec;
  packetizer_.reset(new RtpPacketizer(transport_, storage_.get(), config_));
  return true;
}

bool RtpSender::InitializeVideo(const CastTransportVideoConfig& config) {
  storage_.reset(new PacketStorage(config.rtp.max_outstanding_frames));
  if (!storage_->IsValid()) {
    return false;
  }
  config_.audio = false;
  config_.ssrc = config.rtp.config.ssrc;
  config_.payload_type = config.rtp.config.payload_type;
  config_.frequency = kVideoFrequency;
  config_.video_codec = config.codec;
  packetizer_.reset(new RtpPacketizer(transport_, storage_.get(), config_));
  return true;
}

void RtpSender::SendFrame(const EncodedFrame& frame) {
  DCHECK(packetizer_);
  packetizer_->SendFrameAsPackets(frame);
}

void RtpSender::ResendPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets) {
  DCHECK(storage_);
  // Iterate over all frames in the list.
  for (MissingFramesAndPacketsMap::const_iterator it =
           missing_frames_and_packets.begin();
       it != missing_frames_and_packets.end();
       ++it) {
    SendPacketVector packets_to_resend;
    uint8 frame_id = it->first;
    // Set of packets that the receiver wants us to re-send.
    // If empty, we need to re-send all packets for this frame.
    const PacketIdSet& missing_packet_set = it->second;
    bool success = false;

    for (uint16 packet_id = 0; ; packet_id++) {
      // Get packet from storage.
      success = storage_->GetPacket(frame_id, packet_id, &packets_to_resend);

      // Check that we got at least one packet.
      DCHECK(packet_id != 0 || success)
          << "Failed to resend frame " << static_cast<int>(frame_id);

      if (!success) break;

      if (!missing_packet_set.empty() &&
          missing_packet_set.find(packet_id) == missing_packet_set.end()) {
        transport_->CancelSendingPacket(packets_to_resend.back().first);
        packets_to_resend.pop_back();
      } else {
        // Resend packet to the network.
        VLOG(3) << "Resend " << static_cast<int>(frame_id) << ":"
                << packet_id;
        // Set a unique incremental sequence number for every packet.
        PacketRef packet = packets_to_resend.back().second;
        UpdateSequenceNumber(&packet->data);
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

}  // namespace transport
}  //  namespace cast
}  // namespace media
