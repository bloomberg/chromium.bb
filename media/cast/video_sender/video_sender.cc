// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_sender/video_sender.h"

#include <cstring>
#include <list>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/cast_defines.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/video_sender/external_video_encoder.h"
#include "media/cast/video_sender/video_encoder_impl.h"

namespace media {
namespace cast {

const int64 kMinSchedulingDelayMs = 1;

class LocalRtcpVideoSenderFeedback : public RtcpSenderFeedback {
 public:
  explicit LocalRtcpVideoSenderFeedback(VideoSender* video_sender)
      : video_sender_(video_sender) {}

  virtual void OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback)
      OVERRIDE {
    video_sender_->OnReceivedCastFeedback(cast_feedback);
  }

 private:
  VideoSender* video_sender_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocalRtcpVideoSenderFeedback);
};

VideoSender::VideoSender(
    scoped_refptr<CastEnvironment> cast_environment,
    const VideoSenderConfig& video_config,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
    const CastInitializationCallback& cast_initialization_cb,
    transport::CastTransportSender* const transport_sender)
    : rtp_max_delay_(base::TimeDelta::FromMilliseconds(
          video_config.rtp_config.max_delay_ms)),
      max_frame_rate_(video_config.max_frame_rate),
      cast_environment_(cast_environment),
      transport_sender_(transport_sender),
      rtp_timestamp_helper_(kVideoFrequency),
      rtcp_feedback_(new LocalRtcpVideoSenderFeedback(this)),
      last_acked_frame_id_(-1),
      last_sent_frame_id_(-1),
      frames_in_encoder_(0),
      duplicate_ack_(0),
      last_skip_count_(0),
      current_requested_bitrate_(video_config.start_bitrate),
      current_bitrate_divider_(1),
      congestion_control_(cast_environment->Clock(),
                          video_config.congestion_control_back_off,
                          video_config.max_bitrate,
                          video_config.min_bitrate,
                          video_config.start_bitrate),
      initialized_(false),
      active_session_(false),
      weak_factory_(this) {
  max_unacked_frames_ =
      1 + static_cast<uint8>(video_config.rtp_config.max_delay_ms *
                             max_frame_rate_ / 1000);
  VLOG(1) << "max_unacked_frames " << static_cast<int>(max_unacked_frames_);
  DCHECK_GT(max_unacked_frames_, 0) << "Invalid argument";

  if (video_config.use_external_encoder) {
    video_encoder_.reset(new ExternalVideoEncoder(cast_environment,
                                                  video_config,
                                                  create_vea_cb,
                                                  create_video_encode_mem_cb));
  } else {
    video_encoder_.reset(new VideoEncoderImpl(
        cast_environment, video_config, max_unacked_frames_));
  }


  media::cast::transport::CastTransportVideoConfig transport_config;
  transport_config.codec = video_config.codec;
  transport_config.rtp.config = video_config.rtp_config;
  transport_config.rtp.max_outstanding_frames = max_unacked_frames_ + 1;
  transport_sender_->InitializeVideo(transport_config);

  rtcp_.reset(
      new Rtcp(cast_environment_,
               rtcp_feedback_.get(),
               transport_sender_,
               NULL,  // paced sender.
               NULL,
               video_config.rtcp_mode,
               base::TimeDelta::FromMilliseconds(video_config.rtcp_interval),
               video_config.rtp_config.ssrc,
               video_config.incoming_feedback_ssrc,
               video_config.rtcp_c_name,
               false));
  rtcp_->SetCastReceiverEventHistorySize(kReceiverRtcpEventHistorySize);

  // TODO(pwestin): pass cast_initialization_cb to |video_encoder_|
  // and remove this call.
  cast_environment_->PostTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(cast_initialization_cb, STATUS_VIDEO_INITIALIZED));

  memset(frame_id_to_rtp_timestamp_, 0, sizeof(frame_id_to_rtp_timestamp_));
}

VideoSender::~VideoSender() {
}

void VideoSender::InitializeTimers() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!initialized_) {
    initialized_ = true;
    ScheduleNextResendCheck();
    ScheduleNextSkippedFramesCheck();
  }
}

void VideoSender::InsertRawVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& capture_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
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
      "rtp_timestamp", GetVideoRtpTimestamp(capture_time));

  if (video_encoder_->EncodeVideoFrame(
          video_frame,
          capture_time,
          base::Bind(&VideoSender::SendEncodedVideoFrameMainThread,
                     weak_factory_.GetWeakPtr()))) {
    frames_in_encoder_++;
    UpdateFramesInFlight();
  }
}

