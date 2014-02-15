// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_sender/video_sender.h"

#include <list>

#include "base/bind.h"
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

class LocalRtpVideoSenderStatistics : public RtpSenderStatistics {
 public:
  explicit LocalRtpVideoSenderStatistics(
      transport::CastTransportSender* const transport_sender)
      : transport_sender_(transport_sender), sender_info_(), rtp_timestamp_(0) {
    transport_sender_->SubscribeVideoRtpStatsCallback(
        base::Bind(&LocalRtpVideoSenderStatistics::StoreStatistics,
                   base::Unretained(this)));
  }

  virtual void GetStatistics(const base::TimeTicks& now,
                             transport::RtcpSenderInfo* sender_info) OVERRIDE {
    // Update RTP timestamp and return last stored statistics.
    uint32 ntp_seconds = 0;
    uint32 ntp_fraction = 0;
    uint32 rtp_timestamp = 0;
    if (rtp_timestamp_ > 0) {
      base::TimeDelta time_since_last_send = now - time_sent_;
      rtp_timestamp = rtp_timestamp_ + time_since_last_send.InMilliseconds() *
                                           (kVideoFrequency / 1000);
      // Update NTP time to current time.
      ConvertTimeTicksToNtp(now, &ntp_seconds, &ntp_fraction);
    }
    // Populate sender info.
    sender_info->rtp_timestamp = rtp_timestamp;
    sender_info->ntp_seconds = ntp_seconds;
    sender_info->ntp_fraction = ntp_fraction;
    sender_info->send_packet_count = sender_info_.send_packet_count;
    sender_info->send_octet_count = sender_info_.send_octet_count;
  }

  void StoreStatistics(const transport::RtcpSenderInfo& sender_info,
                       base::TimeTicks time_sent,
                       uint32 rtp_timestamp) {
    sender_info_ = sender_info;
    time_sent_ = time_sent;
    rtp_timestamp_ = rtp_timestamp;
  }

 private:
  transport::CastTransportSender* const transport_sender_;
  transport::RtcpSenderInfo sender_info_;
  base::TimeTicks time_sent_;
  uint32 rtp_timestamp_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocalRtpVideoSenderStatistics);
};

VideoSender::VideoSender(
    scoped_refptr<CastEnvironment> cast_environment,
    const VideoSenderConfig& video_config,
    const scoped_refptr<GpuVideoAcceleratorFactories>& gpu_factories,
    const CastInitializationCallback& initialization_status,
    transport::CastTransportSender* const transport_sender)
    : rtp_max_delay_(base::TimeDelta::FromMilliseconds(
          video_config.rtp_config.max_delay_ms)),
      max_frame_rate_(video_config.max_frame_rate),
      cast_environment_(cast_environment),
      transport_sender_(transport_sender),
      event_subscriber_(kMaxEventSubscriberEntries),
      rtcp_feedback_(new LocalRtcpVideoSenderFeedback(this)),
      last_acked_frame_id_(-1),
      last_sent_frame_id_(-1),
      duplicate_ack_(0),
      last_skip_count_(0),
      congestion_control_(cast_environment->Clock(),
                          video_config.congestion_control_back_off,
                          video_config.max_bitrate, video_config.min_bitrate,
                          video_config.start_bitrate),
      initialized_(false),
      weak_factory_(this) {
  max_unacked_frames_ =
      static_cast<uint8>(video_config.rtp_config.max_delay_ms *
                         video_config.max_frame_rate / 1000) +
      1;
  VLOG(1) << "max_unacked_frames " << static_cast<int>(max_unacked_frames_);
  DCHECK_GT(max_unacked_frames_, 0) << "Invalid argument";

  rtp_video_sender_statistics_.reset(
      new LocalRtpVideoSenderStatistics(transport_sender));

  if (video_config.use_external_encoder) {
    CHECK(gpu_factories);
    video_encoder_.reset(new ExternalVideoEncoder(cast_environment,
        video_config, gpu_factories));
  } else {
    video_encoder_.reset(new VideoEncoderImpl(
        cast_environment, video_config, max_unacked_frames_));
  }

  rtcp_.reset(
      new Rtcp(cast_environment_,
               rtcp_feedback_.get(),
               transport_sender_,
               NULL,  // paced sender.
               rtp_video_sender_statistics_.get(),
               NULL,
               video_config.rtcp_mode,
               base::TimeDelta::FromMilliseconds(video_config.rtcp_interval),
               video_config.sender_ssrc,
               video_config.incoming_feedback_ssrc,
               video_config.rtcp_c_name));

  // TODO(pwestin): pass cast_initialization to |video_encoder_|
  // and remove this call.
  cast_environment_->PostTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(initialization_status, STATUS_INITIALIZED));
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber_);
}

