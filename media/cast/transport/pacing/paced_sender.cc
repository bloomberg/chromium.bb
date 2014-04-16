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
static const size_t kTargetBurstSize = 10;
static const size_t kMaxBurstSize = 20;

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
      max_burst_size_(kTargetBurstSize),
      next_max_burst_size_(kTargetBurstSize),
      next_next_max_burst_size_(kTargetBurstSize),
      current_burst_size_(0),
      state_(State_Unblocked),
      weak_factory_(this) {
}

PacedSender::~PacedSender() {}

void PacedSender::RegisterAudioSsrc(uint32 audio_ssrc) {
  audio_ssrc_ = audio_ssrc;
}

void PacedSender::RegisterVideoSsrc(uint32 video_ssrc) {
  video_ssrc_ = video_ssrc;
}

bool PacedSender::SendPackets(const PacketList& packets) {
  if (packets.empty()) {
    return true;
  }
  packet_list_.insert(packet_list_.end(), packets.begin(), packets.end());
  if (state_ == State_Unblocked) {
    SendStoredPackets();
  }
  return true;
}

bool PacedSender::ResendPackets(const PacketList& packets) {
  if (packets.empty()) {
    return true;
  }
  resend_packet_list_.insert(resend_packet_list_.end(),
                             packets.begin(),
                             packets.end());
  if (state_ == State_Unblocked) {
    SendStoredPackets();
  }
  return true;
}

bool PacedSender::SendRtcpPacket(PacketRef packet) {
  if (state_ == State_TransportBlocked) {

    rtcp_packet_list_.push_back(packet);
  } else {
    // We pass the RTCP packets straight through.
    if (!transport_->SendPacket(
            packet,
            base::Bind(&PacedSender::SendStoredPackets,
                       weak_factory_.GetWeakPtr()))) {
      state_ = State_TransportBlocked;
    }

  }
  return true;
}

PacketRef PacedSender::GetNextPacket(PacketType* packet_type) {
  PacketRef ret;
  if (!rtcp_packet_list_.empty()) {
    ret = rtcp_packet_list_.front();
    rtcp_packet_list_.pop_front();
    *packet_type = PacketType_RTCP;
  } else if (!resend_packet_list_.empty()) {
    ret = resend_packet_list_.front();
    resend_packet_list_.pop_front();
    *packet_type = PacketType_Resend;
  } else if (!packet_list_.empty()) {
    ret = packet_list_.front();
    packet_list_.pop_front();
    *packet_type = PacketType_Normal;
  } else {
    NOTREACHED();
  }
  return ret;
}

bool PacedSender::empty() const {
  return rtcp_packet_list_.empty() &&
      resend_packet_list_.empty() &&
      packet_list_.empty();
}

size_t PacedSender::size() const {
  return rtcp_packet_list_.size() +
      resend_packet_list_.size() +
      packet_list_.size();
}

// This function can be called from three places:
// 1. User called one of the Send* functions and we were in an unblocked state.
// 2. state_ == State_TransportBlocked and the transport is calling us to
//    let us know that it's ok to send again.
// 3. state_ == State_BurstFull and there are still packets to send. In this
//    case we called PostDelayedTask on this function to start a new burst.
void PacedSender::SendStoredPackets() {
  State previous_state = state_;
  state_ = State_Unblocked;
  if (empty()) {
    return;
  }

  base::TimeTicks now = clock_->NowTicks();
  // I don't actually trust that PostDelayTask(x - now) will mean that
  // now >= x when the call happens, so check if the previous state was
  // State_BurstFull too.
  if (now >= burst_end_ || previous_state == State_BurstFull) {
    // Start a new burst.
    current_burst_size_ = 0;
    burst_end_ = now + base::TimeDelta::FromMilliseconds(kPacingIntervalMs);

    // The goal here is to try to send out the queued packets over the next
    // three bursts, while trying to keep the burst size below 10 if possible.
    // We have some evidence that sending more than 12 packets in a row doesn't
    // work very well, but we don't actually know why yet. Sending out packets
    // sooner is better than sending out packets later as that gives us more
    // time to re-send them if needed. So if we have less than 30 packets, just
    // send 10 at a time. If we have less than 60 packets, send n / 3 at a time.
    // if we have more than 60, we send 20 at a time. 20 packets is ~24Mbit/s
    // which is more bandwidth than the cast library should need, and sending
    // out more data per second is unlikely to be helpful.
    size_t max_burst_size = std::min(
        kMaxBurstSize,
        std::max(kTargetBurstSize, size() / kPacingMaxBurstsPerFrame));

    // If the queue is long, issue a warning. Try to limit the number of
    // warnings issued by only issuing the warning when the burst size
    // grows. Otherwise we might get 100 warnings per second.
    if (max_burst_size > next_next_max_burst_size_ && size() > 100) {
      LOG(WARNING) << "Packet queue is very long:" << size();
    }

    max_burst_size_ = std::max(next_max_burst_size_, max_burst_size);
    next_max_burst_size_ = std::max(next_next_max_burst_size_, max_burst_size);
    next_next_max_burst_size_ = max_burst_size;
  }

  base::Closure cb = base::Bind(&PacedSender::SendStoredPackets,
                                weak_factory_.GetWeakPtr());
  while (!empty()) {
    if (current_burst_size_ >= max_burst_size_) {
      transport_task_runner_->PostDelayedTask(FROM_HERE,
                                              cb,
                                              burst_end_ - now);
      state_ = State_BurstFull;
      return;
    }
    PacketType packet_type;
    PacketRef packet = GetNextPacket(&packet_type);

    switch (packet_type) {
      case PacketType_Resend:
        LogPacketEvent(packet->data, true);
        break;
      case PacketType_Normal:
        LogPacketEvent(packet->data, false);
        break;
      case PacketType_RTCP:
        break;
    }
    if (!transport_->SendPacket(packet, cb)) {
      state_ = State_TransportBlocked;
      return;
    }
    current_burst_size_++;
  }
  state_ = State_Unblocked;
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
