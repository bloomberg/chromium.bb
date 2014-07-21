// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtcp/rtcp.h"

#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/net/rtcp/rtcp_receiver.h"
#include "media/cast/net/rtcp/rtcp_sender.h"
#include "media/cast/net/rtcp/rtcp_utility.h"

using base::TimeDelta;

namespace media {
namespace cast {

static const int32 kMaxRttMs = 10000;  // 10 seconds.

class Rtcp::RtcpMessageHandlerImpl : public RtcpMessageHandler {
 public:
  explicit RtcpMessageHandlerImpl(Rtcp* rtcp)
      : rtcp_(rtcp) {}

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

  virtual void OnReceivedReceiverLog(const RtcpReceiverLogMessage& receiver_log)
      OVERRIDE {
    rtcp_->OnReceivedReceiverLog(receiver_log);
  }

  virtual void OnReceivedDelaySinceLastReport(
      uint32 last_report,
      uint32 delay_since_last_report) OVERRIDE {
    rtcp_->OnReceivedDelaySinceLastReport(last_report, delay_since_last_report);
  }

  virtual void OnReceivedCastFeedback(
      const RtcpCastMessage& cast_message) OVERRIDE {
    rtcp_->OnReceivedCastFeedback(cast_message);
  }

 private:
  Rtcp* rtcp_;
};

Rtcp::Rtcp(const RtcpCastMessageCallback& cast_callback,
           const RtcpRttCallback& rtt_callback,
           const RtcpLogMessageCallback& log_callback,
           base::TickClock* clock,
           PacedPacketSender* packet_sender,
           uint32 local_ssrc,
           uint32 remote_ssrc, const std::string& c_name)
    : cast_callback_(cast_callback),
      rtt_callback_(rtt_callback),
      log_callback_(log_callback),
      clock_(clock),
      rtcp_sender_(new RtcpSender(packet_sender, local_ssrc, c_name)),
      local_ssrc_(local_ssrc),
      remote_ssrc_(remote_ssrc),
      c_name_(c_name),
      handler_(new RtcpMessageHandlerImpl(this)),
      rtcp_receiver_(new RtcpReceiver(handler_.get(), local_ssrc)),
      last_report_truncated_ntp_(0),
      local_clock_ahead_by_(ClockDriftSmoother::GetDefaultTimeConstant()),
      lip_sync_rtp_timestamp_(0),
      lip_sync_ntp_timestamp_(0),
      min_rtt_(TimeDelta::FromMilliseconds(kMaxRttMs)),
      number_of_rtt_in_avg_(0) {
  rtcp_receiver_->SetRemoteSSRC(remote_ssrc);

  // This value is the same in FrameReceiver.
  rtcp_receiver_->SetCastReceiverEventHistorySize(
      kReceiverRtcpEventHistorySize);
}

Rtcp::~Rtcp() {}

bool Rtcp::IncomingRtcpPacket(const uint8* data, size_t length) {
  // Check if this is a valid RTCP packet.
  if (!RtcpReceiver::IsRtcpPacket(data, length)) {
    VLOG(1) << "Rtcp@" << this << "::IncomingRtcpPacket() -- "
            << "Received an invalid (non-RTCP?) packet.";
    return false;
  }

  // Check if this packet is to us.
  uint32 ssrc_of_sender = RtcpReceiver::GetSsrcOfSender(data, length);
  if (ssrc_of_sender != remote_ssrc_) {
    return false;
  }

  // Parse this packet.
  RtcpParser rtcp_parser(data, length);
  if (!rtcp_parser.IsValid()) {
    // Silently ignore packet.
    VLOG(1) << "Received invalid RTCP packet";
    return false;
  }
  rtcp_receiver_->IncomingRtcpPacket(&rtcp_parser);
  return true;
}

void Rtcp::SendRtcpFromRtpReceiver(
    const RtcpCastMessage* cast_message,
    base::TimeDelta target_delay,
    const ReceiverRtcpEventSubscriber::RtcpEventMultiMap* rtcp_events,
    RtpReceiverStatistics* rtp_receiver_statistics) {
  uint32 packet_type_flags = 0;

  base::TimeTicks now = clock_->NowTicks();
  RtcpReportBlock report_block;
  RtcpReceiverReferenceTimeReport rrtr;

  // Attach our NTP to all RTCP packets; with this information a "smart" sender
  // can make decisions based on how old the RTCP message is.
  packet_type_flags |= kRtcpRrtr;
  ConvertTimeTicksToNtp(now, &rrtr.ntp_seconds, &rrtr.ntp_fraction);
  SaveLastSentNtpTime(now, rrtr.ntp_seconds, rrtr.ntp_fraction);

  if (cast_message) {
    packet_type_flags |= kRtcpCast;
  }
  if (rtcp_events) {
    packet_type_flags |= kRtcpReceiverLog;
  }
  // If RTCP is in compound mode then we always send a RR.
  if (rtp_receiver_statistics) {
    packet_type_flags |= kRtcpRr;

    report_block.remote_ssrc = 0;            // Not needed to set send side.
    report_block.media_ssrc = remote_ssrc_;  // SSRC of the RTP packet sender.
    if (rtp_receiver_statistics) {
      rtp_receiver_statistics->GetStatistics(
          &report_block.fraction_lost, &report_block.cumulative_lost,
          &report_block.extended_high_sequence_number, &report_block.jitter);
    }

    report_block.last_sr = last_report_truncated_ntp_;
    if (!time_last_report_received_.is_null()) {
      uint32 delay_seconds = 0;
      uint32 delay_fraction = 0;
      base::TimeDelta delta = now - time_last_report_received_;
      ConvertTimeToFractions(delta.InMicroseconds(), &delay_seconds,
                             &delay_fraction);
      report_block.delay_since_last_sr =
          ConvertToNtpDiff(delay_seconds, delay_fraction);
    } else {
      report_block.delay_since_last_sr = 0;
    }
  }
  rtcp_sender_->SendRtcpFromRtpReceiver(packet_type_flags,
                                        &report_block,
                                        &rrtr,
                                        cast_message,
                                        rtcp_events,
                                        target_delay);
}

void Rtcp::SendRtcpFromRtpSender(base::TimeTicks current_time,
                                 uint32 current_time_as_rtp_timestamp,
                                 uint32 send_packet_count,
                                 size_t send_octet_count) {
  uint32 packet_type_flags = kRtcpSr;
  uint32 current_ntp_seconds = 0;
  uint32 current_ntp_fractions = 0;
  ConvertTimeTicksToNtp(current_time, &current_ntp_seconds,
                        &current_ntp_fractions);
  SaveLastSentNtpTime(current_time, current_ntp_seconds,
                      current_ntp_fractions);

  RtcpDlrrReportBlock dlrr;
  if (!time_last_report_received_.is_null()) {
    packet_type_flags |= kRtcpDlrr;
    dlrr.last_rr = last_report_truncated_ntp_;
    uint32 delay_seconds = 0;
    uint32 delay_fraction = 0;
    base::TimeDelta delta = current_time - time_last_report_received_;
    // TODO(hclam): DLRR is not used by any receiver. Consider removing
    // it. There is one race condition in the computation of the time for
    // DLRR: current time is submitted to this method while
    // |time_last_report_received_| is updated just before that. This can
    // happen if current time is not submitted synchronously.
    if (delta < base::TimeDelta())
      delta = base::TimeDelta();
    ConvertTimeToFractions(delta.InMicroseconds(), &delay_seconds,
                           &delay_fraction);

    dlrr.delay_since_last_rr = ConvertToNtpDiff(delay_seconds, delay_fraction);
  }

  RtcpSenderInfo sender_info;
  sender_info.ntp_seconds = current_ntp_seconds;
  sender_info.ntp_fraction = current_ntp_fractions;
  sender_info.rtp_timestamp = current_time_as_rtp_timestamp;
  sender_info.send_packet_count = send_packet_count;
  sender_info.send_octet_count = send_octet_count;

  rtcp_sender_->SendRtcpFromRtpSender(packet_type_flags, sender_info, dlrr);
}

void Rtcp::OnReceivedNtp(uint32 ntp_seconds, uint32 ntp_fraction) {
  last_report_truncated_ntp_ = ConvertToNtpDiff(ntp_seconds, ntp_fraction);

  const base::TimeTicks now = clock_->NowTicks();
  time_last_report_received_ = now;

  // TODO(miu): This clock offset calculation does not account for packet
  // transit time over the network.  End2EndTest.EvilNetwork confirms that this
  // contributes a very significant source of error here.  Fix this along with
  // the RTT clean-up.
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

void Rtcp::OnReceivedLipSyncInfo(uint32 rtp_timestamp, uint32 ntp_seconds,
                                 uint32 ntp_fraction) {
  if (ntp_seconds == 0) {
    NOTREACHED();
    return;
  }
  lip_sync_rtp_timestamp_ = rtp_timestamp;
  lip_sync_ntp_timestamp_ =
      (static_cast<uint64>(ntp_seconds) << 32) | ntp_fraction;
}

bool Rtcp::GetLatestLipSyncTimes(uint32* rtp_timestamp,
                                 base::TimeTicks* reference_time) const {
  if (!lip_sync_ntp_timestamp_)
    return false;

  const base::TimeTicks local_reference_time =
      ConvertNtpToTimeTicks(static_cast<uint32>(lip_sync_ntp_timestamp_ >> 32),
                            static_cast<uint32>(lip_sync_ntp_timestamp_)) +
      local_clock_ahead_by_.Current();

  // Sanity-check: Getting regular lip sync updates?
  DCHECK((clock_->NowTicks() - local_reference_time) <
         base::TimeDelta::FromMinutes(1));

  *rtp_timestamp = lip_sync_rtp_timestamp_;
  *reference_time = local_reference_time;
  return true;
}

void Rtcp::OnReceivedDelaySinceLastReport(uint32 last_report,
                                          uint32 delay_since_last_report) {
  RtcpSendTimeMap::iterator it = last_reports_sent_map_.find(last_report);
  if (it == last_reports_sent_map_.end()) {
    return;  // Feedback on another report.
  }

  base::TimeDelta sender_delay = clock_->NowTicks() - it->second;
  UpdateRtt(sender_delay, ConvertFromNtpDiff(delay_since_last_report));
}

void Rtcp::OnReceivedCastFeedback(const RtcpCastMessage& cast_message) {
  if (cast_callback_.is_null())
    return;
  cast_callback_.Run(cast_message);
}

void Rtcp::SaveLastSentNtpTime(const base::TimeTicks& now,
                               uint32 last_ntp_seconds,
                               uint32 last_ntp_fraction) {
  // Make sure |now| is always greater than the last element in
  // |last_reports_sent_queue_|.
  if (!last_reports_sent_queue_.empty())
    DCHECK(now >= last_reports_sent_queue_.back().second);

  uint32 last_report = ConvertToNtpDiff(last_ntp_seconds, last_ntp_fraction);
  last_reports_sent_map_[last_report] = now;
  last_reports_sent_queue_.push(std::make_pair(last_report, now));

  base::TimeTicks timeout = now - TimeDelta::FromMilliseconds(kMaxRttMs);

  // Cleanup old statistics older than |timeout|.
  while (!last_reports_sent_queue_.empty()) {
    RtcpSendTimePair oldest_report = last_reports_sent_queue_.front();
    if (oldest_report.second < timeout) {
      last_reports_sent_map_.erase(oldest_report.first);
      last_reports_sent_queue_.pop();
    } else {
      break;
    }
  }
}

void Rtcp::UpdateRtt(const base::TimeDelta& sender_delay,
                     const base::TimeDelta& receiver_delay) {
  base::TimeDelta rtt = sender_delay - receiver_delay;
  // TODO(miu): Find out why this must be >= 1 ms, and remove the fudge if it's
  // bogus.
  rtt = std::max(rtt, base::TimeDelta::FromMilliseconds(1));
  rtt_ = rtt;
  min_rtt_ = std::min(min_rtt_, rtt);
  max_rtt_ = std::max(max_rtt_, rtt);

  // TODO(miu): Replace "average for all time" with an EWMA, or suitable
  // "average over recent past" mechanism.
  if (number_of_rtt_in_avg_ != 0) {
    // Integer math equivalent of (ac/(ac+1.0))*avg_rtt_ + (1.0/(ac+1.0))*rtt).
    // (TimeDelta only supports math with other TimeDeltas and int64s.)
    avg_rtt_ = (avg_rtt_ * number_of_rtt_in_avg_ + rtt) /
        (number_of_rtt_in_avg_ + 1);
  } else {
    avg_rtt_ = rtt;
  }
  number_of_rtt_in_avg_++;

  if (!rtt_callback_.is_null())
    rtt_callback_.Run(rtt, avg_rtt_, min_rtt_, max_rtt_);
}

bool Rtcp::Rtt(base::TimeDelta* rtt, base::TimeDelta* avg_rtt,
               base::TimeDelta* min_rtt, base::TimeDelta* max_rtt) const {
  DCHECK(rtt) << "Invalid argument";
  DCHECK(avg_rtt) << "Invalid argument";
  DCHECK(min_rtt) << "Invalid argument";
  DCHECK(max_rtt) << "Invalid argument";

  if (number_of_rtt_in_avg_ == 0) return false;

  *rtt = rtt_;
  *avg_rtt = avg_rtt_;
  *min_rtt = min_rtt_;
  *max_rtt = max_rtt_;
  return true;
}

void Rtcp::OnReceivedReceiverLog(const RtcpReceiverLogMessage& receiver_log) {
  if (log_callback_.is_null())
    return;
  log_callback_.Run(receiver_log);
}

}  // namespace cast
}  // namespace media
