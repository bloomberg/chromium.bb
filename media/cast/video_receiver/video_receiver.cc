// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_receiver/video_receiver.h"

#include <algorithm>
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/cast_defines.h"
#include "media/cast/framer/framer.h"
#include "media/cast/video_receiver/video_decoder.h"

namespace media {
namespace cast {

const int64 kMinSchedulingDelayMs = 1;
static const int64 kMaxFrameWaitMs = 20;
static const int64 kMinTimeBetweenOffsetUpdatesMs = 500;
static const int kTimeOffsetFilter = 8;

// Local implementation of RtpData (defined in rtp_rtcp_defines.h).
// Used to pass payload data into the video receiver.
class LocalRtpVideoData : public RtpData {
 public:
  explicit LocalRtpVideoData(VideoReceiver* video_receiver)
      : video_receiver_(video_receiver),
        time_updated_(false),
        incoming_rtp_timestamp_(0) {
  }
  ~LocalRtpVideoData() {}

  virtual void OnReceivedPayloadData(const uint8* payload_data,
                                     int payload_size,
                                     const RtpCastHeader* rtp_header) OVERRIDE {
    {
      if (!time_updated_) {
        incoming_rtp_timestamp_ = rtp_header->webrtc.header.timestamp;
        time_incoming_packet_ = video_receiver_->clock_->NowTicks();
        time_updated_ = true;
      } else if (video_receiver_->clock_->NowTicks() > time_incoming_packet_ +
            base::TimeDelta::FromMilliseconds(kMinTimeBetweenOffsetUpdatesMs)) {
        incoming_rtp_timestamp_ = rtp_header->webrtc.header.timestamp;
        time_incoming_packet_ = video_receiver_->clock_->NowTicks();
        time_updated_ = true;
      }
    }
    video_receiver_->IncomingRtpPacket(payload_data, payload_size, *rtp_header);
  }

  bool GetPacketTimeInformation(base::TimeTicks* time_incoming_packet,
                                uint32* incoming_rtp_timestamp) {
    *time_incoming_packet = time_incoming_packet_;
    *incoming_rtp_timestamp = incoming_rtp_timestamp_;
    bool time_updated = time_updated_;
    time_updated_ = false;
    return time_updated;
  }

 private:
  VideoReceiver* video_receiver_;
  bool time_updated_;
  base::TimeTicks time_incoming_packet_;
  uint32 incoming_rtp_timestamp_;
};

// Local implementation of RtpPayloadFeedback (defined in rtp_defines.h)
// Used to convey cast-specific feedback from receiver to sender.
// Callback triggered by the Framer (cast message builder).
class LocalRtpVideoFeedback : public RtpPayloadFeedback {
 public:
  explicit LocalRtpVideoFeedback(VideoReceiver* video_receiver)
      : video_receiver_(video_receiver) {
  }
  virtual void CastFeedback(const RtcpCastMessage& cast_message) OVERRIDE {
    video_receiver_->CastFeedback(cast_message);
  }

  virtual void RequestKeyFrame() OVERRIDE {
    video_receiver_->RequestKeyFrame();
  }

 private:
  VideoReceiver* video_receiver_;
};

// Local implementation of RtpReceiverStatistics (defined by rtcp.h).
// Used to pass statistics data from the RTP module to the RTCP module.
class LocalRtpReceiverStatistics : public RtpReceiverStatistics {
 public:
  explicit LocalRtpReceiverStatistics(RtpReceiver* rtp_receiver)
     : rtp_receiver_(rtp_receiver) {
  }

  virtual void GetStatistics(uint8* fraction_lost,
                             uint32* cumulative_lost,  // 24 bits valid.
                             uint32* extended_high_sequence_number,
                             uint32* jitter) OVERRIDE {
    rtp_receiver_->GetStatistics(fraction_lost,
                                 cumulative_lost,
                                 extended_high_sequence_number,
                                 jitter);
  }

