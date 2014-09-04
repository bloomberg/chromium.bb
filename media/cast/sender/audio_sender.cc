// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/audio_sender.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/cast_defines.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/sender/audio_encoder.h"

namespace media {
namespace cast {
namespace {

const int kNumAggressiveReportsSentAtStart = 100;

// TODO(miu): This should be specified in AudioSenderConfig, but currently it is
// fixed to 100 FPS (i.e., 10 ms per frame), and AudioEncoder assumes this as
// well.
const int kAudioFrameRate = 100;

}  // namespace

AudioSender::AudioSender(scoped_refptr<CastEnvironment> cast_environment,
                         const AudioSenderConfig& audio_config,
                         CastTransportSender* const transport_sender)
    : FrameSender(
        cast_environment,
        transport_sender,
        base::TimeDelta::FromMilliseconds(audio_config.rtcp_interval),
        audio_config.frequency,
        audio_config.ssrc,
        kAudioFrameRate * 2.0, // We lie to increase max outstanding frames.
        audio_config.target_playout_delay),
      configured_encoder_bitrate_(audio_config.bitrate),
      weak_factory_(this) {
  cast_initialization_status_ = STATUS_AUDIO_UNINITIALIZED;
  VLOG(1) << "max_unacked_frames " << max_unacked_frames_;
  DCHECK_GT(max_unacked_frames_, 0);

  if (!audio_config.use_external_encoder) {
    audio_encoder_.reset(
        new AudioEncoder(cast_environment,
                         audio_config.channels,
                         audio_config.frequency,
                         audio_config.bitrate,
                         audio_config.codec,
                         base::Bind(&AudioSender::SendEncodedAudioFrame,
                                    weak_factory_.GetWeakPtr())));
    cast_initialization_status_ = audio_encoder_->InitializationResult();
  } else {
    NOTREACHED();  // No support for external audio encoding.
    cast_initialization_status_ = STATUS_AUDIO_UNINITIALIZED;
  }

  media::cast::CastTransportRtpConfig transport_config;
  transport_config.ssrc = audio_config.ssrc;
  transport_config.feedback_ssrc = audio_config.incoming_feedback_ssrc;
  transport_config.rtp_payload_type = audio_config.rtp_payload_type;
  // TODO(miu): AudioSender needs to be like VideoSender in providing an upper
  // limit on the number of in-flight frames.
  transport_config.stored_frames = max_unacked_frames_;
  transport_config.aes_key = audio_config.aes_key;
  transport_config.aes_iv_mask = audio_config.aes_iv_mask;

  transport_sender->InitializeAudio(
      transport_config,
      base::Bind(&AudioSender::OnReceivedCastFeedback,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&AudioSender::OnReceivedRtt, weak_factory_.GetWeakPtr()));
}

AudioSender::~AudioSender() {}

void AudioSender::InsertAudio(scoped_ptr<AudioBus> audio_bus,
                              const base::TimeTicks& recorded_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (cast_initialization_status_ != STATUS_AUDIO_INITIALIZED) {
    NOTREACHED();
    return;
  }
  DCHECK(audio_encoder_.get()) << "Invalid internal state";

  if (ShouldDropNextFrame(recorded_time)) {
    VLOG(1) << "Dropping frame due to too many frames currently in-flight.";
    return;
  }

  audio_encoder_->InsertAudio(audio_bus.Pass(), recorded_time);
}

void AudioSender::SendEncodedAudioFrame(
    scoped_ptr<EncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  const uint32 frame_id = encoded_frame->frame_id;

  const bool is_first_frame_to_be_sent = last_send_time_.is_null();
  last_send_time_ = cast_environment_->Clock()->NowTicks();
  last_sent_frame_id_ = frame_id;
  // If this is the first frame about to be sent, fake the value of
  // |latest_acked_frame_id_| to indicate the receiver starts out all caught up.
  // Also, schedule the periodic frame re-send checks.
  if (is_first_frame_to_be_sent) {
    latest_acked_frame_id_ = frame_id - 1;
    ScheduleNextResendCheck();
  }

  cast_environment_->Logging()->InsertEncodedFrameEvent(
      last_send_time_, FRAME_ENCODED, AUDIO_EVENT, encoded_frame->rtp_timestamp,
      frame_id, static_cast<int>(encoded_frame->data.size()),
      encoded_frame->dependency == EncodedFrame::KEY,
      configured_encoder_bitrate_);

  RecordLatestFrameTimestamps(frame_id,
                              encoded_frame->reference_time,
                              encoded_frame->rtp_timestamp);

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

  if (send_target_playout_delay_) {
    encoded_frame->new_playout_delay_ms =
        target_playout_delay_.InMilliseconds();
  }
  transport_sender_->InsertFrame(ssrc_, *encoded_frame);
}

void AudioSender::OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  if (is_rtt_available()) {
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

  if (last_send_time_.is_null())
    return;  // Cannot get an ACK without having first sent a frame.

  if (cast_feedback.missing_frames_and_packets.empty()) {
    // We only count duplicate ACKs when we have sent newer frames.
    if (latest_acked_frame_id_ == cast_feedback.ack_frame_id &&
        latest_acked_frame_id_ != last_sent_frame_id_) {
      duplicate_ack_counter_++;
    } else {
      duplicate_ack_counter_ = 0;
    }
    // TODO(miu): The values "2" and "3" should be derived from configuration.
    if (duplicate_ack_counter_ >= 2 && duplicate_ack_counter_ % 3 == 2) {
      VLOG(1) << "Received duplicate ACK for frame " << latest_acked_frame_id_;
      ResendForKickstart();
    }
  } else {
    // Only count duplicated ACKs if there is no NACK request in between.
    // This is to avoid aggresive resend.
    duplicate_ack_counter_ = 0;
  }

  cast_environment_->Logging()->InsertFrameEvent(
      cast_environment_->Clock()->NowTicks(),
      FRAME_ACK_RECEIVED,
      AUDIO_EVENT,
      GetRecordedRtpTimestamp(cast_feedback.ack_frame_id),
      cast_feedback.ack_frame_id);

  const bool is_acked_out_of_order =
      static_cast<int32>(cast_feedback.ack_frame_id -
                             latest_acked_frame_id_) < 0;
  VLOG(2) << "Received ACK" << (is_acked_out_of_order ? " out-of-order" : "")
          << " for frame " << cast_feedback.ack_frame_id;
  if (!is_acked_out_of_order) {
    // Cancel resends of acked frames.
    std::vector<uint32> cancel_sending_frames;
    while (latest_acked_frame_id_ != cast_feedback.ack_frame_id) {
      latest_acked_frame_id_++;
      cancel_sending_frames.push_back(latest_acked_frame_id_);
    }
    transport_sender_->CancelSendingFrames(ssrc_, cancel_sending_frames);
    latest_acked_frame_id_ = cast_feedback.ack_frame_id;
  }
}

bool AudioSender::ShouldDropNextFrame(base::TimeTicks capture_time) const {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  int frames_in_flight = 0;
  base::TimeDelta duration_in_flight;
  if (!last_send_time_.is_null()) {
    frames_in_flight =
        static_cast<int32>(last_sent_frame_id_ - latest_acked_frame_id_);
    if (frames_in_flight > 0) {
      const uint32 oldest_unacked_frame_id = latest_acked_frame_id_ + 1;
      duration_in_flight =
          capture_time - GetRecordedReferenceTime(oldest_unacked_frame_id);
    }
  }
  VLOG(2) << frames_in_flight
          << " frames in flight; last sent: " << last_sent_frame_id_
          << "; latest acked: " << latest_acked_frame_id_
          << "; duration in flight: "
          << duration_in_flight.InMicroseconds() << " usec ("
          << (target_playout_delay_ > base::TimeDelta() ?
                  100 * duration_in_flight / target_playout_delay_ :
                  kint64max) << "%)";
  return frames_in_flight >= max_unacked_frames_ ||
      duration_in_flight >= target_playout_delay_;
}

}  // namespace cast
}  // namespace media
