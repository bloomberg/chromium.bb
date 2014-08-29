// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/frame_sender.h"

namespace media {
namespace cast {
namespace {
const int kMinSchedulingDelayMs = 1;
}  // namespace

FrameSender::FrameSender(scoped_refptr<CastEnvironment> cast_environment,
                         CastTransportSender* const transport_sender,
                         base::TimeDelta rtcp_interval,
                         int rtp_timebase,
                         uint32 ssrc,
                         double max_frame_rate,
                         base::TimeDelta playout_delay)
    : cast_environment_(cast_environment),
      transport_sender_(transport_sender),
      ssrc_(ssrc),
      rtt_available_(false),
      rtcp_interval_(rtcp_interval),
      max_frame_rate_(max_frame_rate),
      num_aggressive_rtcp_reports_sent_(0),
      last_sent_frame_id_(0),
      latest_acked_frame_id_(0),
      duplicate_ack_counter_(0),
      rtp_timebase_(rtp_timebase),
      weak_factory_(this) {
  DCHECK_GT(rtp_timebase_, 0);
  SetTargetPlayoutDelay(playout_delay);
  send_target_playout_delay_ = false;
  memset(frame_rtp_timestamps_, 0, sizeof(frame_rtp_timestamps_));
}

FrameSender::~FrameSender() {
}

void FrameSender::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_next = rtcp_interval_;

  time_to_next = std::max(
      time_to_next, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&FrameSender::SendRtcpReport, weak_factory_.GetWeakPtr(),
                 true),
      time_to_next);
}

void FrameSender::SendRtcpReport(bool schedule_future_reports) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  // Sanity-check: We should have sent at least the first frame by this point.
  DCHECK(!last_send_time_.is_null());

  // Create lip-sync info for the sender report.  The last sent frame's
  // reference time and RTP timestamp are used to estimate an RTP timestamp in
  // terms of "now."  Note that |now| is never likely to be precise to an exact
  // frame boundary; and so the computation here will result in a
  // |now_as_rtp_timestamp| value that is rarely equal to any one emitted by the
  // encoder.
  const base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  const base::TimeDelta time_delta =
      now - GetRecordedReferenceTime(last_sent_frame_id_);
  const int64 rtp_delta = TimeDeltaToRtpDelta(time_delta, rtp_timebase_);
  const uint32 now_as_rtp_timestamp =
      GetRecordedRtpTimestamp(last_sent_frame_id_) +
          static_cast<uint32>(rtp_delta);
  transport_sender_->SendSenderReport(ssrc_, now, now_as_rtp_timestamp);

  if (schedule_future_reports)
    ScheduleNextRtcpReport();
}

void FrameSender::OnReceivedRtt(base::TimeDelta rtt,
                                base::TimeDelta avg_rtt,
                                base::TimeDelta min_rtt,
                                base::TimeDelta max_rtt) {
  rtt_available_ = true;
  rtt_ = rtt;
  avg_rtt_ = avg_rtt;
  min_rtt_ = min_rtt;
  max_rtt_ = max_rtt;
}

void FrameSender::SetTargetPlayoutDelay(
    base::TimeDelta new_target_playout_delay) {
  target_playout_delay_ = new_target_playout_delay;
  max_unacked_frames_ =
      std::min(kMaxUnackedFrames,
               1 + static_cast<int>(target_playout_delay_ *
                                    max_frame_rate_ /
                                    base::TimeDelta::FromSeconds(1)));
  send_target_playout_delay_ = true;
}

void FrameSender::ResendCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!last_send_time_.is_null());
  const base::TimeDelta time_since_last_send =
      cast_environment_->Clock()->NowTicks() - last_send_time_;
  if (time_since_last_send > target_playout_delay_) {
    if (latest_acked_frame_id_ == last_sent_frame_id_) {
      // Last frame acked, no point in doing anything
    } else {
      VLOG(1) << "ACK timeout; last acked frame: " << latest_acked_frame_id_;
      ResendForKickstart();
    }
  }
  ScheduleNextResendCheck();
}

void FrameSender::ScheduleNextResendCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!last_send_time_.is_null());
  base::TimeDelta time_to_next =
      last_send_time_ - cast_environment_->Clock()->NowTicks() +
      target_playout_delay_;
  time_to_next = std::max(
      time_to_next, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));
  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&FrameSender::ResendCheck, weak_factory_.GetWeakPtr()),
      time_to_next);
}

void FrameSender::ResendForKickstart() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!last_send_time_.is_null());
  VLOG(1) << "Resending last packet of frame " << last_sent_frame_id_
          << " to kick-start.";
  last_send_time_ = cast_environment_->Clock()->NowTicks();
  transport_sender_->ResendFrameForKickstart(ssrc_, last_sent_frame_id_);
}

void FrameSender::RecordLatestFrameTimestamps(uint32 frame_id,
                                              base::TimeTicks reference_time,
                                              RtpTimestamp rtp_timestamp) {
  DCHECK(!reference_time.is_null());
  frame_reference_times_[frame_id % arraysize(frame_reference_times_)] =
      reference_time;
  frame_rtp_timestamps_[frame_id % arraysize(frame_rtp_timestamps_)] =
      rtp_timestamp;
}

base::TimeTicks FrameSender::GetRecordedReferenceTime(uint32 frame_id) const {
  return frame_reference_times_[frame_id % arraysize(frame_reference_times_)];
}

RtpTimestamp FrameSender::GetRecordedRtpTimestamp(uint32 frame_id) const {
  return frame_rtp_timestamps_[frame_id % arraysize(frame_rtp_timestamps_)];
}

}  // namespace cast
}  // namespace media
