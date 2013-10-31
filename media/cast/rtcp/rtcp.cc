// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtcp/rtcp.h"

#include "base/debug/trace_event.h"
#include "base/rand_util.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "media/cast/rtcp/rtcp_receiver.h"
#include "media/cast/rtcp/rtcp_sender.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "net/base/big_endian.h"

namespace media {
namespace cast {

static const int kMaxRttMs = 1000000;  // 1000 seconds.

// Time limit for received RTCP messages when we stop using it for lip-sync.
static const int64 kMaxDiffSinceReceivedRtcpMs = 100000;  // 100 seconds.

class LocalRtcpRttFeedback : public RtcpRttFeedback {
 public:
  explicit LocalRtcpRttFeedback(Rtcp* rtcp)
      : rtcp_(rtcp) {
  }

  virtual void OnReceivedDelaySinceLastReport(
      uint32 receivers_ssrc,
      uint32 last_report,
      uint32 delay_since_last_report) OVERRIDE {
    rtcp_->OnReceivedDelaySinceLastReport(receivers_ssrc,
                                          last_report,
                                          delay_since_last_report);
  }

 private:
  Rtcp* rtcp_;
};

RtcpCastMessage::RtcpCastMessage(uint32 media_ssrc)
    : media_ssrc_(media_ssrc) {}

RtcpCastMessage::~RtcpCastMessage() {}

RtcpNackMessage::RtcpNackMessage() {}
RtcpNackMessage::~RtcpNackMessage() {}

RtcpRembMessage::RtcpRembMessage() {}
RtcpRembMessage::~RtcpRembMessage() {}


class LocalRtcpReceiverFeedback : public RtcpReceiverFeedback {
 public:
  explicit LocalRtcpReceiverFeedback(Rtcp* rtcp)
      : rtcp_(rtcp) {
  }

  virtual void OnReceivedSenderReport(
      const RtcpSenderInfo& remote_sender_info) OVERRIDE {
    rtcp_->OnReceivedNtp(remote_sender_info.ntp_seconds,
                         remote_sender_info.ntp_fraction);
    if (remote_sender_info.send_packet_count != 0) {
      rtcp_->OnReceivedLipSyncInfo(remote_sender_info.rtp_timestamp,
                                   remote_sender_info.ntp_seconds,
                                   remote_sender_info.ntp_fraction);
    }
  }

  virtual void OnReceiverReferenceTimeReport(
      const RtcpReceiverReferenceTimeReport& remote_time_report) OVERRIDE {
    rtcp_->OnReceivedNtp(remote_time_report.ntp_seconds,
                         remote_time_report.ntp_fraction);
  }

  virtual void OnReceivedSendReportRequest() OVERRIDE {
    rtcp_->OnReceivedSendReportRequest();
  }

