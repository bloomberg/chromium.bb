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
#include "media/cast/rtcp/sender_rtcp_event_subscriber.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/video_sender/external_video_encoder.h"
#include "media/cast/video_sender/video_encoder_impl.h"

namespace media {
namespace cast {

const int64 kMinSchedulingDelayMs = 1;

// This is the maxmimum number of sender frame log messages that can fit in a
// single RTCP packet.
const int64 kMaxEventSubscriberEntries =
    (kMaxIpPacketSize - kRtcpCastLogHeaderSize) / kRtcpSenderFrameLogSize;

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
      event_subscriber_(kMaxEventSubscriberEntries),
      rtp_stats_(kVideoFrequency),
      rtcp_feedback_(new LocalRtcpVideoSenderFeedback(this)),
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

  rtcp_.reset(
      new Rtcp(cast_environment_,
               rtcp_feedback_.get(),
               transport_sender_,
               NULL,  // paced sender.
               NULL,
               video_config.rtcp_mode,
               base::TimeDelta::FromMilliseconds(video_config.rtcp_interval),
               video_config.sender_ssrc,
               video_config.incoming_feedback_ssrc,
               video_config.rtcp_c_name));
  rtcp_->SetCastReceiverEventHistorySize(kReceiverRtcpEventHistorySize);

  // TODO(pwestin): pass cast_initialization_cb to |video_encoder_|
  // and remove this call.
  cast_environment_->PostTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(cast_initialization_cb, STATUS_VIDEO_INITIALIZED));
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber_);

  memset(frame_id_to_rtp_timestamp_, 0, sizeof(frame_id_to_rtp_timestamp_));

  transport_sender_->SubscribeVideoRtpStatsCallback(
      base::Bind(&VideoSender::StoreStatistics, weak_factory_.GetWeakPtr()));
}

VideoSender::~VideoSender() {
  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber_);
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
      capture_time, kVideoFrameCaptured, rtp_timestamp, kFrameIdUnknown);
  cast_environment_->Logging()->InsertFrameEvent(
      cast_environment_->Clock()->NowTicks(),
      kVideoFrameReceived,
      rtp_timestamp,
      kFrameIdUnknown);

  // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
  TRACE_EVENT_INSTANT2(
      "cast_perf_test", "InsertRawVideoFrame",
      TRACE_EVENT_SCOPE_THREAD,
      "timestamp", capture_time.ToInternalValue(),
      "rtp_timestamp", GetVideoRtpTimestamp(capture_time));

  if (!video_encoder_->EncodeVideoFrame(
          video_frame,
          capture_time,
          base::Bind(&VideoSender::SendEncodedVideoFrameMainThread,
                     weak_factory_.GetWeakPtr()))) {
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

  uint32 frame_id = encoded_frame->frame_id;
  cast_environment_->Logging()->InsertFrameEvent(last_send_time_,
                                                 kVideoFrameEncoded,
                                                 encoded_frame->rtp_timestamp,
                                                 frame_id);

  // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
  TRACE_EVENT_INSTANT1(
      "cast_perf_test", "VideoFrameEncoded",
      TRACE_EVENT_SCOPE_THREAD,
      "rtp_timestamp", GetVideoRtpTimestamp(capture_time));

  // Only use lowest 8 bits as key.
  frame_id_to_rtp_timestamp_[frame_id & 0xff] = encoded_frame->rtp_timestamp;

  last_sent_frame_id_ = static_cast<int>(encoded_frame->frame_id);
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

void VideoSender::StoreStatistics(
    const transport::RtcpSenderInfo& sender_info,
    base::TimeTicks time_sent,
    uint32 rtp_timestamp) {
  rtp_stats_.Store(sender_info, time_sent, rtp_timestamp);
}

void VideoSender::SendRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  transport::RtcpSenderLogMessage sender_log_message;
  RtcpEventMap rtcp_events;
  event_subscriber_.GetRtcpEventsAndReset(&rtcp_events);

  for (RtcpEventMap::iterator it = rtcp_events.begin(); it != rtcp_events.end();
       ++it) {
    CastLoggingEvent event_type = it->second.type;
    if (event_type == kVideoFrameCaptured ||
        event_type == kVideoFrameSentToEncoder ||
        event_type == kVideoFrameEncoded) {
      transport::RtcpSenderFrameLogMessage frame_message;
      frame_message.rtp_timestamp = it->first;
      switch (event_type) {
        case kVideoFrameCaptured:
          frame_message.frame_status =
              transport::kRtcpSenderFrameStatusDroppedByFlowControl;
          break;
        case kVideoFrameSentToEncoder:
          frame_message.frame_status =
              transport::kRtcpSenderFrameStatusDroppedByEncoder;
          break;
        case kVideoFrameEncoded:
          frame_message.frame_status =
              transport::kRtcpSenderFrameStatusSentToNetwork;
          break;
        default:
          NOTREACHED();
          break;
      }
      sender_log_message.push_back(frame_message);
    } else {
      // This shouldn't happen because RtcpEventMap isn't supposed to contain
      // other event types.
      NOTREACHED() << "Got unknown event type in RtcpEventMap: " << event_type;
    }
  }

  rtp_stats_.UpdateInfo(cast_environment_->Clock()->NowTicks());

  rtcp_->SendRtcpFromRtpSender(sender_log_message, rtp_stats_.sender_info());
  if (!sender_log_message.empty()) {
    VLOG(1) << "Failed to send all log messages";
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
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();

  // Update delay and max number of frames in flight based on the the new
  // received target delay.
  rtp_max_delay_ =
      base::TimeDelta::FromMilliseconds(cast_feedback.target_delay_ms_);
  max_unacked_frames_ = 1 + static_cast<uint8>(cast_feedback.target_delay_ms_ *
                                               max_frame_rate_ / 1000);
  if (rtcp_->Rtt(&rtt, &avg_rtt, &min_rtt, &max_rtt)) {
    cast_environment_->Logging()->InsertGenericEvent(
        now, kRttMs, rtt.InMilliseconds());
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
        video_encoder_->SetBitRate(new_bitrate);
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
      video_encoder_->SetBitRate(new_bitrate);
    }
  }
  ReceivedAck(cast_feedback.ack_frame_id_);
}

void VideoSender::ReceivedAck(uint32 acked_frame_id) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
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
      now, kVideoAckReceived, rtp_timestamp, acked_frame_id);

  VLOG(2) << "ReceivedAck:" << static_cast<int>(acked_frame_id);
  last_acked_frame_id_ = acked_frame_id;
  active_session_ = true;
  UpdateFramesInFlight();
}

void VideoSender::UpdateFramesInFlight() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (last_sent_frame_id_ != -1) {
    DCHECK_LE(0, last_sent_frame_id_);
    uint32 frames_in_flight = 0;
    if (last_acked_frame_id_ != -1) {
      DCHECK_LE(0, last_acked_frame_id_);
      frames_in_flight = static_cast<uint32>(last_sent_frame_id_) -
                         static_cast<uint32>(last_acked_frame_id_);
    } else {
      frames_in_flight = static_cast<uint32>(last_sent_frame_id_) + 1;
    }
    VLOG(2) << frames_in_flight
            << " Frames in flight; last sent: " << last_sent_frame_id_
            << " last acked:" << last_acked_frame_id_;
    if (frames_in_flight >= max_unacked_frames_) {
      video_encoder_->SkipNextFrame(true);
      return;
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

}  // namespace cast
}  // namespace media
