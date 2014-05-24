// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/audio_sender/audio_sender.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/audio_sender/audio_encoder.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace media {
namespace cast {

const int kNumAggressiveReportsSentAtStart = 100;
const int kMinSchedulingDelayMs = 1;

// TODO(mikhal): Reduce heap allocation when not needed.
AudioSender::AudioSender(scoped_refptr<CastEnvironment> cast_environment,
                         const AudioSenderConfig& audio_config,
                         transport::CastTransportSender* const transport_sender)
    : cast_environment_(cast_environment),
      transport_sender_(transport_sender),
      rtp_timestamp_helper_(audio_config.frequency),
      rtcp_(cast_environment,
            this,
            transport_sender_,
            NULL,  // paced sender.
            NULL,
            audio_config.rtcp_mode,
            base::TimeDelta::FromMilliseconds(audio_config.rtcp_interval),
            audio_config.rtp_config.ssrc,
            audio_config.incoming_feedback_ssrc,
            audio_config.rtcp_c_name,
            true),
      num_aggressive_rtcp_reports_sent_(0),
      cast_initialization_cb_(STATUS_AUDIO_UNINITIALIZED),
      weak_factory_(this) {
  rtcp_.SetCastReceiverEventHistorySize(kReceiverRtcpEventHistorySize);
  if (!audio_config.use_external_encoder) {
    audio_encoder_.reset(
        new AudioEncoder(cast_environment,
                         audio_config,
                         base::Bind(&AudioSender::SendEncodedAudioFrame,
                                    weak_factory_.GetWeakPtr())));
    cast_initialization_cb_ = audio_encoder_->InitializationResult();
  }

  media::cast::transport::CastTransportAudioConfig transport_config;
  transport_config.codec = audio_config.codec;
  transport_config.rtp.config = audio_config.rtp_config;
  transport_config.frequency = audio_config.frequency;
  transport_config.channels = audio_config.channels;
  transport_config.rtp.max_outstanding_frames =
      audio_config.rtp_config.max_delay_ms / 100 + 1;
  transport_sender_->InitializeAudio(transport_config);
}

AudioSender::~AudioSender() {}

void AudioSender::InsertAudio(scoped_ptr<AudioBus> audio_bus,
                              const base::TimeTicks& recorded_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(audio_encoder_.get()) << "Invalid internal state";
  audio_encoder_->InsertAudio(audio_bus.Pass(), recorded_time);
}

void AudioSender::SendEncodedAudioFrame(
    scoped_ptr<transport::EncodedFrame> audio_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!audio_frame->reference_time.is_null());
  rtp_timestamp_helper_.StoreLatestTime(audio_frame->reference_time,
                                        audio_frame->rtp_timestamp);

  // At the start of the session, it's important to send reports before each
  // frame so that the receiver can properly compute playout times.  The reason
  // more than one report is sent is because transmission is not guaranteed,
  // only best effort, so we send enough that one should almost certainly get
  // through.
  if (num_aggressive_rtcp_reports_sent_ < kNumAggressiveReportsSentAtStart) {
    // SendRtcpReport() will schedule future reports to be made if this is the
    // last "aggressive report."
    ++num_aggressive_rtcp_reports_sent_;
    const bool is_last_aggressive_report =
        (num_aggressive_rtcp_reports_sent_ == kNumAggressiveReportsSentAtStart);
    VLOG_IF(1, is_last_aggressive_report) << "Sending last aggressive report.";
    SendRtcpReport(is_last_aggressive_report);
  }

  transport_sender_->InsertCodedAudioFrame(*audio_frame);
}

void AudioSender::ResendPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  transport_sender_->ResendPackets(true, missing_frames_and_packets);
}

void AudioSender::IncomingRtcpPacket(scoped_ptr<Packet> packet) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  rtcp_.IncomingRtcpPacket(&packet->front(), packet->size());
}

void AudioSender::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_next =
      rtcp_.TimeToSendNextRtcpReport() - cast_environment_->Clock()->NowTicks();

  time_to_next = std::max(
      time_to_next, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&AudioSender::SendRtcpReport,
                 weak_factory_.GetWeakPtr(),
                 true),
      time_to_next);
}

void AudioSender::SendRtcpReport(bool schedule_future_reports) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  const base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  uint32 now_as_rtp_timestamp = 0;
  if (rtp_timestamp_helper_.GetCurrentTimeAsRtpTimestamp(
          now, &now_as_rtp_timestamp)) {
    rtcp_.SendRtcpFromRtpSender(now, now_as_rtp_timestamp);
  } else {
    // |rtp_timestamp_helper_| should have stored a mapping by this point.
    NOTREACHED();
  }
  if (schedule_future_reports)
    ScheduleNextRtcpReport();
}

void AudioSender::OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  if (rtcp_.is_rtt_available()) {
    // Having the RTT values implies the receiver sent back a receiver report
    // based on it having received a report from here.  Therefore, ensure this
    // sender stops aggressively sending reports.
    if (num_aggressive_rtcp_reports_sent_ < kNumAggressiveReportsSentAtStart) {
      VLOG(1) << "No longer a need to send reports aggressively (sent "
              << num_aggressive_rtcp_reports_sent_ << ").";
      num_aggressive_rtcp_reports_sent_ = kNumAggressiveReportsSentAtStart;
      ScheduleNextRtcpReport();
    }
  }

  if (!cast_feedback.missing_frames_and_packets_.empty()) {
    ResendPackets(cast_feedback.missing_frames_and_packets_);
  }
  VLOG(2) << "Received audio ACK "
          << static_cast<int>(cast_feedback.ack_frame_id_);
}

}  // namespace cast
}  // namespace media
