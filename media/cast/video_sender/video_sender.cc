// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_sender/video_sender.h"

#include <list>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/cast_defines.h"
#include "media/cast/pacing/paced_sender.h"
#include "media/cast/video_sender/video_encoder.h"

namespace media {
namespace cast {

const int64 kMinSchedulingDelayMs = 1;

class LocalRtcpVideoSenderFeedback : public RtcpSenderFeedback {
 public:
  explicit LocalRtcpVideoSenderFeedback(VideoSender* video_sender)
      : video_sender_(video_sender) {
  }

  virtual void OnReceivedSendReportRequest() OVERRIDE {}

  virtual void OnReceivedReportBlock(
      const RtcpReportBlock& report_block) OVERRIDE {}

  virtual void OnReceivedRpsi(uint8 payload_type,
                              uint64 picture_id) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void OnReceivedRemb(uint32 bitrate) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void OnReceivedNackRequest(
      const std::list<uint16>& nack_sequence_numbers) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void OnReceivedIntraFrameRequest() OVERRIDE {
    video_sender_->OnReceivedIntraFrameRequest();
  }

  virtual void OnReceivedCastFeedback(
      const RtcpCastMessage& cast_feedback) OVERRIDE {
    video_sender_->OnReceivedCastFeedback(cast_feedback);
  }

 private:
  VideoSender* video_sender_;
};

class LocalRtpVideoSenderStatistics : public RtpSenderStatistics {
 public:
  explicit LocalRtpVideoSenderStatistics(RtpSender* rtp_sender)
     : rtp_sender_(rtp_sender) {
  }

  virtual void GetStatistics(const base::TimeTicks& now,
                             RtcpSenderInfo* sender_info) OVERRIDE {
    rtp_sender_->RtpStatistics(now, sender_info);
  }

