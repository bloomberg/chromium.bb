// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtcp/rtcp.h"

#include "base/big_endian.h"
#include "base/rand_util.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "media/cast/rtcp/rtcp_receiver.h"
#include "media/cast/rtcp/rtcp_sender.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace media {
namespace cast {

static const int kMaxRttMs = 10000;  // 10 seconds.
static const uint16 kMaxDelay = 2000;

// Time limit for received RTCP messages when we stop using it for lip-sync.
static const int64 kMaxDiffSinceReceivedRtcpMs = 100000;  // 100 seconds.

class LocalRtcpRttFeedback : public RtcpRttFeedback {
 public:
  explicit LocalRtcpRttFeedback(Rtcp* rtcp) : rtcp_(rtcp) {}

  virtual void OnReceivedDelaySinceLastReport(
      uint32 receivers_ssrc, uint32 last_report,
      uint32 delay_since_last_report) OVERRIDE {
    rtcp_->OnReceivedDelaySinceLastReport(receivers_ssrc, last_report,
                                          delay_since_last_report);
  }

 private:
  Rtcp* rtcp_;
};

class LocalRtcpReceiverFeedback : public RtcpReceiverFeedback {
 public:
  LocalRtcpReceiverFeedback(Rtcp* rtcp,
                            scoped_refptr<CastEnvironment> cast_environment)
      : rtcp_(rtcp), cast_environment_(cast_environment) {}