void VideoSender::SendEncodedVideoFrameMainThread(
    scoped_ptr<transport::EncodedVideoFrame> encoded_frame,
    const base::TimeTicks& capture_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  last_send_time_ = cast_environment_->Clock()->NowTicks();
  if (encoded_frame->key_frame) {
    VLOG(1) << "Send encoded key frame; frame_id:"
            << static_cast<int>(encoded_frame->frame_id);
  }

  DCHECK_GT(frames_in_encoder_, 0);
  frames_in_encoder_--;
  uint32 frame_id = encoded_frame->frame_id;
  cast_environment_->Logging()->InsertEncodedFrameEvent(
      last_send_time_, FRAME_ENCODED, VIDEO_EVENT, encoded_frame->rtp_timestamp,
      frame_id, static_cast<int>(encoded_frame->data.size()),
      encoded_frame->key_frame,
      current_requested_bitrate_);

  // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
  TRACE_EVENT_INSTANT1(
      "cast_perf_test", "VideoFrameEncoded",
      TRACE_EVENT_SCOPE_THREAD,
      "rtp_timestamp", GetVideoRtpTimestamp(capture_time));

  // Only use lowest 8 bits as key.
  frame_id_to_rtp_timestamp_[frame_id & 0xff] = encoded_frame->rtp_timestamp;

  last_sent_frame_id_ = static_cast<int>(encoded_frame->frame_id);
  rtp_timestamp_helper_.StoreLatestTime(capture_time,
                                        encoded_frame->rtp_timestamp);
  transport_sender_->InsertCodedVideoFrame(encoded_frame.get(), capture_time);
  UpdateFramesInFlight();
  InitializeTimers();
}

void VideoSender::IncomingRtcpPacket(scoped_ptr<Packet> packet) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  rtcp_->IncomingRtcpPacket(&packet->front(), packet->size());
}

void VideoSender::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_next = rtcp_->TimeToSendNextRtcpReport() -
                                 cast_environment_->Clock()->NowTicks();

  time_to_next = std::max(
      time_to_next, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&VideoSender::SendRtcpReport, weak_factory_.GetWeakPtr()),
      time_to_next);
}

void VideoSender::SendRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  const base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  uint32 now_as_rtp_timestamp = 0;
  if (rtp_timestamp_helper_.GetCurrentTimeAsRtpTimestamp(
          now, &now_as_rtp_timestamp)) {
    rtcp_->SendRtcpFromRtpSender(now, now_as_rtp_timestamp);
  }
  ScheduleNextRtcpReport();
}

void VideoSender::ScheduleNextResendCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_next;
  if (last_send_time_.is_null()) {
    time_to_next = rtp_max_delay_;
  } else {
    time_to_next = last_send_time_ - cast_environment_->Clock()->NowTicks() +
                   rtp_max_delay_;
  }
  time_to_next = std::max(
      time_to_next, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&VideoSender::ResendCheck, weak_factory_.GetWeakPtr()),
      time_to_next);
}

void VideoSender::ResendCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!last_send_time_.is_null() && last_sent_frame_id_ != -1) {
    base::TimeDelta time_since_last_send =
        cast_environment_->Clock()->NowTicks() - last_send_time_;
    if (time_since_last_send > rtp_max_delay_) {
      if (!active_session_) {
        // We have not received any acks, resend the first encoded frame (id 0),
        // which must also be a key frame.
        VLOG(1) << "ACK timeout resend first key frame";
        ResendFrame(0);
      } else {
        if (last_acked_frame_id_ == last_sent_frame_id_) {
          // Last frame acked, no point in doing anything
        } else {
          DCHECK_LE(0, last_acked_frame_id_);
          uint32 frame_id = static_cast<uint32>(last_acked_frame_id_ + 1);
          VLOG(1) << "ACK timeout resend frame:" << static_cast<int>(frame_id);
          ResendFrame(frame_id);
        }
      }
    }
  }
  ScheduleNextResendCheck();
}

void VideoSender::ScheduleNextSkippedFramesCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_next;
  if (last_checked_skip_count_time_.is_null()) {
    time_to_next =
        base::TimeDelta::FromMilliseconds(kSkippedFramesCheckPeriodkMs);
  } else {
    time_to_next =
        last_checked_skip_count_time_ - cast_environment_->Clock()->NowTicks() +
        base::TimeDelta::FromMilliseconds(kSkippedFramesCheckPeriodkMs);
  }
  time_to_next = std::max(
      time_to_next, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&VideoSender::SkippedFramesCheck, weak_factory_.GetWeakPtr()),
      time_to_next);
}

void VideoSender::SkippedFramesCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  int skip_count = video_encoder_->NumberOfSkippedFrames();
  if (skip_count - last_skip_count_ >
      kSkippedFramesThreshold * max_frame_rate_) {
    // TODO(pwestin): Propagate this up to the application.
  }
  last_skip_count_ = skip_count;
  last_checked_skip_count_time_ = cast_environment_->Clock()->NowTicks();
  ScheduleNextSkippedFramesCheck();
}

