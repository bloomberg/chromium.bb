// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_sender/video_sender.h"

#include <list>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/cast/cast_defines.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/video_sender/video_encoder.h"

namespace media {
namespace cast {

const int64 kMinSchedulingDelayMs = 1;

class LocalRtcpVideoSenderFeedback : public RtcpSenderFeedback {
 public:
  explicit LocalRtcpVideoSenderFeedback(VideoSender* video_sender)
      : video_sender_(video_sender) {
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
    scoped_refptr<CastEnvironment> cast_environment,
    const VideoSenderConfig& video_config,
    VideoEncoderController* const video_encoder_controller,
    PacedPacketSender* const paced_packet_sender)
    : rtp_max_delay_(
          base::TimeDelta::FromMilliseconds(video_config.rtp_max_delay_ms)),
      max_frame_rate_(video_config.max_frame_rate),
      cast_environment_(cast_environment),
      rtcp_feedback_(new LocalRtcpVideoSenderFeedback(this)),
      rtp_sender_(new RtpSender(cast_environment, NULL, &video_config,
                                paced_packet_sender)),
      last_acked_frame_id_(-1),
      last_sent_frame_id_(-1),
      duplicate_ack_(0),
      last_skip_count_(0),
      congestion_control_(cast_environment->Clock(),
                          video_config.congestion_control_back_off,
                          video_config.max_bitrate,
                          video_config.min_bitrate,
                          video_config.start_bitrate),
      initialized_(false),
      weak_factory_(this) {
  max_unacked_frames_ = static_cast<uint8>(video_config.rtp_max_delay_ms *
      video_config.max_frame_rate / 1000) + 1;
  VLOG(1) << "max_unacked_frames " << static_cast<int>(max_unacked_frames_);
  DCHECK_GT(max_unacked_frames_, 0) << "Invalid argument";

  rtp_video_sender_statistics_.reset(
      new LocalRtpVideoSenderStatistics(rtp_sender_.get()));

  if (video_config.use_external_encoder) {
    DCHECK(video_encoder_controller) << "Invalid argument";
    video_encoder_controller_ = video_encoder_controller;
  } else {
    video_encoder_.reset(new VideoEncoder(cast_environment, video_config,
        max_unacked_frames_));
    video_encoder_controller_ = video_encoder_.get();
  }

  if (video_config.aes_iv_mask.size() == kAesKeySize &&
      video_config.aes_key.size() == kAesKeySize) {
    iv_mask_ = video_config.aes_iv_mask;
    crypto::SymmetricKey* key = crypto::SymmetricKey::Import(
        crypto::SymmetricKey::AES, video_config.aes_key);
    encryptor_.reset(new crypto::Encryptor());
    encryptor_->Init(key, crypto::Encryptor::CTR, std::string());
  } else if (video_config.aes_iv_mask.size() != 0 ||
             video_config.aes_key.size() != 0) {
    DCHECK(false) << "Invalid crypto configuration";
  }

  rtcp_.reset(new Rtcp(
      cast_environment_,
      rtcp_feedback_.get(),
      paced_packet_sender,
      rtp_video_sender_statistics_.get(),
      NULL,
      video_config.rtcp_mode,
      base::TimeDelta::FromMilliseconds(video_config.rtcp_interval),
      video_config.sender_ssrc,
      video_config.incoming_feedback_ssrc,
      video_config.rtcp_c_name));
}

VideoSender::~VideoSender() {}

void VideoSender::InitializeTimers() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!initialized_) {
    initialized_ = true;
    ScheduleNextRtcpReport();
    ScheduleNextResendCheck();
    ScheduleNextSkippedFramesCheck();
  }
}

void VideoSender::InsertRawVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& capture_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(video_encoder_.get()) << "Invalid state";
  cast_environment_->Logging()->InsertFrameEvent(kVideoFrameReceived,
      GetVideoRtpTimestamp(capture_time), kFrameIdUnknown);

  if (!video_encoder_->EncodeVideoFrame(video_frame, capture_time,
      base::Bind(&VideoSender::SendEncodedVideoFrameMainThread,
          weak_factory_.GetWeakPtr()))) {
  }
}

void VideoSender::InsertCodedVideoFrame(const EncodedVideoFrame* encoded_frame,
                                        const base::TimeTicks& capture_time,
                                        const base::Closure callback) {
  DCHECK(!video_encoder_.get()) << "Invalid state";
  DCHECK(encoded_frame) << "Invalid argument";
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  SendEncodedVideoFrame(encoded_frame, capture_time);
  cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE, callback);
}

