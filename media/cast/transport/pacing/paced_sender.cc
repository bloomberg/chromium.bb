// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/pacing/paced_sender.h"

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace media {
namespace cast {
namespace transport {

namespace {
static const int64 kPacingIntervalMs = 10;
// Each frame will be split into no more than kPacingMaxBurstsPerFrame
// bursts of packets.
static const size_t kPacingMaxBurstsPerFrame = 3;

using media::cast::CastLoggingEvent;

CastLoggingEvent GetLoggingEvent(bool is_audio, bool retransmit) {
  if (retransmit) {
    return is_audio ? media::cast::kAudioPacketRetransmitted
                    : media::cast::kVideoPacketRetransmitted;
  } else {
    return is_audio ? media::cast::kAudioPacketSentToNetwork
                    : media::cast::kVideoPacketSentToNetwork;
  }
}

}  // namespace

PacedSender::PacedSender(
    base::TickClock* clock,
    LoggingImpl* logging,
    PacketSender* transport,
    const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner)
    : clock_(clock),
      logging_(logging),
      transport_(transport),
      transport_task_runner_(transport_task_runner),
      audio_ssrc_(0),
      video_ssrc_(0),
      burst_size_(1),
      packets_sent_in_burst_(0),
      weak_factory_(this) {
  ScheduleNextSend();
}

PacedSender::~PacedSender() {}

void PacedSender::RegisterAudioSsrc(uint32 audio_ssrc) {
  audio_ssrc_ = audio_ssrc;
}

void PacedSender::RegisterVideoSsrc(uint32 video_ssrc) {
  video_ssrc_ = video_ssrc;
}

bool PacedSender::SendPackets(const PacketList& packets) {
  return SendPacketsToTransport(packets, &packet_list_, false);
}

bool PacedSender::ResendPackets(const PacketList& packets) {
  return SendPacketsToTransport(packets, &resend_packet_list_, true);
}

bool PacedSender::SendPacketsToTransport(const PacketList& packets,
                                         PacketList* packets_not_sent,
                                         bool retransmit) {
  UpdateBurstSize(packets.size());

  if (!packets_not_sent->empty()) {
    packets_not_sent->insert(
        packets_not_sent->end(), packets.begin(), packets.end());
    return true;
  }

  PacketList packets_to_send;
  PacketList::const_iterator first_to_store_it = packets.begin();

  size_t max_packets_to_send_now = burst_size_ - packets_sent_in_burst_;

  if (max_packets_to_send_now > 0) {
    size_t packets_to_send_now =
        std::min(max_packets_to_send_now, packets.size());

    std::advance(first_to_store_it, packets_to_send_now);
    packets_to_send.insert(
        packets_to_send.begin(), packets.begin(), first_to_store_it);
  }
  packets_not_sent->insert(
      packets_not_sent->end(), first_to_store_it, packets.end());
  packets_sent_in_burst_ = packets_to_send.size();
  if (packets_to_send.empty())
    return true;

  for (PacketList::iterator it = packets_to_send.begin();
       it != packets_to_send.end();
       ++it) {
    LogPacketEvent(*it, retransmit);
  }

  return TransmitPackets(packets_to_send);
}

bool PacedSender::SendRtcpPacket(const Packet& packet) {
  // We pass the RTCP packets straight through.
  return transport_->SendPacket(packet);
}

void PacedSender::ScheduleNextSend() {
  base::TimeDelta time_to_next =
      time_last_process_ - clock_->NowTicks() +
      base::TimeDelta::FromMilliseconds(kPacingIntervalMs);

  time_to_next = std::max(time_to_next, base::TimeDelta());

  transport_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PacedSender::SendNextPacketBurst, weak_factory_.GetWeakPtr()),
      time_to_next);
}

void PacedSender::SendNextPacketBurst() {
  SendStoredPackets();
  time_last_process_ = clock_->NowTicks();
  ScheduleNextSend();
}

void PacedSender::SendStoredPackets() {
  if (packet_list_.empty() && resend_packet_list_.empty())
    return;

  size_t packets_to_send = burst_size_;
  PacketList packets_to_resend;

  // Send our re-send packets first.
  if (!resend_packet_list_.empty()) {
    PacketList::iterator it = resend_packet_list_.begin();
    size_t packets_to_send_now =
        std::min(packets_to_send, resend_packet_list_.size());
    std::advance(it, packets_to_send_now);

    for (PacketList::iterator log_it = resend_packet_list_.begin();
         log_it != it;
         ++log_it) {
      LogPacketEvent(*log_it, true);
    }
    packets_to_resend.insert(
        packets_to_resend.begin(), resend_packet_list_.begin(), it);
    resend_packet_list_.erase(resend_packet_list_.begin(), it);
    packets_to_send -= packets_to_resend.size();
  }
  if (!packet_list_.empty() && packets_to_send > 0) {
    PacketList::iterator it = packet_list_.begin();
    size_t packets_to_send_now = std::min(packets_to_send, packet_list_.size());
    std::advance(it, packets_to_send_now);

    for (PacketList::iterator log_it = packet_list_.begin(); log_it != it;
         ++log_it) {
      LogPacketEvent(*log_it, false);
    }

    packets_to_resend.insert(packets_to_resend.end(), packet_list_.begin(), it);
    packet_list_.erase(packet_list_.begin(), it);

    if (packet_list_.empty()) {
      burst_size_ = 1;  // Reset burst size after we sent the last stored packet
      packets_sent_in_burst_ = 0;
    } else {
      packets_sent_in_burst_ = packets_to_resend.size();
    }
  }
  TransmitPackets(packets_to_resend);
}

bool PacedSender::TransmitPackets(const PacketList& packets) {
  bool ret = true;
  for (size_t i = 0; i < packets.size(); i++) {
    ret &= transport_->SendPacket(packets[i]);
  }
  return ret;
}

void PacedSender::UpdateBurstSize(size_t packets_to_send) {
  packets_to_send = std::max(packets_to_send,
                             resend_packet_list_.size() + packet_list_.size());

  packets_to_send += (kPacingMaxBurstsPerFrame - 1);  // Round up.
  burst_size_ =
      std::max(packets_to_send / kPacingMaxBurstsPerFrame, burst_size_);
}

void PacedSender::LogPacketEvent(const Packet& packet, bool retransmit) {
  // Get SSRC from packet and compare with the audio_ssrc / video_ssrc to see
  // if the packet is audio or video.
  DCHECK_GE(packet.size(), 12u);
  base::BigEndianReader reader(reinterpret_cast<const char*>(&packet[8]), 4);
  uint32 ssrc;
  bool success = reader.ReadU32(&ssrc);
  DCHECK(success);
  bool is_audio;
  if (ssrc == audio_ssrc_) {
    is_audio = true;
  } else if (ssrc == video_ssrc_) {
    is_audio = false;
  } else {
    DVLOG(3) << "Got unknown ssrc " << ssrc << " when logging packet event";
    return;
  }

  CastLoggingEvent event = GetLoggingEvent(is_audio, retransmit);

  logging_->InsertSinglePacketEvent(clock_->NowTicks(), event, packet);
}

}  // namespace transport
}  // namespace cast
}  // namespace media