 private:
  Rtcp* rtcp_;
};

Rtcp::Rtcp(base::TickClock* clock,
           RtcpSenderFeedback* sender_feedback,
           PacedPacketSender* paced_packet_sender,
           RtpSenderStatistics* rtp_sender_statistics,
           RtpReceiverStatistics* rtp_receiver_statistics,
           RtcpMode rtcp_mode,
           const base::TimeDelta& rtcp_interval,
           bool sending_media,
           uint32 local_ssrc,
           const std::string& c_name)
    : rtcp_interval_(rtcp_interval),
      rtcp_mode_(rtcp_mode),
      sending_media_(sending_media),
      local_ssrc_(local_ssrc),
      rtp_sender_statistics_(rtp_sender_statistics),
      rtp_receiver_statistics_(rtp_receiver_statistics),
      receiver_feedback_(new LocalRtcpReceiverFeedback(this)),
      rtt_feedback_(new LocalRtcpRttFeedback(this)),
      rtcp_sender_(new RtcpSender(paced_packet_sender, local_ssrc, c_name)),
      last_report_sent_(0),
      last_report_received_(0),
      last_received_rtp_timestamp_(0),
      last_received_ntp_seconds_(0),
      last_received_ntp_fraction_(0),
      min_rtt_(base::TimeDelta::FromMilliseconds(kMaxRttMs)),
      number_of_rtt_in_avg_(0),
      clock_(clock) {
  rtcp_receiver_.reset(new RtcpReceiver(sender_feedback,
                                        receiver_feedback_.get(),
                                        rtt_feedback_.get(),
                                        local_ssrc));
}

Rtcp::~Rtcp() {}

// static
bool Rtcp::IsRtcpPacket(const uint8* packet, size_t length) {
  DCHECK_GE(length, kMinLengthOfRtcp) << "Invalid RTCP packet";
  if (length < kMinLengthOfRtcp) return false;

  uint8 packet_type = packet[1];
  if (packet_type >= kPacketTypeLow && packet_type <= kPacketTypeHigh) {
    return true;
  }
  return false;
}

// static
uint32 Rtcp::GetSsrcOfSender(const uint8* rtcp_buffer, size_t length) {
  DCHECK_GE(length, kMinLengthOfRtcp) << "Invalid RTCP packet";
  uint32 ssrc_of_sender;
  net::BigEndianReader big_endian_reader(rtcp_buffer, length);
  big_endian_reader.Skip(4);  // Skip header
  big_endian_reader.ReadU32(&ssrc_of_sender);
  return ssrc_of_sender;
}

base::TimeTicks Rtcp::TimeToSendNextRtcpReport() {
  if (next_time_to_send_rtcp_.is_null()) {
    UpdateNextTimeToSendRtcp();
  }
  return next_time_to_send_rtcp_;
}

void Rtcp::SetRemoteSSRC(uint32 ssrc) {
  rtcp_receiver_->SetRemoteSSRC(ssrc);
}

void Rtcp::IncomingRtcpPacket(const uint8* rtcp_buffer, size_t length) {
  RtcpParser rtcp_parser(rtcp_buffer, length);
  if (!rtcp_parser.IsValid()) {
    // Silently ignore packet.
    DLOG(ERROR) << "Received invalid RTCP packet";
    return;
  }
  rtcp_receiver_->IncomingRtcpPacket(&rtcp_parser);
}

void Rtcp::SendRtcpCast(const RtcpCastMessage& cast_message) {
  uint32 packet_type_flags = 0;
  base::TimeTicks now = clock_->NowTicks();

  if (rtcp_mode_ == kRtcpCompound || now >= next_time_to_send_rtcp_) {
    if (sending_media_) {
      packet_type_flags = RtcpSender::kRtcpSr;
    } else {
      packet_type_flags = RtcpSender::kRtcpRr;
    }
  }
  packet_type_flags |= RtcpSender::kRtcpCast;

  SendRtcp(now, packet_type_flags, 0, &cast_message);
}

void Rtcp::SendRtcpPli(uint32 pli_remote_ssrc) {
  uint32 packet_type_flags = 0;
  base::TimeTicks now = clock_->NowTicks();

  if (rtcp_mode_ == kRtcpCompound || now >= next_time_to_send_rtcp_) {
    if (sending_media_) {
      packet_type_flags = RtcpSender::kRtcpSr;
    } else {
      packet_type_flags = RtcpSender::kRtcpRr;
    }
  }
  packet_type_flags |= RtcpSender::kRtcpPli;
  SendRtcp(now, packet_type_flags, pli_remote_ssrc, NULL);
}

void Rtcp::SendRtcpReport(uint32 media_ssrc) {
  uint32 packet_type_flags;
  base::TimeTicks now = clock_->NowTicks();
  if (sending_media_) {
    packet_type_flags = RtcpSender::kRtcpSr;
  } else {
    packet_type_flags = RtcpSender::kRtcpRr;
  }
  SendRtcp(now, packet_type_flags, media_ssrc, NULL);
}

void Rtcp::SendRtcp(const base::TimeTicks& now,
                    uint32 packet_type_flags,
                    uint32 media_ssrc,
                    const RtcpCastMessage* cast_message) {
  if (packet_type_flags & RtcpSender::kRtcpSr ||
      packet_type_flags & RtcpSender::kRtcpRr) {
    UpdateNextTimeToSendRtcp();
  }
  if (packet_type_flags & RtcpSender::kRtcpSr) {
    RtcpSenderInfo sender_info;

    if (rtp_sender_statistics_) {
      rtp_sender_statistics_->GetStatistics(now, &sender_info);
    } else {
      memset(&sender_info, 0, sizeof(sender_info));
    }
    time_last_report_sent_ = now;
    last_report_sent_ = (sender_info.ntp_seconds << 16) +
                        (sender_info.ntp_fraction >> 16);

    RtcpDlrrReportBlock dlrr;
    if (!time_last_report_received_.is_null()) {
      packet_type_flags |= RtcpSender::kRtcpDlrr;
      dlrr.last_rr = last_report_received_;
      uint32 delay_seconds = 0;
      uint32 delay_fraction = 0;
      base::TimeDelta delta = now - time_last_report_received_;
      ConvertTimeToFractions(delta.InMicroseconds(),
                             &delay_seconds,
                             &delay_fraction);

      dlrr.delay_since_last_rr =
         ConvertToNtpDiff(delay_seconds, delay_fraction);
    }
    rtcp_sender_->SendRtcp(packet_type_flags,
                           &sender_info,
                           NULL,
                           media_ssrc,
                           &dlrr,
                           NULL,
                           NULL);
  } else {
    RtcpReportBlock report_block;
    report_block.remote_ssrc = 0;  // Not needed to set send side.
    report_block.media_ssrc = media_ssrc;  // SSRC of the RTP packet sender.
    if (rtp_receiver_statistics_) {
      rtp_receiver_statistics_->GetStatistics(
          &report_block.fraction_lost,
          &report_block.cumulative_lost,
          &report_block.extended_high_sequence_number,
          &report_block.jitter);
    }

    report_block.last_sr = last_report_received_;
    if (!time_last_report_received_.is_null()) {
      uint32 delay_seconds = 0;
      uint32 delay_fraction = 0;
      base::TimeDelta delta = now - time_last_report_received_;
      ConvertTimeToFractions(delta.InMicroseconds(),
                             &delay_seconds,
                             &delay_fraction);
      report_block.delay_since_last_sr =
          ConvertToNtpDiff(delay_seconds, delay_fraction);
    } else {
      report_block.delay_since_last_sr = 0;
    }

    packet_type_flags |= RtcpSender::kRtcpRrtr;
    RtcpReceiverReferenceTimeReport rrtr;
    ConvertTimeTicksToNtp(now, &rrtr.ntp_seconds, &rrtr.ntp_fraction);

    time_last_report_sent_ = now;
    last_report_sent_ = ConvertToNtpDiff(rrtr.ntp_seconds, rrtr.ntp_fraction);

    rtcp_sender_->SendRtcp(packet_type_flags,
                           NULL,
                           &report_block,
                           media_ssrc,
                           NULL,
                           &rrtr,
                           cast_message);
  }
}

void Rtcp::OnReceivedNtp(uint32 ntp_seconds, uint32 ntp_fraction) {
  last_report_received_ = (ntp_seconds << 16) + (ntp_fraction >> 16);

  base::TimeTicks now = clock_->NowTicks();
  time_last_report_received_ = now;
}

void Rtcp::OnReceivedLipSyncInfo(uint32 rtp_timestamp,
                                 uint32 ntp_seconds,
                                 uint32 ntp_fraction) {
  last_received_rtp_timestamp_ = rtp_timestamp;
  last_received_ntp_seconds_ = ntp_seconds;
  last_received_ntp_fraction_ = ntp_fraction;
}

void Rtcp::OnReceivedSendReportRequest() {
  base::TimeTicks now = clock_->NowTicks();

  // Trigger a new RTCP report at next timer.
  next_time_to_send_rtcp_ = now;
}

bool Rtcp::RtpTimestampInSenderTime(int frequency, uint32 rtp_timestamp,
      base::TimeTicks* rtp_timestamp_in_ticks) const {
  if (last_received_ntp_seconds_ == 0)  return false;

  int wrap = CheckForWrapAround(rtp_timestamp, last_received_rtp_timestamp_);
  int64 rtp_timestamp_int64 = rtp_timestamp;
  int64 last_received_rtp_timestamp_int64 = last_received_rtp_timestamp_;

  if (wrap == 1) {
    rtp_timestamp_int64 += (1LL << 32);
  } else if (wrap == -1) {
    last_received_rtp_timestamp_int64 += (1LL << 32);
  }
  // Time since the last RTCP message.
  // Note that this can be negative since we can compare a rtp timestamp from
  // a frame older than the last received RTCP message.
  int64 rtp_timestamp_diff =
      rtp_timestamp_int64 - last_received_rtp_timestamp_int64;

  int frequency_khz = frequency / 1000;
  int64 rtp_time_diff_ms = rtp_timestamp_diff / frequency_khz;

  // Sanity check.
  if (abs(rtp_time_diff_ms) > kMaxDiffSinceReceivedRtcpMs)  return false;

  *rtp_timestamp_in_ticks = ConvertNtpToTimeTicks(last_received_ntp_seconds_,
      last_received_ntp_fraction_) +
      base::TimeDelta::FromMilliseconds(rtp_time_diff_ms);
  return true;
}

void Rtcp::OnReceivedDelaySinceLastReport(uint32 receivers_ssrc,
                                          uint32 last_report,
                                          uint32 delay_since_last_report) {
  if (last_report_sent_ != last_report)  return;  // Feedback on another report.
  if (time_last_report_sent_.is_null())  return;

  base::TimeDelta sender_delay = clock_->NowTicks() - time_last_report_sent_;
  UpdateRtt(sender_delay, ConvertFromNtpDiff(delay_since_last_report));
}

void Rtcp::UpdateRtt(const base::TimeDelta& sender_delay,
                     const base::TimeDelta& receiver_delay) {
  base::TimeDelta rtt = sender_delay - receiver_delay;
  rtt = std::max(rtt, base::TimeDelta::FromMilliseconds(1));
  rtt_ = rtt;
  min_rtt_ = std::min(min_rtt_, rtt);
  max_rtt_ = std::max(max_rtt_, rtt);

  if (number_of_rtt_in_avg_ != 0) {
    float ac = static_cast<float>(number_of_rtt_in_avg_);
    avg_rtt_ms_= ((ac / (ac + 1.0)) * avg_rtt_ms_) +
        ((1.0 / (ac + 1.0)) * rtt.InMilliseconds());
  } else {
    avg_rtt_ms_ = rtt.InMilliseconds();
  }
  number_of_rtt_in_avg_++;
  TRACE_COUNTER_ID1("cast_rtcp", "RTT", local_ssrc_, rtt.InMilliseconds());
}

bool Rtcp::Rtt(base::TimeDelta* rtt,
               base::TimeDelta* avg_rtt,
               base::TimeDelta* min_rtt,
               base::TimeDelta* max_rtt) const {
  DCHECK(rtt) << "Invalid argument";
  DCHECK(avg_rtt) << "Invalid argument";
  DCHECK(min_rtt) << "Invalid argument";
  DCHECK(max_rtt) << "Invalid argument";

  if (number_of_rtt_in_avg_ == 0)  return false;

  *rtt = rtt_;
  *avg_rtt = base::TimeDelta::FromMilliseconds(avg_rtt_ms_);
  *min_rtt = min_rtt_;
  *max_rtt = max_rtt_;
  return true;
}

int Rtcp::CheckForWrapAround(uint32 new_timestamp,
                             uint32 old_timestamp) const {
  if (new_timestamp < old_timestamp) {
    // This difference should be less than -2^31 if we have had a wrap around
    // (e.g. |new_timestamp| = 1, |rtcp_rtp_timestamp| = 2^32 - 1). Since it is
    // cast to a int32_t, it should be positive.
    if (static_cast<int32>(new_timestamp - old_timestamp) > 0) {
      return 1;  // Forward wrap around.
    }
  } else if (static_cast<int32>(old_timestamp - new_timestamp) > 0) {
    // This difference should be less than -2^31 if we have had a backward wrap
    // around. Since it is cast to a int32, it should be positive.
    return -1;
  }
  return 0;
}

void Rtcp::UpdateNextTimeToSendRtcp() {
  int random = base::RandInt(0, 999);
  base::TimeDelta time_to_next = (rtcp_interval_ / 2) +
      (rtcp_interval_ * random / 1000);

  base::TimeTicks now = clock_->NowTicks();
  next_time_to_send_rtcp_ = now + time_to_next;
}

}  // namespace cast
}  // namespace media