void VideoSender::SendEncodedVideoFrameMainThread(
    scoped_ptr<EncodedVideoFrame> video_frame,
    const base::TimeTicks& capture_time) {
  SendEncodedVideoFrame(video_frame.get(), capture_time);
}

bool VideoSender::EncryptVideoFrame(const EncodedVideoFrame& video_frame,
                                    EncodedVideoFrame* encrypted_frame) {
  DCHECK(encryptor_) << "Invalid state";

  if (!encryptor_->SetCounter(GetAesNonce(video_frame.frame_id, iv_mask_))) {
    NOTREACHED() << "Failed to set counter";
    return false;
  }

  if (!encryptor_->Encrypt(video_frame.data, &encrypted_frame->data)) {
    NOTREACHED() << "Encrypt error";
    return false;
  }
  encrypted_frame->codec = video_frame.codec;
  encrypted_frame->key_frame = video_frame.key_frame;
  encrypted_frame->frame_id = video_frame.frame_id;
  encrypted_frame->last_referenced_frame_id =
      video_frame.last_referenced_frame_id;
  return true;
}

void VideoSender::SendEncodedVideoFrame(const EncodedVideoFrame* encoded_frame,
                                        const base::TimeTicks& capture_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  last_send_time_ = cast_environment_->Clock()->NowTicks();

  if (encryptor_) {
    EncodedVideoFrame encrypted_video_frame;

    if (!EncryptVideoFrame(*encoded_frame, &encrypted_video_frame)) {
      // Logging already done.
      return;
    }
    rtp_sender_->IncomingEncodedVideoFrame(&encrypted_video_frame,
                                           capture_time);
  } else {
    rtp_sender_->IncomingEncodedVideoFrame(encoded_frame, capture_time);
  }
  if (encoded_frame->key_frame) {
    VLOG(1) << "Send encoded key frame; frame_id:"
            << static_cast<int>(encoded_frame->frame_id);
  }
  last_sent_frame_id_ = static_cast<int>(encoded_frame->frame_id);
  UpdateFramesInFlight();
  InitializeTimers();
}

void VideoSender::IncomingRtcpPacket(const uint8* packet, size_t length,
                                     const base::Closure callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  rtcp_->IncomingRtcpPacket(packet, length);
  cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE, callback);
}

void VideoSender::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_next = rtcp_->TimeToSendNextRtcpReport() -
      cast_environment_->Clock()->NowTicks();

  time_to_next = std::max(time_to_next,
      base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(CastEnvironment::MAIN, FROM_HERE,
      base::Bind(&VideoSender::SendRtcpReport, weak_factory_.GetWeakPtr()),
                 time_to_next);
}

void VideoSender::SendRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  RtcpSenderLogMessage sender_log_message;
  const FrameRawMap& frame_raw_map =
      cast_environment_->Logging()->GetFrameRawData();

  FrameRawMap::const_iterator it = frame_raw_map.begin();
  while (it != frame_raw_map.end()) {
    RtcpSenderFrameLogMessage frame_message;
    frame_message.rtp_timestamp = it->first;
    frame_message.frame_status = kRtcpSenderFrameStatusUnknown;
    if (it->second.type.empty()) {
      ++it;
      continue;
    }
    CastLoggingEvent last_event = it->second.type.back();
    switch (last_event) {
      case kVideoFrameCaptured:
        frame_message.frame_status = kRtcpSenderFrameStatusDroppedByFlowControl;
        break;
      case kVideoFrameSentToEncoder:
        frame_message.frame_status = kRtcpSenderFrameStatusDroppedByEncoder;
        break;
      case kVideoFrameEncoded:
        frame_message.frame_status = kRtcpSenderFrameStatusSentToNetwork;
        break;
      default:
        ++it;
        continue;
    }
    ++it;
    if (it == frame_raw_map.end()) {
      // Last message on our map; only send if it is kVideoFrameEncoded.
      if (last_event != kVideoFrameEncoded) {
        // For other events we will wait for it to finish and report the result
        // in the next report.
        break;
      }
    }
    sender_log_message.push_back(frame_message);
  }
  rtcp_->SendRtcpFromRtpSender(&sender_log_message);
  if (!sender_log_message.empty()) {
    VLOG(1) << "Failed to send all log messages";
  }

  // TODO(pwestin): When we start pulling out the logging by other means we need
  // to synchronize this.
  cast_environment_->Logging()->Reset();
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
  time_to_next = std::max(time_to_next,
      base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(CastEnvironment::MAIN, FROM_HERE,
      base::Bind(&VideoSender::ResendCheck, weak_factory_.GetWeakPtr()),
                 time_to_next);
}