void VideoSender::OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta rtt;
  base::TimeDelta avg_rtt;
  base::TimeDelta min_rtt;
  base::TimeDelta max_rtt;

  if (rtcp_->Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt)) {
    // Don't use a RTT lower than our average.
    rtt = std::max(rtt, avg_rtt);
  } else {
    // We have no measured value use default.
    rtt = base::TimeDelta::FromMilliseconds(kStartRttMs);
  }
  if (cast_feedback.missing_frames_and_packets_.empty()) {
    // No lost packets.
    int resend_frame = -1;
    if (last_sent_frame_id_ == -1)
      return;

    video_encoder_->LatestFrameIdToReference(cast_feedback.ack_frame_id_);

    if (static_cast<uint32>(last_acked_frame_id_ + 1) ==
        cast_feedback.ack_frame_id_) {
      uint32 new_bitrate = 0;
      if (congestion_control_.OnAck(rtt, &new_bitrate)) {
        UpdateBitrate(new_bitrate);
      }
    }
    // We only count duplicate ACKs when we have sent newer frames.
    if (static_cast<uint32>(last_acked_frame_id_) ==
            cast_feedback.ack_frame_id_ &&
        IsNewerFrameId(last_sent_frame_id_, last_acked_frame_id_)) {
      duplicate_ack_++;
    } else {
      duplicate_ack_ = 0;
    }
    if (duplicate_ack_ >= 2 && duplicate_ack_ % 3 == 2) {
      // Resend last ACK + 1 frame.
      resend_frame = static_cast<uint32>(last_acked_frame_id_ + 1);
    }
    if (resend_frame != -1) {
      DCHECK_LE(0, resend_frame);
      VLOG(1) << "Received duplicate ACK for frame:"
              << static_cast<int>(resend_frame);
      ResendFrame(static_cast<uint32>(resend_frame));
    }
  } else {
    transport_sender_->ResendPackets(
        false, cast_feedback.missing_frames_and_packets_);
    uint32 new_bitrate = 0;
    if (congestion_control_.OnNack(rtt, &new_bitrate)) {
      UpdateBitrate(new_bitrate);
    }
  }
  ReceivedAck(cast_feedback.ack_frame_id_);
}

void VideoSender::ReceivedAck(uint32 acked_frame_id) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (acked_frame_id == UINT32_C(0xFFFFFFFF)) {
    // Receiver is sending a status message before any frames are ready to
    // be acked. Ignore.
    return;
  }
  // Start sending RTCP packets only after receiving the first ACK, i.e. only
  // after establishing that the receiver is active.
  if (last_acked_frame_id_ == -1) {
    ScheduleNextRtcpReport();
  }
  last_acked_frame_id_ = static_cast<int>(acked_frame_id);
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();

  RtpTimestamp rtp_timestamp =
      frame_id_to_rtp_timestamp_[acked_frame_id & 0xff];
  cast_environment_->Logging()->InsertFrameEvent(
      now, FRAME_ACK_RECEIVED, VIDEO_EVENT, rtp_timestamp, acked_frame_id);

  VLOG(2) << "ReceivedAck:" << static_cast<int>(acked_frame_id);
  active_session_ = true;
  DCHECK_NE(-1, last_acked_frame_id_);
  UpdateFramesInFlight();
}

void VideoSender::UpdateFramesInFlight() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (last_sent_frame_id_ != -1) {
    DCHECK_LE(0, last_sent_frame_id_);
    int frames_in_flight = 0;
    if (last_acked_frame_id_ != -1) {
      DCHECK_LE(0, last_acked_frame_id_);
      frames_in_flight = last_sent_frame_id_ - last_acked_frame_id_;
    } else {
      frames_in_flight = last_sent_frame_id_ + 1;
    }
    frames_in_flight += frames_in_encoder_;
    VLOG(2) << frames_in_flight
            << " Frames in flight; last sent: " << last_sent_frame_id_
            << " last acked:" << last_acked_frame_id_
            << " frames in encoder: " << frames_in_encoder_;
    if (frames_in_flight >= max_unacked_frames_) {
      video_encoder_->SkipNextFrame(true);
      return;
    } else if (frames_in_flight > max_unacked_frames_ * 4 / 5) {
      current_bitrate_divider_ = 3;
    } else if (frames_in_flight > max_unacked_frames_ * 2 / 3) {
      current_bitrate_divider_ = 2;
    } else {
      current_bitrate_divider_ = 1;
    }
    DCHECK(frames_in_flight <= max_unacked_frames_);
  }
  video_encoder_->SkipNextFrame(false);
}

void VideoSender::ResendFrame(uint32 resend_frame_id) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  MissingFramesAndPacketsMap missing_frames_and_packets;
  PacketIdSet missing;
  missing_frames_and_packets.insert(std::make_pair(resend_frame_id, missing));
  last_send_time_ = cast_environment_->Clock()->NowTicks();
  transport_sender_->ResendPackets(false, missing_frames_and_packets);
}

void VideoSender::UpdateBitrate(int new_bitrate) {
  new_bitrate /= current_bitrate_divider_;
  // Make sure we don't set the bitrate too insanely low.
  DCHECK_GT(new_bitrate, 1000);
  video_encoder_->SetBitRate(new_bitrate);
  current_requested_bitrate_ = new_bitrate;
}

}  // namespace cast
}  // namespace media
