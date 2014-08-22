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
                         int frequency,
                         uint32 ssrc,
                         double max_frame_rate,
                         base::TimeDelta playout_delay)
    : cast_environment_(cast_environment),
      transport_sender_(transport_sender),
      ssrc_(ssrc),
      rtp_timestamp_helper_(frequency),
      rtt_available_(false),
      rtcp_interval_(rtcp_interval),
      max_frame_rate_(max_frame_rate),
      weak_factory_(this) {
  SetTargetPlayoutDelay(playout_delay);
  send_target_playout_delay_ = false;
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
  const base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  uint32 now_as_rtp_timestamp = 0;
  if (rtp_timestamp_helper_.GetCurrentTimeAsRtpTimestamp(
          now, &now_as_rtp_timestamp)) {
    transport_sender_->SendSenderReport(ssrc_, now, now_as_rtp_timestamp);
  } else {
    // |rtp_timestamp_helper_| should have stored a mapping by this point.
    NOTREACHED();
  }
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

}  // namespace cast
}  // namespace media