void VideoSender::ResendCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!last_send_time_.is_null() && last_sent_frame_id_ != -1) {
    base::TimeDelta time_since_last_send =
       cast_environment_->Clock()->NowTicks() - last_send_time_;
    if (time_since_last_send > rtp_max_delay_) {
      if (last_acked_frame_id_ == -1) {
        // We have not received any ack, send a key frame.
        video_encoder_controller_->GenerateKeyFrame();
        last_acked_frame_id_ = -1;
        last_sent_frame_id_ = -1;
        UpdateFramesInFlight();
      } else {
        DCHECK_LE(0, last_acked_frame_id_);

        uint32 frame_id = static_cast<uint32>(last_acked_frame_id_ + 1);
        VLOG(1) << "ACK timeout resend frame:" << static_cast<int>(frame_id);
        ResendFrame(frame_id);
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
    time_to_next = last_checked_skip_count_time_ -
        cast_environment_->Clock()->NowTicks() +
        base::TimeDelta::FromMilliseconds(kSkippedFramesCheckPeriodkMs);
  }
  time_to_next = std::max(time_to_next,
      base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(CastEnvironment::MAIN, FROM_HERE,
      base::Bind(&VideoSender::SkippedFramesCheck, weak_factory_.GetWeakPtr()),
                 time_to_next);
}

void VideoSender::SkippedFramesCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  int skip_count = video_encoder_controller_->NumberOfSkippedFrames();
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
    cast_environment_->Logging()->InsertGenericEvent(kRttMs,
        rtt.InMilliseconds());
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

    if (static_cast<uint32>(last_acked_frame_id_ + 1) ==
        cast_feedback.ack_frame_id_) {
      uint32 new_bitrate = 0;
      if (congestion_control_.OnAck(rtt, &new_bitrate)) {
        video_encoder_controller_->SetBitRate(new_bitrate);
      }
    }
    if (static_cast<uint32>(last_acked_frame_id_) == cast_feedback.ack_frame_id_
        // We only count duplicate ACKs when we have sent newer frames.
        && IsNewerFrameId(last_sent_frame_id_, last_acked_frame_id_)) {
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
    rtp_sender_->ResendPackets(cast_feedback.missing_frames_and_packets_);
    last_send_time_ = cast_environment_->Clock()->NowTicks();

    uint32 new_bitrate = 0;
    if (congestion_control_.OnNack(rtt, &new_bitrate)) {
      video_encoder_controller_->SetBitRate(new_bitrate);
    }
  }
  ReceivedAck(cast_feedback.ack_frame_id_);
}

void VideoSender::ReceivedAck(uint32 acked_frame_id) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  last_acked_frame_id_ = static_cast<int>(acked_frame_id);
  cast_environment_->Logging()->InsertGenericEvent(kAckReceived,
                                                   acked_frame_id);
  VLOG(1) << "ReceivedAck:" << static_cast<int>(acked_frame_id);
  last_acked_frame_id_ = acked_frame_id;
  UpdateFramesInFlight();
}

void VideoSender::UpdateFramesInFlight() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (last_sent_frame_id_ != -1) {
    DCHECK_LE(0, last_sent_frame_id_);
    uint32 frames_in_flight;
    if (last_acked_frame_id_ != -1) {
      DCHECK_LE(0, last_acked_frame_id_);
      frames_in_flight = static_cast<uint32>(last_sent_frame_id_) -
                         static_cast<uint32>(last_acked_frame_id_);
    } else {
      frames_in_flight = static_cast<uint32>(last_sent_frame_id_) + 1;
    }
    VLOG(1) << "Frames in flight; last sent: " << last_sent_frame_id_
            << " last acked:" << last_acked_frame_id_;
    if (frames_in_flight >= max_unacked_frames_) {
      video_encoder_controller_->SkipNextFrame(true);
      return;
    }
  }
  video_encoder_controller_->SkipNextFrame(false);
}

void VideoSender::ResendFrame(uint32 resend_frame_id) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  MissingFramesAndPacketsMap missing_frames_and_packets;
  PacketIdSet missing;
  missing_frames_and_packets.insert(std::make_pair(resend_frame_id, missing));
  rtp_sender_->ResendPackets(missing_frames_and_packets);
  last_send_time_ = cast_environment_->Clock()->NowTicks();
}

}  // namespace cast
}  // namespace media
