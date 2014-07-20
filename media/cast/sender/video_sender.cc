// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_sender.h"

#include <algorithm>
#include <cstring>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/cast_defines.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/sender/external_video_encoder.h"
#include "media/cast/sender/video_encoder_impl.h"

namespace media {
namespace cast {

const int kNumAggressiveReportsSentAtStart = 100;
const int kMinSchedulingDelayMs = 1;

VideoSender::VideoSender(
    scoped_refptr<CastEnvironment> cast_environment,
    const VideoSenderConfig& video_config,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
    CastTransportSender* const transport_sender)
    : FrameSender(
        cast_environment,
        transport_sender,
        base::TimeDelta::FromMilliseconds(video_config.rtcp_interval),
        kVideoFrequency,
        video_config.ssrc),
      target_playout_delay_(video_config.target_playout_delay),
      max_unacked_frames_(
          std::min(kMaxUnackedFrames,
                   1 + static_cast<int>(target_playout_delay_ *
                                        video_config.max_frame_rate /
                                        base::TimeDelta::FromSeconds(1)))),
      num_aggressive_rtcp_reports_sent_(0),
      frames_in_encoder_(0),
      last_sent_frame_id_(0),
      latest_acked_frame_id_(0),
      duplicate_ack_counter_(0),
      congestion_control_(cast_environment->Clock(),
                          video_config.max_bitrate,
                          video_config.min_bitrate,
                          max_unacked_frames_),
      cast_initialization_status_(STATUS_VIDEO_UNINITIALIZED),
      weak_factory_(this) {
  VLOG(1) << "max_unacked_frames " << max_unacked_frames_;
  DCHECK_GT(max_unacked_frames_, 0);

  if (video_config.use_external_encoder) {
    video_encoder_.reset(new ExternalVideoEncoder(cast_environment,
                                                  video_config,
                                                  create_vea_cb,
                                                  create_video_encode_mem_cb));
  } else {
    video_encoder_.reset(new VideoEncoderImpl(
        cast_environment, video_config, max_unacked_frames_));
  }
  cast_initialization_status_ = STATUS_VIDEO_INITIALIZED;

  media::cast::CastTransportRtpConfig transport_config;
  transport_config.ssrc = video_config.ssrc;
  transport_config.feedback_ssrc = video_config.incoming_feedback_ssrc;
  transport_config.c_name = video_config.rtcp_c_name;
  transport_config.rtp_payload_type = video_config.rtp_payload_type;
  transport_config.stored_frames = max_unacked_frames_;
  transport_config.aes_key = video_config.aes_key;
  transport_config.aes_iv_mask = video_config.aes_iv_mask;

  transport_sender->InitializeVideo(
      transport_config,
      base::Bind(&VideoSender::OnReceivedCastFeedback,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&VideoSender::OnReceivedRtt, weak_factory_.GetWeakPtr()));

  memset(frame_id_to_rtp_timestamp_, 0, sizeof(frame_id_to_rtp_timestamp_));
}

VideoSender::~VideoSender() {
}

void VideoSender::InsertRawVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& capture_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (cast_initialization_status_ != STATUS_VIDEO_INITIALIZED) {
    NOTREACHED();
    return;
  }
  DCHECK(video_encoder_.get()) << "Invalid state";

  RtpTimestamp rtp_timestamp = GetVideoRtpTimestamp(capture_time);
  cast_environment_->Logging()->InsertFrameEvent(
      capture_time, FRAME_CAPTURE_BEGIN, VIDEO_EVENT,
      rtp_timestamp, kFrameIdUnknown);
  cast_environment_->Logging()->InsertFrameEvent(
      cast_environment_->Clock()->NowTicks(),
      FRAME_CAPTURE_END, VIDEO_EVENT,
      rtp_timestamp,
      kFrameIdUnknown);

  // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
  TRACE_EVENT_INSTANT2(
      "cast_perf_test", "InsertRawVideoFrame",
      TRACE_EVENT_SCOPE_THREAD,
      "timestamp", capture_time.ToInternalValue(),
      "rtp_timestamp", rtp_timestamp);

  if (AreTooManyFramesInFlight()) {
    VLOG(1) << "Dropping frame due to too many frames currently in-flight.";
    return;
  }

  uint32 bitrate = congestion_control_.GetBitrate(
      capture_time + target_playout_delay_, target_playout_delay_);

  video_encoder_->SetBitRate(bitrate);

