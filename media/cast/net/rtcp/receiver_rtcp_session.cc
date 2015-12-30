// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/big_endian.h"
#include "base/time/tick_clock.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtcp/receiver_rtcp_session.h"
#include "media/cast/net/rtcp/rtcp_builder.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/net/rtcp/rtcp_utility.h"

namespace media {
namespace cast {

namespace {

// Create a NTP diff from seconds and fractions of seconds; delay_fraction is
// fractions of a second where 0x80000000 is half a second.
uint32_t ConvertToNtpDiff(uint32_t delay_seconds, uint32_t delay_fraction) {
  return ((delay_seconds & 0x0000FFFF) << 16) +
         ((delay_fraction & 0xFFFF0000) >> 16);
}

}  // namespace

ReceiverRtcpSession::ReceiverRtcpSession(base::TickClock* clock,
                                         PacedPacketSender* packet_sender,
                                         uint32_t local_ssrc,
                                         uint32_t remote_ssrc)
    : clock_(clock),
      packet_sender_(packet_sender),
      local_ssrc_(local_ssrc),
      remote_ssrc_(remote_ssrc),
      last_report_truncated_ntp_(0),
      local_clock_ahead_by_(ClockDriftSmoother::GetDefaultTimeConstant()),
      lip_sync_ntp_timestamp_(0),
      parser_(local_ssrc, remote_ssrc) {}

ReceiverRtcpSession::~ReceiverRtcpSession() {}

bool ReceiverRtcpSession::IncomingRtcpPacket(const uint8_t* data,
                                             size_t length) {
  // Check if this is a valid RTCP packet.
  if (!IsRtcpPacket(data, length)) {
    VLOG(1) << "Rtcp@" << this << "::IncomingRtcpPacket() -- "
            << "Received an invalid (non-RTCP?) packet.";
    return false;
  }

  // Check if this packet is to us.
  uint32_t ssrc_of_sender = GetSsrcOfSender(data, length);
  if (ssrc_of_sender != remote_ssrc_) {
    return false;
  }

  // Parse this packet.
  base::BigEndianReader reader(reinterpret_cast<const char*>(data), length);
  if (parser_.Parse(&reader)) {
    if (parser_.has_sender_report()) {
      OnReceivedNtp(parser_.sender_report().ntp_seconds,
                    parser_.sender_report().ntp_fraction);
      OnReceivedLipSyncInfo(parser_.sender_report().rtp_timestamp,
                            parser_.sender_report().ntp_seconds,
                            parser_.sender_report().ntp_fraction);
    }
  }
  return true;
}

void ReceiverRtcpSession::OnReceivedNtp(uint32_t ntp_seconds,
                                        uint32_t ntp_fraction) {
  last_report_truncated_ntp_ = ConvertToNtpDiff(ntp_seconds, ntp_fraction);

  const base::TimeTicks now = clock_->NowTicks();
  time_last_report_received_ = now;

  // TODO(miu): This clock offset calculation does not account for packet
  // transit time over the network.  End2EndTest.EvilNetwork confirms that this
  // contributes a very significant source of error here.  Determine whether
  // RTT should be factored-in, and how that changes the rest of the
  // calculation.
  const base::TimeDelta measured_offset =
      now - ConvertNtpToTimeTicks(ntp_seconds, ntp_fraction);
  local_clock_ahead_by_.Update(now, measured_offset);
  if (measured_offset < local_clock_ahead_by_.Current()) {
    // Logically, the minimum offset between the clocks has to be the correct
    // one.  For example, the time it took to transmit the current report may
    // have been lower than usual, and so some of the error introduced by the
    // transmission time can be eliminated.
    local_clock_ahead_by_.Reset(now, measured_offset);
  }
  VLOG(1) << "Local clock is ahead of the remote clock by: "
          << "measured=" << measured_offset.InMicroseconds() << " usec, "
          << "filtered=" << local_clock_ahead_by_.Current().InMicroseconds()
          << " usec.";
}

void ReceiverRtcpSession::SendRtcpReport(
    RtcpTimeData time_data,
    const RtcpCastMessage* cast_message,
    base::TimeDelta target_delay,
    const ReceiverRtcpEventSubscriber::RtcpEvents* rtcp_events,
    const RtpReceiverStatistics* rtp_receiver_statistics) const {
  RtcpReportBlock report_block;
  RtcpReceiverReferenceTimeReport rrtr;
  rrtr.ntp_seconds = time_data.ntp_seconds;
  rrtr.ntp_fraction = time_data.ntp_fraction;

  if (rtp_receiver_statistics) {
    report_block.remote_ssrc = 0;            // Not needed to set send side.
    report_block.media_ssrc = remote_ssrc_;  // SSRC of the RTP packet sender.
    report_block.fraction_lost = rtp_receiver_statistics->fraction_lost;
    report_block.cumulative_lost = rtp_receiver_statistics->cumulative_lost;
    report_block.extended_high_sequence_number =
        rtp_receiver_statistics->extended_high_sequence_number;
    report_block.jitter = rtp_receiver_statistics->jitter;
    report_block.last_sr = last_report_truncated_ntp_;
    if (!time_last_report_received_.is_null()) {
      uint32_t delay_seconds = 0;
      uint32_t delay_fraction = 0;
      base::TimeDelta delta = time_data.timestamp - time_last_report_received_;
      ConvertTimeToFractions(delta.InMicroseconds(), &delay_seconds,
                             &delay_fraction);
      report_block.delay_since_last_sr =
          ConvertToNtpDiff(delay_seconds, delay_fraction);
    } else {
      report_block.delay_since_last_sr = 0;
    }
  }
  RtcpBuilder rtcp_builder(local_ssrc_);
  packet_sender_->SendRtcpPacket(
      local_ssrc_, rtcp_builder.BuildRtcpFromReceiver(
                       rtp_receiver_statistics ? &report_block : NULL, &rrtr,
                       cast_message, rtcp_events, target_delay));
}

void ReceiverRtcpSession::OnReceivedLipSyncInfo(RtpTimeTicks rtp_timestamp,
                                                uint32_t ntp_seconds,
                                                uint32_t ntp_fraction) {
  if (ntp_seconds == 0) {
    NOTREACHED();
    return;
  }
  lip_sync_rtp_timestamp_ = rtp_timestamp;
  lip_sync_ntp_timestamp_ =
      (static_cast<uint64_t>(ntp_seconds) << 32) | ntp_fraction;
}

bool ReceiverRtcpSession::GetLatestLipSyncTimes(
    RtpTimeTicks* rtp_timestamp,
    base::TimeTicks* reference_time) const {
  if (!lip_sync_ntp_timestamp_)
    return false;

  const base::TimeTicks local_reference_time =
      ConvertNtpToTimeTicks(
          static_cast<uint32_t>(lip_sync_ntp_timestamp_ >> 32),
          static_cast<uint32_t>(lip_sync_ntp_timestamp_)) +
      local_clock_ahead_by_.Current();

  // Sanity-check: Getting regular lip sync updates?
  DCHECK((clock_->NowTicks() - local_reference_time) <
         base::TimeDelta::FromMinutes(1));

  *rtp_timestamp = lip_sync_rtp_timestamp_;
  *reference_time = local_reference_time;
  return true;
}

}  // namespace cast
}  // namespace media