 private:
  RtpReceiver* rtp_receiver_;
};


VideoReceiver::VideoReceiver(scoped_refptr<CastThread> cast_thread,
                             const VideoReceiverConfig& video_config,
                             PacedPacketSender* const packet_sender)
      : cast_thread_(cast_thread),
        codec_(video_config.codec),
        incoming_ssrc_(video_config.incoming_ssrc),
        default_tick_clock_(new base::DefaultTickClock()),
        clock_(default_tick_clock_.get()),
        incoming_payload_callback_(new LocalRtpVideoData(this)),
        incoming_payload_feedback_(new LocalRtpVideoFeedback(this)),
        rtp_receiver_(NULL, &video_config, incoming_payload_callback_.get()),
        rtp_video_receiver_statistics_(
            new LocalRtpReceiverStatistics(&rtp_receiver_)),
        weak_factory_(this) {
  target_delay_delta_ = base::TimeDelta::FromMilliseconds(
      video_config.rtp_max_delay_ms);
  int max_unacked_frames = video_config.rtp_max_delay_ms *
      video_config.max_frame_rate / 1000;
  DCHECK(max_unacked_frames) << "Invalid argument";

  framer_.reset(new Framer(incoming_payload_feedback_.get(),
                           video_config.incoming_ssrc,
                           video_config.decoder_faster_than_max_frame_rate,
                           max_unacked_frames));
  if (!video_config.use_external_decoder) {
    video_decoder_ = new VideoDecoder(cast_thread_, video_config);
  }

  rtcp_.reset(new Rtcp(NULL,
              packet_sender,
              NULL,
              rtp_video_receiver_statistics_.get(),
              video_config.rtcp_mode,
              base::TimeDelta::FromMilliseconds(video_config.rtcp_interval),
              false,
              video_config.feedback_ssrc,
              video_config.rtcp_c_name));

  rtcp_->SetRemoteSSRC(video_config.incoming_ssrc);
  ScheduleNextRtcpReport();
  ScheduleNextCastMessage();
}

VideoReceiver::~VideoReceiver() {}

void VideoReceiver::GetRawVideoFrame(
    const VideoFrameDecodedCallback& callback) {
  DCHECK(video_decoder_);
  scoped_ptr<EncodedVideoFrame> encoded_frame(new EncodedVideoFrame());
  base::TimeTicks render_time;
    if (GetEncodedVideoFrame(encoded_frame.get(), &render_time)) {
      base::Closure frame_release_callback =
        base::Bind(&VideoReceiver::ReleaseFrame,
        weak_factory_.GetWeakPtr(), encoded_frame->frame_id);
    // Hand the ownership of the encoded frame to the decode thread.
    cast_thread_->PostTask(CastThread::VIDEO_DECODER, FROM_HERE,
        base::Bind(&VideoReceiver::DecodeVideoFrameThread,
        weak_factory_.GetWeakPtr(), encoded_frame.release(),
        render_time, callback, frame_release_callback));
  }
}

// Utility function to run the decoder on a designated decoding thread.
void VideoReceiver::DecodeVideoFrameThread(
    const EncodedVideoFrame* encoded_frame,
    const base::TimeTicks render_time,
    const VideoFrameDecodedCallback& frame_decoded_callback,
    base::Closure frame_release_callback) {
  video_decoder_->DecodeVideoFrame(encoded_frame, render_time,
      frame_decoded_callback, frame_release_callback);
  // Release memory.
  delete encoded_frame;
}

bool VideoReceiver::GetEncodedVideoFrame(EncodedVideoFrame* encoded_frame,
                                         base::TimeTicks* render_time) {
  DCHECK(encoded_frame);
  DCHECK(render_time);

  uint32 rtp_timestamp = 0;
  bool next_frame = false;

  base::TimeTicks timeout = clock_->NowTicks() +
      base::TimeDelta::FromMilliseconds(kMaxFrameWaitMs);
  if (!framer_->GetEncodedVideoFrame(timeout,
                                     encoded_frame,
                                     &rtp_timestamp,
                                     &next_frame)) {
    return false;
  }
  base::TimeTicks now = clock_->NowTicks();
  *render_time = GetRenderTime(now, rtp_timestamp);

  base::TimeDelta max_frame_wait_delta =
      base::TimeDelta::FromMilliseconds(kMaxFrameWaitMs);
  base::TimeDelta time_until_render = *render_time - now;
  base::TimeDelta time_until_release = time_until_render - max_frame_wait_delta;
  base::TimeDelta zero_delta = base::TimeDelta::FromMilliseconds(0);
  if (!next_frame && (time_until_release > zero_delta)) {
    // TODO(mikhal): If returning false, then the application should sleep, or
    // else which may spin here. Alternatively, we could sleep here, which will
    // be posting a delayed task to ourselves, but then can end up in getting
    // stuck as well.
    return false;
  }

  base::TimeDelta dont_show_timeout_delta = time_until_render -
      base::TimeDelta::FromMilliseconds(-kDontShowTimeoutMs);
  if (codec_ == kVp8 && time_until_render < dont_show_timeout_delta) {
    encoded_frame->data[0] &= 0xef;
    VLOG(1) << "Don't show frame";
  }

  encoded_frame->codec = codec_;
  return true;
}

base::TimeTicks VideoReceiver::GetRenderTime(base::TimeTicks now,
                                             uint32 rtp_timestamp) {
  // Senders time in ms when this frame was captured.
  // Note: the senders clock and our local clock might not be synced.
  base::TimeTicks rtp_timestamp_in_ticks;
  base::TimeTicks time_incoming_packet;
  uint32 incoming_rtp_timestamp;

  if (time_offset_.InMilliseconds()) {  // was == 0
    incoming_payload_callback_->GetPacketTimeInformation(
        &time_incoming_packet, &incoming_rtp_timestamp);

    if (!rtcp_->RtpTimestampInSenderTime(kVideoFrequency,
                                         incoming_rtp_timestamp,
                                         &rtp_timestamp_in_ticks)) {
      // We have not received any RTCP to sync the stream play it out as soon as
      // possible.
      return now;
    }
    time_offset_ = time_incoming_packet - rtp_timestamp_in_ticks;
  } else if (incoming_payload_callback_->GetPacketTimeInformation(
      &time_incoming_packet, &incoming_rtp_timestamp)) {
    if (rtcp_->RtpTimestampInSenderTime(kVideoFrequency,
                                        incoming_rtp_timestamp,
                                        &rtp_timestamp_in_ticks)) {
      // Time to update the time_offset.
      base::TimeDelta time_offset =
          time_incoming_packet - rtp_timestamp_in_ticks;
      time_offset_ = ((kTimeOffsetFilter - 1) * time_offset_ + time_offset)
          / kTimeOffsetFilter;
    }
  }
  if (!rtcp_->RtpTimestampInSenderTime(kVideoFrequency,
                                       rtp_timestamp,
                                       &rtp_timestamp_in_ticks)) {
    // This can fail if we have not received any RTCP packets in a long time.
    return now;
  }
  return (rtp_timestamp_in_ticks + time_offset_ + target_delay_delta_);
}

void VideoReceiver::IncomingPacket(const uint8* packet, int length) {
  if (Rtcp::IsRtcpPacket(packet, length)) {
    rtcp_->IncomingRtcpPacket(packet, length);
    return;
  }
  rtp_receiver_.ReceivedPacket(packet, length);
}

void VideoReceiver::IncomingRtpPacket(const uint8* payload_data,
                                      int payload_size,
                                      const RtpCastHeader& rtp_header) {
  framer_->InsertPacket(payload_data, payload_size, rtp_header);
}

// Send a cast feedback message. Actual message created in the framer (cast
// message builder).
void VideoReceiver::CastFeedback(const RtcpCastMessage& cast_message) {
  rtcp_->SendRtcpCast(cast_message);
  time_last_sent_cast_message_= clock_->NowTicks();
}

void VideoReceiver::ReleaseFrame(uint8 frame_id) {
  framer_->ReleaseFrame(frame_id);
}

// Send a key frame request to the sender.
void VideoReceiver::RequestKeyFrame() {
  rtcp_->SendRtcpPli(incoming_ssrc_);
}

// Cast messages should be sent within a maximum interval. Schedule a call
// if not triggered elsewhere, e.g. by the cast message_builder.
void VideoReceiver::ScheduleNextCastMessage() {
  base::TimeTicks send_time;
  framer_->TimeToSendNextCastMessage(&send_time);

  base::TimeDelta time_to_send = send_time - clock_->NowTicks();
  time_to_send = std::max(time_to_send,
      base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));
  cast_thread_->PostDelayedTask(CastThread::MAIN, FROM_HERE,
      base::Bind(&VideoReceiver::SendNextCastMessage,
                 weak_factory_.GetWeakPtr()), time_to_send);
}

void VideoReceiver::SendNextCastMessage() {
  framer_->SendCastMessage();  // Will only send a message if it is time.
  ScheduleNextCastMessage();
}

// Schedule the next RTCP report to be sent back to the sender.
void VideoReceiver::ScheduleNextRtcpReport() {
  base::TimeDelta time_to_next =
      rtcp_->TimeToSendNextRtcpReport() - clock_->NowTicks();

  time_to_next = std::max(time_to_next,
      base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_thread_->PostDelayedTask(CastThread::MAIN, FROM_HERE,
      base::Bind(&VideoReceiver::SendNextRtcpReport,
                weak_factory_.GetWeakPtr()), time_to_next);
}

void VideoReceiver::SendNextRtcpReport() {
  rtcp_->SendRtcpReport(incoming_ssrc_);
  ScheduleNextRtcpReport();
}

}  // namespace cast
}  // namespace media