  if (video_encoder_->EncodeVideoFrame(
          video_frame,
          capture_time,
          base::Bind(&VideoSender::SendEncodedVideoFrame,
                     weak_factory_.GetWeakPtr(),
                     bitrate))) {
    frames_in_encoder_++;
  } else {
    VLOG(1) << "Encoder rejected a frame.  Skipping...";
  }
}

void VideoSender::SendEncodedVideoFrame(
    int requested_bitrate_before_encode,
    scoped_ptr<EncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  DCHECK_GT(frames_in_encoder_, 0);
  frames_in_encoder_--;

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

  VLOG_IF(1, encoded_frame->dependency == EncodedFrame::KEY)
      << "Send encoded key frame; frame_id: " << frame_id;

  cast_environment_->Logging()->InsertEncodedFrameEvent(
      last_send_time_, FRAME_ENCODED, VIDEO_EVENT, encoded_frame->rtp_timestamp,
      frame_id, static_cast<int>(encoded_frame->data.size()),
      encoded_frame->dependency == EncodedFrame::KEY,
      requested_bitrate_before_encode);
  // Only use lowest 8 bits as key.
  frame_id_to_rtp_timestamp_[frame_id & 0xff] = encoded_frame->rtp_timestamp;

  // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
  TRACE_EVENT_INSTANT1(
      "cast_perf_test", "VideoFrameEncoded",
      TRACE_EVENT_SCOPE_THREAD,
      "rtp_timestamp", encoded_frame->rtp_timestamp);

  DCHECK(!encoded_frame->reference_time.is_null());
  rtp_timestamp_helper_.StoreLatestTime(encoded_frame->reference_time,
                                        encoded_frame->rtp_timestamp);

  // At the start of the session, it's important to send reports before each
  // frame so that the receiver can properly compute playout times.  The reason
  // more than one report is sent is because transmission is not guaranteed,
  // only best effort, so send enough that one should almost certainly get
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

  congestion_control_.SendFrameToTransport(
      frame_id, encoded_frame->data.size() * 8, last_send_time_);

  transport_sender_->InsertCodedVideoFrame(*encoded_frame);
}

void VideoSender::ResendCheck() {
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

void VideoSender::ScheduleNextResendCheck() {
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
      base::Bind(&VideoSender::ResendCheck, weak_factory_.GetWeakPtr()),
      time_to_next);
}

void VideoSender::OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  base::TimeDelta rtt;
  base::TimeDelta avg_rtt;
  base::TimeDelta min_rtt;
  base::TimeDelta max_rtt;
  if (is_rtt_available()) {
    rtt = rtt_;
    avg_rtt = avg_rtt_;
    min_rtt = min_rtt_;
    max_rtt = max_rtt_;

    congestion_control_.UpdateRtt(rtt);

    // Don't use a RTT lower than our average.
    rtt = std::max(rtt, avg_rtt);

    // Having the RTT values implies the receiver sent back a receiver report
    // based on it having received a report from here.  Therefore, ensure this
    // sender stops aggressively sending reports.
    if (num_aggressive_rtcp_reports_sent_ < kNumAggressiveReportsSentAtStart) {
      VLOG(1) << "No longer a need to send reports aggressively (sent "
              << num_aggressive_rtcp_reports_sent_ << ").";
      num_aggressive_rtcp_reports_sent_ = kNumAggressiveReportsSentAtStart;
      ScheduleNextRtcpReport();
    }
  } else {
    // We have no measured value use default.
    rtt = base::TimeDelta::FromMilliseconds(kStartRttMs);
  }

  if (last_send_time_.is_null())
    return;  // Cannot get an ACK without having first sent a frame.

  if (cast_feedback.missing_frames_and_packets.empty()) {
    video_encoder_->LatestFrameIdToReference(cast_feedback.ack_frame_id);

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

    // A NACK is also used to cancel pending re-transmissions.
    transport_sender_->ResendPackets(
        false, cast_feedback.missing_frames_and_packets, true, rtt);
  }

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  congestion_control_.AckFrame(cast_feedback.ack_frame_id, now);

  RtpTimestamp rtp_timestamp =
      frame_id_to_rtp_timestamp_[cast_feedback.ack_frame_id & 0xff];
  cast_environment_->Logging()->InsertFrameEvent(now,
                                                 FRAME_ACK_RECEIVED,
                                                 VIDEO_EVENT,
                                                 rtp_timestamp,
                                                 cast_feedback.ack_frame_id);

  const bool is_acked_out_of_order =
      static_cast<int32>(cast_feedback.ack_frame_id -
                             latest_acked_frame_id_) < 0;
  VLOG(2) << "Received ACK" << (is_acked_out_of_order ? " out-of-order" : "")
          << " for frame " << cast_feedback.ack_frame_id;
  if (!is_acked_out_of_order) {
    // Cancel resends of acked frames.
    MissingFramesAndPacketsMap missing_frames_and_packets;
    PacketIdSet missing;
    while (latest_acked_frame_id_ != cast_feedback.ack_frame_id) {
      latest_acked_frame_id_++;
      missing_frames_and_packets[latest_acked_frame_id_] = missing;
    }
    transport_sender_->ResendPackets(
        false, missing_frames_and_packets, true, rtt);
    latest_acked_frame_id_ = cast_feedback.ack_frame_id;
  }
}

bool VideoSender::AreTooManyFramesInFlight() const {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  int frames_in_flight = frames_in_encoder_;
  if (!last_send_time_.is_null()) {
    frames_in_flight +=
        static_cast<int32>(last_sent_frame_id_ - latest_acked_frame_id_);
  }
  VLOG(2) << frames_in_flight
          << " frames in flight; last sent: " << last_sent_frame_id_
          << " latest acked: " << latest_acked_frame_id_
          << " frames in encoder: " << frames_in_encoder_;
  return frames_in_flight >= max_unacked_frames_;
}

void VideoSender::ResendForKickstart() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!last_send_time_.is_null());
  VLOG(1) << "Resending last packet of frame " << last_sent_frame_id_
          << " to kick-start.";
  // Send the first packet of the last encoded frame to kick start
  // retransmission. This gives enough information to the receiver what
  // packets and frames are missing.
  MissingFramesAndPacketsMap missing_frames_and_packets;
  PacketIdSet missing;
  missing.insert(kRtcpCastLastPacket);
  missing_frames_and_packets.insert(
      std::make_pair(last_sent_frame_id_, missing));
  last_send_time_ = cast_environment_->Clock()->NowTicks();

  // Sending this extra packet is to kick-start the session. There is
  // no need to optimize re-transmission for this case.
  transport_sender_->ResendPackets(false, missing_frames_and_packets,
                                   false, rtt_);
}

}  // namespace cast
}  // namespace media