  virtual void OnReceivedSenderReport(
      const transport::RtcpSenderInfo& remote_sender_info) OVERRIDE {
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

  virtual void OnReceivedReceiverLog(const RtcpReceiverLogMessage& receiver_log)
      OVERRIDE {
    // Add received log messages into our log system.
    RtcpReceiverLogMessage::const_iterator it = receiver_log.begin();

    for (; it != receiver_log.end(); ++it) {
      uint32 rtp_timestamp = it->rtp_timestamp_;

      RtcpReceiverEventLogMessages::const_iterator event_it =
          it->event_log_messages_.begin();
      for (; event_it != it->event_log_messages_.end(); ++event_it) {
        switch (event_it->type) {
          case kAudioPacketReceived:
          case kVideoPacketReceived:
          case kDuplicateAudioPacketReceived:
          case kDuplicateVideoPacketReceived:
            cast_environment_->Logging()->InsertPacketEvent(
                event_it->event_timestamp, event_it->type, rtp_timestamp,
                kFrameIdUnknown, event_it->packet_id, 0, 0);
            break;
          case kAudioAckSent:
          case kVideoAckSent:
          case kAudioFrameDecoded:
          case kVideoFrameDecoded:
            cast_environment_->Logging()->InsertFrameEvent(
                event_it->event_timestamp, event_it->type, rtp_timestamp,
                kFrameIdUnknown);
            break;
          case kAudioPlayoutDelay:
          case kVideoRenderDelay:
            cast_environment_->Logging()->InsertFrameEventWithDelay(
                event_it->event_timestamp, event_it->type, rtp_timestamp,
                kFrameIdUnknown, event_it->delay_delta);
            break;
          default:
            VLOG(2) << "Received log message via RTCP that we did not expect: "
                    << static_cast<int>(event_it->type);
            break;
        }
      }
    }
  }

  virtual void OnReceivedSenderLog(
      const transport::RtcpSenderLogMessage& sender_log) OVERRIDE {
    transport::RtcpSenderLogMessage::const_iterator it = sender_log.begin();

    for (; it != sender_log.end(); ++it) {
      uint32 rtp_timestamp = it->rtp_timestamp;
      CastLoggingEvent log_event = kUnknown;

      // These events are provided to know the status of frames that never
      // reached the receiver. The timing information for these events are not
      // relevant and is not sent over the wire.
      switch (it->frame_status) {
        case transport::kRtcpSenderFrameStatusDroppedByFlowControl:
          // A frame that have been dropped by the flow control would have
          // kVideoFrameCaptured as its last event in the log.
          log_event = kVideoFrameCaptured;
          break;
        case transport::kRtcpSenderFrameStatusDroppedByEncoder:
          // A frame that have been dropped by the encoder would have
          // kVideoFrameSentToEncoder as its last event in the log.
          log_event = kVideoFrameSentToEncoder;
          break;
        case transport::kRtcpSenderFrameStatusSentToNetwork:
          // A frame that have be encoded is always sent to the network. We
          // do not add a new log entry for this.
          log_event = kVideoFrameEncoded;
          break;
        default:
          continue;
      }
      // TODO(pwestin): how do we handle the truncated rtp_timestamp?
      // Add received log messages into our log system.
      // TODO(pwestin): how do we handle the time? we don't care about it but
      // we need to send in one.
      base::TimeTicks now = cast_environment_->Clock()->NowTicks();
      cast_environment_->Logging()->InsertFrameEvent(
          now, log_event, rtp_timestamp, kFrameIdUnknown);
    }
  }

 private:
  Rtcp* rtcp_;
  scoped_refptr<CastEnvironment> cast_environment_;
};

Rtcp::Rtcp(scoped_refptr<CastEnvironment> cast_environment,
           RtcpSenderFeedback* sender_feedback,
           transport::CastTransportSender* const transport_sender,
           transport::PacedPacketSender* paced_packet_sender,
           RtpReceiverStatistics* rtp_receiver_statistics, RtcpMode rtcp_mode,
           const base::TimeDelta& rtcp_interval, uint32 local_ssrc,
           uint32 remote_ssrc, const std::string& c_name)
    : cast_environment_(cast_environment),
      transport_sender_(transport_sender),
      rtcp_interval_(rtcp_interval),
      rtcp_mode_(rtcp_mode),
      local_ssrc_(local_ssrc),
      remote_ssrc_(remote_ssrc),
      c_name_(c_name),
      rtp_receiver_statistics_(rtp_receiver_statistics),
      rtt_feedback_(new LocalRtcpRttFeedback(this)),
      receiver_feedback_(new LocalRtcpReceiverFeedback(this, cast_environment)),
      rtcp_sender_(new RtcpSender(cast_environment, paced_packet_sender,
                                  local_ssrc, c_name)),
      last_report_received_(0),
      last_received_rtp_timestamp_(0),
      last_received_ntp_seconds_(0),
      last_received_ntp_fraction_(0),
      min_rtt_(base::TimeDelta::FromMilliseconds(kMaxRttMs)),
      number_of_rtt_in_avg_(0) {
  rtcp_receiver_.reset(new RtcpReceiver(cast_environment, sender_feedback,
                                        receiver_feedback_.get(),
                                        rtt_feedback_.get(), local_ssrc));
  rtcp_receiver_->SetRemoteSSRC(remote_ssrc);
}

Rtcp::~Rtcp() {}

// static
bool Rtcp::IsRtcpPacket(const uint8* packet, size_t length) {
  DCHECK_GE(length, kMinLengthOfRtcp) << "Invalid RTCP packet";
  if (length < kMinLengthOfRtcp) return false;

  uint8 packet_type = packet[1];
  if (packet_type >= transport::kPacketTypeLow &&
      packet_type <= transport::kPacketTypeHigh) {
    return true;
  }
  return false;
}

// static
uint32 Rtcp::GetSsrcOfSender(const uint8* rtcp_buffer, size_t length) {
  DCHECK_GE(length, kMinLengthOfRtcp) << "Invalid RTCP packet";
  uint32 ssrc_of_sender;
  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(rtcp_buffer), length);
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

void Rtcp::IncomingRtcpPacket(const uint8* rtcp_buffer, size_t length) {
  RtcpParser rtcp_parser(rtcp_buffer, length);
  if (!rtcp_parser.IsValid()) {
    // Silently ignore packet.
    DLOG(ERROR) << "Received invalid RTCP packet";
    return;
  }
  rtcp_receiver_->IncomingRtcpPacket(&rtcp_parser);
}

void Rtcp::SendRtcpFromRtpReceiver(
    const RtcpCastMessage* cast_message,
    const ReceiverRtcpEventSubscriber::RtcpEventMultiMap* rtcp_events) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  uint32 packet_type_flags = 0;

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  transport::RtcpReportBlock report_block;
  RtcpReceiverReferenceTimeReport rrtr;