VideoSender::~VideoSender() {
  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber_);
}

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

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  cast_environment_->Logging()->InsertFrameEvent(
      now,
      kVideoFrameReceived,
      GetVideoRtpTimestamp(capture_time),
      kFrameIdUnknown);

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

  cast_environment_->Logging()->InsertFrameEvent(
      last_send_time_,
      kVideoFrameEncoded,
      encoded_frame->rtp_timestamp,
      encoded_frame->frame_id);

  last_sent_frame_id_ = static_cast<int>(encoded_frame->frame_id);
  cast_environment_->PostTask(
      CastEnvironment::TRANSPORT,
      FROM_HERE,
      base::Bind(&VideoSender::SendEncodedVideoFrameToTransport,
                 base::Unretained(this),
                 base::Passed(&encoded_frame),
                 capture_time));
  UpdateFramesInFlight();
  InitializeTimers();
}

void VideoSender::SendEncodedVideoFrameToTransport(
    scoped_ptr<transport::EncodedVideoFrame> encoded_frame,
    const base::TimeTicks& capture_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::TRANSPORT));
  transport_sender_->InsertCodedVideoFrame(encoded_frame.get(), capture_time);
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

  rtcp_->SendRtcpFromRtpSender(sender_log_message);
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
      if (last_acked_frame_id_ == -1) {
        // We have not received any ack, send a key frame.
        video_encoder_->GenerateKeyFrame();
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
    cast_environment_->PostTask(
        CastEnvironment::TRANSPORT,
        FROM_HERE,
        base::Bind(&VideoSender::ResendPacketsOnTransportThread,
                   base::Unretained(this),
                   cast_feedback.missing_frames_and_packets_));

    uint32 new_bitrate = 0;
    if (congestion_control_.OnNack(rtt, &new_bitrate)) {
      video_encoder_->SetBitRate(new_bitrate);
    }
  }
  ReceivedAck(cast_feedback.ack_frame_id_);
}

void VideoSender::ReceivedAck(uint32 acked_frame_id) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  last_acked_frame_id_ = static_cast<int>(acked_frame_id);
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  cast_environment_->Logging()->InsertGenericEvent(
      now, kVideoAckReceived, acked_frame_id);
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
      video_encoder_->SkipNextFrame(true);
      return;
    }
  }
  video_encoder_->SkipNextFrame(false);
}

void VideoSender::ResendFrame(uint32 resend_frame_id) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  MissingFramesAndPacketsMap missing_frames_and_packets;
  PacketIdSet missing;
  missing_frames_and_packets.insert(std::make_pair(resend_frame_id, missing));
  cast_environment_->PostTask(
      CastEnvironment::TRANSPORT,
      FROM_HERE,
      base::Bind(&VideoSender::ResendPacketsOnTransportThread,
                 base::Unretained(this),
                 missing_frames_and_packets));
}

void VideoSender::ResendPacketsOnTransportThread(
    const transport::MissingFramesAndPacketsMap& missing_packets) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::TRANSPORT));
  last_send_time_ = cast_environment_->Clock()->NowTicks();
  transport_sender_->ResendPackets(false, missing_packets);
}

}  // namespace cast
}  // namespace media