 private:
  RtpSender* rtp_sender_;
};

VideoSender::VideoSender(
    scoped_refptr<CastThread> cast_thread,
    const VideoSenderConfig& video_config,
    VideoEncoderController* const video_encoder_controller,
    PacedPacketSender* const paced_packet_sender)
    : incoming_feedback_ssrc_(video_config.incoming_feedback_ssrc),
      rtp_max_delay_(
          base::TimeDelta::FromMilliseconds(video_config.rtp_max_delay_ms)),
      max_frame_rate_(video_config.max_frame_rate),
      cast_thread_(cast_thread),
      rtcp_feedback_(new LocalRtcpVideoSenderFeedback(this)),
      rtp_sender_(new RtpSender(NULL, &video_config, paced_packet_sender)),
      last_acked_frame_id_(-1),
      last_sent_frame_id_(-1),
      last_sent_key_frame_id_(-1),
      duplicate_ack_(0),
      last_skip_count_(0),
      congestion_control_(video_config.congestion_control_back_off,
                          video_config.max_bitrate,
                          video_config.min_bitrate,
                          video_config.start_bitrate),
      clock_(&default_tick_clock_),
      weak_factory_(this) {
  max_unacked_frames_ = static_cast<uint8>(video_config.rtp_max_delay_ms *
      video_config.max_frame_rate / 1000);
  DCHECK(max_unacked_frames_ > 0) << "Invalid argument";

  rtp_video_sender_statistics_.reset(
      new LocalRtpVideoSenderStatistics(rtp_sender_.get()));

  if (video_config.use_external_encoder) {
    DCHECK(video_encoder_controller) << "Invalid argument";
    video_encoder_controller_ = video_encoder_controller;
  } else {
    video_encoder_ = new VideoEncoder(cast_thread, video_config,
        max_unacked_frames_);
    video_encoder_controller_ = video_encoder_.get();
  }
  rtcp_.reset(new Rtcp(
      rtcp_feedback_.get(),
      paced_packet_sender,
      rtp_video_sender_statistics_.get(),
      NULL,
      video_config.rtcp_mode,
      base::TimeDelta::FromMilliseconds(video_config.rtcp_interval),
      true,
      video_config.sender_ssrc,
      video_config.rtcp_c_name));

  rtcp_->SetRemoteSSRC(video_config.incoming_feedback_ssrc);
  ScheduleNextRtcpReport();
  ScheduleNextResendCheck();
  ScheduleNextSkippedFramesCheck();
}

VideoSender::~VideoSender() {}

void VideoSender::InsertRawVideoFrame(
    const I420VideoFrame* video_frame,
    const base::TimeTicks& capture_time,
    const base::Closure callback) {
  DCHECK(video_encoder_.get()) << "Invalid state";

  if (!video_encoder_->EncodeVideoFrame(video_frame, capture_time,
      base::Bind(&VideoSender::SendEncodedVideoFrameMainThread,
          weak_factory_.GetWeakPtr()), callback)) {
    VLOG(1) << "Failed to InsertRawVideoFrame";
  }
}

void VideoSender::InsertCodedVideoFrame(const EncodedVideoFrame* encoded_frame,
                                        const base::TimeTicks& capture_time,
                                        const base::Closure callback) {
  DCHECK(!video_encoder_.get()) << "Invalid state";
  DCHECK(encoded_frame) << "Invalid argument";

  SendEncodedVideoFrame(encoded_frame, capture_time);
  callback.Run();
}

void VideoSender::SendEncodedVideoFrameMainThread(
    scoped_ptr<EncodedVideoFrame> video_frame,
    const base::TimeTicks& capture_time) {
  SendEncodedVideoFrame(video_frame.get(), capture_time);
}

void VideoSender::SendEncodedVideoFrame(const EncodedVideoFrame* encoded_frame,
                                        const base::TimeTicks& capture_time) {
  last_send_time_ = clock_->NowTicks();
  rtp_sender_->IncomingEncodedVideoFrame(encoded_frame, capture_time);
  if (encoded_frame->key_frame) {
    last_sent_key_frame_id_ = encoded_frame->frame_id;
  }
  last_sent_frame_id_ = encoded_frame->frame_id;
  UpdateFramesInFlight();
}

void VideoSender::OnReceivedIntraFrameRequest() {
  if (last_sent_key_frame_id_ != -1) {
    uint8 frames_in_flight = static_cast<uint8>(last_sent_frame_id_) -
                             static_cast<uint8>(last_sent_key_frame_id_);
    if (frames_in_flight < (max_unacked_frames_ - 1)) return;
  }
  video_encoder_controller_->GenerateKeyFrame();
  last_acked_frame_id_ = -1;
  last_sent_frame_id_ = -1;
}

void VideoSender::IncomingRtcpPacket(const uint8* packet, int length) {
  rtcp_->IncomingRtcpPacket(packet, length);
}

void VideoSender::ScheduleNextRtcpReport() {
  base::TimeDelta time_to_next =
      rtcp_->TimeToSendNextRtcpReport() - clock_->NowTicks();

  time_to_next = std::max(time_to_next,
      base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_thread_->PostDelayedTask(CastThread::MAIN, FROM_HERE,
      base::Bind(&VideoSender::SendRtcpReport, weak_factory_.GetWeakPtr()),
                 time_to_next);
}

void VideoSender::SendRtcpReport() {
  rtcp_->SendRtcpReport(incoming_feedback_ssrc_);
  ScheduleNextRtcpReport();
}

void VideoSender::ScheduleNextResendCheck() {
  base::TimeDelta time_to_next;
  if (last_send_time_.is_null()) {
    time_to_next = rtp_max_delay_;
  } else {
    time_to_next = last_send_time_ - clock_->NowTicks() + rtp_max_delay_;
  }
  time_to_next = std::max(time_to_next,
      base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_thread_->PostDelayedTask(CastThread::MAIN, FROM_HERE,
      base::Bind(&VideoSender::ResendCheck, weak_factory_.GetWeakPtr()),
                 time_to_next);
}

void VideoSender::ResendCheck() {
  if (!last_send_time_.is_null() && last_sent_frame_id_ != -1) {
    base::TimeDelta time_to_next =
        last_send_time_ - clock_->NowTicks() + rtp_max_delay_;

    if (last_acked_frame_id_ == -1) {
      // We have not received any ack, send a key frame.
      video_encoder_controller_->GenerateKeyFrame();
      last_acked_frame_id_ = -1;
      last_sent_frame_id_ = -1;
      UpdateFramesInFlight();
    } else {
      ResendFrame(static_cast<uint8>(last_acked_frame_id_ + 1));
    }
  }
  ScheduleNextResendCheck();
}

void VideoSender::ScheduleNextSkippedFramesCheck() {
  base::TimeDelta time_to_next;
  if (last_checked_skip_count_time_.is_null()) {
    time_to_next =
        base::TimeDelta::FromMilliseconds(kSkippedFramesCheckPeriodkMs);
  } else {
    time_to_next = last_checked_skip_count_time_ - clock_->NowTicks() +
         base::TimeDelta::FromMilliseconds(kSkippedFramesCheckPeriodkMs);
  }
  time_to_next = std::max(time_to_next,
      base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_thread_->PostDelayedTask(CastThread::MAIN, FROM_HERE,
      base::Bind(&VideoSender::SkippedFramesCheck, weak_factory_.GetWeakPtr()),
                 time_to_next);
}

void VideoSender::SkippedFramesCheck() {
  int skip_count = video_encoder_controller_->NumberOfSkippedFrames();
  if (skip_count - last_skip_count_ >
      kSkippedFramesThreshold * max_frame_rate_) {
      // TODO(pwestin): Propagate this up to the application.
  }
  last_skip_count_ = skip_count;
  last_checked_skip_count_time_ = clock_->NowTicks();
  ScheduleNextSkippedFramesCheck();
}

void VideoSender::OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback) {
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
    if (last_sent_frame_id_ == -1) return;

    video_encoder_controller_->LatestFrameIdToReference(
        cast_feedback.ack_frame_id_);

    if (static_cast<uint8>(last_acked_frame_id_ + 1) ==
        cast_feedback.ack_frame_id_) {
      uint32 new_bitrate = 0;
      if (congestion_control_.OnAck(rtt, &new_bitrate)) {
        video_encoder_controller_->SetBitRate(new_bitrate);
      }
    }
    if (last_acked_frame_id_ == cast_feedback.ack_frame_id_ &&
        // We only count duplicate ACKs when we have sent newer frames.
        IsNewerFrameId(last_sent_frame_id_, last_acked_frame_id_)) {
      duplicate_ack_++;
    } else {
      duplicate_ack_ = 0;
    }
    if (duplicate_ack_ >= 2 && duplicate_ack_ % 3 == 2) {
      // Resend last ACK + 1 frame.
      resend_frame = static_cast<uint8>(last_acked_frame_id_ + 1);
    }
    if (resend_frame != -1) {
      ResendFrame(static_cast<uint8>(resend_frame));
    }
  } else {
    rtp_sender_->ResendPackets(cast_feedback.missing_frames_and_packets_);
    last_send_time_ = clock_->NowTicks();

    uint32 new_bitrate = 0;
    if (congestion_control_.OnNack(rtt, &new_bitrate)) {
      video_encoder_controller_->SetBitRate(new_bitrate);
    }
  }
  ReceivedAck(cast_feedback.ack_frame_id_);
}

void VideoSender::ReceivedAck(uint8 acked_frame_id) {
  last_acked_frame_id_ = acked_frame_id;
  UpdateFramesInFlight();
}

void VideoSender::UpdateFramesInFlight() {
  if (last_sent_frame_id_ != -1) {
    uint8 frames_in_flight = static_cast<uint8>(last_sent_frame_id_) -
                             static_cast<uint8>(last_acked_frame_id_);
    if (frames_in_flight >= max_unacked_frames_) {
      video_encoder_controller_->SkipNextFrame(true);
      return;
    }
  }
  video_encoder_controller_->SkipNextFrame(false);
}

void VideoSender::ResendFrame(uint8 resend_frame_id) {
  MissingFramesAndPacketsMap missing_frames_and_packets;
  PacketIdSet missing;
  missing_frames_and_packets.insert(std::make_pair(resend_frame_id, missing));
  rtp_sender_->ResendPackets(missing_frames_and_packets);
  last_send_time_ = clock_->NowTicks();
}

}  // namespace cast
}  // namespace media