  // Attach our NTP to all RTCP packets; with this information a "smart" sender
  // can make decisions based on how old the RTCP message is.
  packet_type_flags |= transport::kRtcpRrtr;
  ConvertTimeTicksToNtp(now, &rrtr.ntp_seconds, &rrtr.ntp_fraction);
  SaveLastSentNtpTime(now, rrtr.ntp_seconds, rrtr.ntp_fraction);

  if (cast_message) {
    packet_type_flags |= transport::kRtcpCast;
  }
  if (rtcp_events) {
    packet_type_flags |= transport::kRtcpReceiverLog;
  }
  if (rtcp_mode_ == kRtcpCompound || now >= next_time_to_send_rtcp_) {
    packet_type_flags |= transport::kRtcpRr;

    report_block.remote_ssrc = 0;            // Not needed to set send side.
    report_block.media_ssrc = remote_ssrc_;  // SSRC of the RTP packet sender.
    if (rtp_receiver_statistics_) {
      rtp_receiver_statistics_->GetStatistics(
          &report_block.fraction_lost, &report_block.cumulative_lost,
          &report_block.extended_high_sequence_number, &report_block.jitter);
      cast_environment_->Logging()->InsertGenericEvent(now, kJitterMs,
                                                       report_block.jitter);
      cast_environment_->Logging()->InsertGenericEvent(
          now, kPacketLoss, report_block.fraction_lost);
    }

    report_block.last_sr = last_report_received_;
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
    UpdateNextTimeToSendRtcp();
  }
  rtcp_sender_->SendRtcpFromRtpReceiver(packet_type_flags,
                                        &report_block,
                                        &rrtr,
                                        cast_message,
                                        rtcp_events,
                                        target_delay_ms_);
}

void Rtcp::SendRtcpFromRtpSender(
    const transport::RtcpSenderLogMessage& sender_log_message,
    transport::RtcpSenderInfo sender_info) {
  DCHECK(transport_sender_);
  uint32 packet_type_flags = transport::kRtcpSr;
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();

  if (sender_log_message.size()) {
    packet_type_flags |= transport::kRtcpSenderLog;
  }

  SaveLastSentNtpTime(now, sender_info.ntp_seconds, sender_info.ntp_fraction);

  transport::RtcpDlrrReportBlock dlrr;
  if (!time_last_report_received_.is_null()) {
    packet_type_flags |= transport::kRtcpDlrr;
    dlrr.last_rr = last_report_received_;
    uint32 delay_seconds = 0;
    uint32 delay_fraction = 0;
    base::TimeDelta delta = now - time_last_report_received_;
    ConvertTimeToFractions(delta.InMicroseconds(), &delay_seconds,
                           &delay_fraction);

    dlrr.delay_since_last_rr = ConvertToNtpDiff(delay_seconds, delay_fraction);
  }

  transport_sender_->SendRtcpFromRtpSender(
      packet_type_flags, sender_info, dlrr, sender_log_message, local_ssrc_,
      c_name_);
  UpdateNextTimeToSendRtcp();
}

void Rtcp::OnReceivedNtp(uint32 ntp_seconds, uint32 ntp_fraction) {
  last_report_received_ = (ntp_seconds << 16) + (ntp_fraction >> 16);

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  time_last_report_received_ = now;
}

void Rtcp::OnReceivedLipSyncInfo(uint32 rtp_timestamp, uint32 ntp_seconds,
                                 uint32 ntp_fraction) {
  last_received_rtp_timestamp_ = rtp_timestamp;
  last_received_ntp_seconds_ = ntp_seconds;
  last_received_ntp_fraction_ = ntp_fraction;
}

void Rtcp::OnReceivedSendReportRequest() {
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();

  // Trigger a new RTCP report at next timer.
  next_time_to_send_rtcp_ = now;
}

bool Rtcp::RtpTimestampInSenderTime(int frequency, uint32 rtp_timestamp,
                                    base::TimeTicks* rtp_timestamp_in_ticks)
    const {
  if (last_received_ntp_seconds_ == 0)
    return false;

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
  if (std::abs(rtp_time_diff_ms) > kMaxDiffSinceReceivedRtcpMs)
    return false;

  *rtp_timestamp_in_ticks = ConvertNtpToTimeTicks(last_received_ntp_seconds_,
                                                  last_received_ntp_fraction_) +
                            base::TimeDelta::FromMilliseconds(rtp_time_diff_ms);
  return true;
}

void Rtcp::SetCastReceiverEventHistorySize(size_t size) {
  rtcp_receiver_->SetCastReceiverEventHistorySize(size);
}

void Rtcp::SetTargetDelay(base::TimeDelta target_delay) {
  target_delay_ms_ = static_cast<uint16>(target_delay.InMilliseconds());
  DCHECK(target_delay_ms_ < kMaxDelay);
}

void Rtcp::OnReceivedDelaySinceLastReport(uint32 receivers_ssrc,
                                          uint32 last_report,
                                          uint32 delay_since_last_report) {
  RtcpSendTimeMap::iterator it = last_reports_sent_map_.find(last_report);
  if (it == last_reports_sent_map_.end()) {
    return;  // Feedback on another report.
  }

  base::TimeDelta sender_delay =
      cast_environment_->Clock()->NowTicks() - it->second;
  UpdateRtt(sender_delay, ConvertFromNtpDiff(delay_since_last_report));
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

  base::TimeTicks timeout = now - base::TimeDelta::FromMilliseconds(kMaxRttMs);

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
  rtt = std::max(rtt, base::TimeDelta::FromMilliseconds(1));
  rtt_ = rtt;
  min_rtt_ = std::min(min_rtt_, rtt);
  max_rtt_ = std::max(max_rtt_, rtt);

  if (number_of_rtt_in_avg_ != 0) {
    float ac = static_cast<float>(number_of_rtt_in_avg_);
    avg_rtt_ms_ = ((ac / (ac + 1.0)) * avg_rtt_ms_) +
                  ((1.0 / (ac + 1.0)) * rtt.InMilliseconds());
  } else {
    avg_rtt_ms_ = rtt.InMilliseconds();
  }
  number_of_rtt_in_avg_++;
}

bool Rtcp::Rtt(base::TimeDelta* rtt, base::TimeDelta* avg_rtt,
               base::TimeDelta* min_rtt, base::TimeDelta* max_rtt) const {
  DCHECK(rtt) << "Invalid argument";
  DCHECK(avg_rtt) << "Invalid argument";
  DCHECK(min_rtt) << "Invalid argument";
  DCHECK(max_rtt) << "Invalid argument";

  if (number_of_rtt_in_avg_ == 0) return false;

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  cast_environment_->Logging()->InsertGenericEvent(now, kRttMs,
                                                   rtt->InMilliseconds());

  *rtt = rtt_;
  *avg_rtt = base::TimeDelta::FromMilliseconds(avg_rtt_ms_);
  *min_rtt = min_rtt_;
  *max_rtt = max_rtt_;
  return true;
}

int Rtcp::CheckForWrapAround(uint32 new_timestamp, uint32 old_timestamp) const {
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
  base::TimeDelta time_to_next =
      (rtcp_interval_ / 2) + (rtcp_interval_ * random / 1000);

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  next_time_to_send_rtcp_ = now + time_to_next;
}

}  // namespace cast
}  // namespace media
