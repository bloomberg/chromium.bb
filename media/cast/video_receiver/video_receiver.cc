// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_receiver/video_receiver.h"

#include <algorithm>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/cast_defines.h"
#include "media/cast/framer/framer.h"
#include "media/cast/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/rtcp/rtcp_sender.h"
#include "media/cast/video_receiver/video_decoder.h"

namespace {

static const int64 kMinSchedulingDelayMs = 1;
static const int64 kMinTimeBetweenOffsetUpdatesMs = 1000;
static const int kTimeOffsetMaxCounter = 10;

}  // namespace

namespace media {
namespace cast {

// Local implementation of RtpPayloadFeedback (defined in rtp_defines.h)
// Used to convey cast-specific feedback from receiver to sender.
// Callback triggered by the Framer (cast message builder).
class LocalRtpVideoFeedback : public RtpPayloadFeedback {
 public:
  explicit LocalRtpVideoFeedback(VideoReceiver* video_receiver)
      : video_receiver_(video_receiver) {}

  virtual void CastFeedback(const RtcpCastMessage& cast_message) OVERRIDE {
    video_receiver_->CastFeedback(cast_message);
  }

 private:
  VideoReceiver* video_receiver_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocalRtpVideoFeedback);
};

VideoReceiver::VideoReceiver(scoped_refptr<CastEnvironment> cast_environment,
                             const VideoReceiverConfig& video_config,
                             transport::PacedPacketSender* const packet_sender,
                             const SetTargetDelayCallback& target_delay_cb)
    : RtpReceiver(cast_environment->Clock(), NULL, &video_config),
      cast_environment_(cast_environment),
      event_subscriber_(kReceiverRtcpEventHistorySize,
                        ReceiverRtcpEventSubscriber::kVideoEventSubscriber),
      codec_(video_config.codec),
      target_delay_delta_(
          base::TimeDelta::FromMilliseconds(video_config.rtp_max_delay_ms)),
      frame_delay_(base::TimeDelta::FromMilliseconds(
          1000 / video_config.max_frame_rate)),
      incoming_payload_feedback_(new LocalRtpVideoFeedback(this)),
      time_offset_counter_(0),
      decryptor_(),
      time_incoming_packet_updated_(false),
      incoming_rtp_timestamp_(0),
      target_delay_cb_(target_delay_cb),
      weak_factory_(this) {
  int max_unacked_frames =
      video_config.rtp_max_delay_ms * video_config.max_frame_rate / 1000;
  DCHECK(max_unacked_frames) << "Invalid argument";

  decryptor_.Initialize(video_config.aes_key, video_config.aes_iv_mask);
  framer_.reset(new Framer(cast_environment->Clock(),
                           incoming_payload_feedback_.get(),
                           video_config.incoming_ssrc,
                           video_config.decoder_faster_than_max_frame_rate,
                           max_unacked_frames));

  if (!video_config.use_external_decoder) {
    video_decoder_.reset(new VideoDecoder(video_config, cast_environment));
  }

  rtcp_.reset(
      new Rtcp(cast_environment_,
               NULL,
               NULL,
               packet_sender,
               GetStatistics(),
               video_config.rtcp_mode,
               base::TimeDelta::FromMilliseconds(video_config.rtcp_interval),
               video_config.feedback_ssrc,
               video_config.incoming_ssrc,
               video_config.rtcp_c_name));
  // Set the target delay that will be conveyed to the sender.
  rtcp_->SetTargetDelay(target_delay_delta_);
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber_);
  memset(frame_id_to_rtp_timestamp_, 0, sizeof(frame_id_to_rtp_timestamp_));
}

VideoReceiver::~VideoReceiver() {
  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber_);
}

void VideoReceiver::InitializeTimers() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  ScheduleNextRtcpReport();
  ScheduleNextCastMessage();
}

void VideoReceiver::GetRawVideoFrame(
    const VideoFrameDecodedCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  GetEncodedVideoFrame(base::Bind(
      &VideoReceiver::DecodeVideoFrame, base::Unretained(this), callback));
}

// Called when we have a frame to decode.
void VideoReceiver::DecodeVideoFrame(
    const VideoFrameDecodedCallback& callback,
    scoped_ptr<transport::EncodedVideoFrame> encoded_frame,
    const base::TimeTicks& render_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  // Hand the ownership of the encoded frame to the decode thread.
  cast_environment_->PostTask(CastEnvironment::VIDEO,
                              FROM_HERE,
                              base::Bind(&VideoReceiver::DecodeVideoFrameThread,
                                         base::Unretained(this),
                                         base::Passed(&encoded_frame),
                                         render_time,
                                         callback));
}

// Utility function to run the decoder on a designated decoding thread.
void VideoReceiver::DecodeVideoFrameThread(
    scoped_ptr<transport::EncodedVideoFrame> encoded_frame,
    const base::TimeTicks render_time,
    const VideoFrameDecodedCallback& frame_decoded_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::VIDEO));
  DCHECK(video_decoder_);

  if (!(video_decoder_->DecodeVideoFrame(
           encoded_frame.get(), render_time, frame_decoded_callback))) {
    // This will happen if we decide to decode but not show a frame.
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&VideoReceiver::GetRawVideoFrame,
                                           base::Unretained(this),
                                           frame_decoded_callback));
  }
}

bool VideoReceiver::DecryptVideoFrame(
    scoped_ptr<transport::EncodedVideoFrame>* video_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  if (!decryptor_.initialized())
    return false;

  std::string decrypted_video_data;
  if (!decryptor_.Decrypt((*video_frame)->frame_id,
                          (*video_frame)->data,
                          &decrypted_video_data)) {
    // Give up on this frame, release it from jitter buffer.
    framer_->ReleaseFrame((*video_frame)->frame_id);
    return false;
  }
  (*video_frame)->data.swap(decrypted_video_data);
  return true;
}

// Called from the main cast thread.
void VideoReceiver::GetEncodedVideoFrame(
    const VideoFrameEncodedCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  scoped_ptr<transport::EncodedVideoFrame> encoded_frame(
      new transport::EncodedVideoFrame());
  bool next_frame = false;

  if (!framer_->GetEncodedVideoFrame(encoded_frame.get(), &next_frame)) {
    // We have no video frames. Wait for new packet(s).
    queued_encoded_callbacks_.push_back(callback);
    return;
  }

  if (decryptor_.initialized() && !DecryptVideoFrame(&encoded_frame)) {
    // Logging already done.
    queued_encoded_callbacks_.push_back(callback);
    return;
  }

  base::TimeTicks render_time;
  if (PullEncodedVideoFrame(next_frame, &encoded_frame, &render_time)) {
    cast_environment_->PostTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(callback, base::Passed(&encoded_frame), render_time));
  } else {
    // We have a video frame; however we are missing packets and we have time
    // to wait for new packet(s).
    queued_encoded_callbacks_.push_back(callback);
  }
}

// Should we pull the encoded video frame from the framer? decided by if this is
// the next frame or we are running out of time and have to pull the following
// frame.
// If the frame is too old to be rendered we set the don't show flag in the
// video bitstream where possible.
bool VideoReceiver::PullEncodedVideoFrame(
    bool next_frame,
    scoped_ptr<transport::EncodedVideoFrame>* encoded_frame,
    base::TimeTicks* render_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  *render_time = GetRenderTime(now, (*encoded_frame)->rtp_timestamp);

  // TODO(mikhal): Store actual render time and not diff.
  cast_environment_->Logging()->InsertFrameEventWithDelay(
      now,
      kVideoRenderDelay,
      (*encoded_frame)->rtp_timestamp,
      (*encoded_frame)->frame_id,
      now - *render_time);

  // Minimum time before a frame is due to be rendered before we pull it for
  // decode.
  base::TimeDelta min_wait_delta = frame_delay_;
  base::TimeDelta time_until_render = *render_time - now;
  if (!next_frame && (time_until_render > min_wait_delta)) {
    // Example:
    // We have decoded frame 1 and we have received the complete frame 3, but
    // not frame 2. If we still have time before frame 3 should be rendered we
    // will wait for 2 to arrive, however if 2 never show up this timer will hit
    // and we will pull out frame 3 for decoding and rendering.
    base::TimeDelta time_until_release = time_until_render - min_wait_delta;
    cast_environment_->PostDelayedTask(
        CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(&VideoReceiver::PlayoutTimeout, weak_factory_.GetWeakPtr()),
        time_until_release);
    VLOG(1) << "Wait before releasing frame "
            << static_cast<int>((*encoded_frame)->frame_id) << " time "
            << time_until_release.InMilliseconds();
    return false;
  }

  base::TimeDelta dont_show_timeout_delta =
      base::TimeDelta::FromMilliseconds(-kDontShowTimeoutMs);
  if (codec_ == transport::kVp8 &&
      time_until_render < dont_show_timeout_delta) {
    (*encoded_frame)->data[0] &= 0xef;
    VLOG(1) << "Don't show frame "
            << static_cast<int>((*encoded_frame)->frame_id)
            << " time_until_render:" << time_until_render.InMilliseconds();
  } else {
    VLOG(2) << "Show frame " << static_cast<int>((*encoded_frame)->frame_id)
            << " time_until_render:" << time_until_render.InMilliseconds();
  }
  // We have a copy of the frame, release this one.
  framer_->ReleaseFrame((*encoded_frame)->frame_id);
  (*encoded_frame)->codec = codec_;

  // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
  TRACE_EVENT_INSTANT2(
      "cast_perf_test", "PullEncodedVideoFrame",
      TRACE_EVENT_SCOPE_THREAD,
      "rtp_timestamp", (*encoded_frame)->rtp_timestamp,
      "render_time", render_time->ToInternalValue());

  return true;
}

void VideoReceiver::PlayoutTimeout() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (queued_encoded_callbacks_.empty())
    return;

  bool next_frame = false;
  scoped_ptr<transport::EncodedVideoFrame> encoded_frame(
      new transport::EncodedVideoFrame());

  if (!framer_->GetEncodedVideoFrame(encoded_frame.get(), &next_frame)) {
    // We have no video frames. Wait for new packet(s).
    // Since the application can post multiple VideoFrameEncodedCallback and
    // we only check the next frame to play out we might have multiple timeout
    // events firing after each other; however this should be a rare event.
    VLOG(1) << "Failed to retrieved a complete frame at this point in time";
    return;
  }
  VLOG(2) << "PlayoutTimeout retrieved frame "
          << static_cast<int>(encoded_frame->frame_id);

  if (decryptor_.initialized() && !DecryptVideoFrame(&encoded_frame)) {
    // Logging already done.
    return;
  }

  base::TimeTicks render_time;
  if (PullEncodedVideoFrame(next_frame, &encoded_frame, &render_time)) {
    if (!queued_encoded_callbacks_.empty()) {
      VideoFrameEncodedCallback callback = queued_encoded_callbacks_.front();
      queued_encoded_callbacks_.pop_front();
      cast_environment_->PostTask(
          CastEnvironment::MAIN,
          FROM_HERE,
          base::Bind(callback, base::Passed(&encoded_frame), render_time));
    }
  }
  // Else we have a video frame; however we are missing packets and we have time
  // to wait for new packet(s).
}

base::TimeTicks VideoReceiver::GetRenderTime(base::TimeTicks now,
                                             uint32 rtp_timestamp) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  // Senders time in ms when this frame was captured.
  // Note: the senders clock and our local clock might not be synced.
  base::TimeTicks rtp_timestamp_in_ticks;

  // Compute the time offset_in_ticks based on the incoming_rtp_timestamp_.
  if (time_offset_counter_ == 0) {
    // Check for received RTCP to sync the stream play it out asap.
    if (rtcp_->RtpTimestampInSenderTime(kVideoFrequency,
                                        incoming_rtp_timestamp_,
                                        &rtp_timestamp_in_ticks)) {

       ++time_offset_counter_;
    }
    return now;
  } else if (time_incoming_packet_updated_) {
    if (rtcp_->RtpTimestampInSenderTime(kVideoFrequency,
                                        incoming_rtp_timestamp_,
                                        &rtp_timestamp_in_ticks)) {
      // Time to update the time_offset.
      base::TimeDelta time_offset =
          time_incoming_packet_ - rtp_timestamp_in_ticks;
      // Taking the minimum of the first kTimeOffsetMaxCounter values. We are
      // assuming that we are looking for the minimum offset, which will occur
      // when network conditions are the best. This should occur at least once
      // within the first kTimeOffsetMaxCounter samples. Any drift should be
      // very slow, and negligible for this use case.
      if (time_offset_counter_ == 1)
        time_offset_ = time_offset;
      else if (time_offset_counter_  < kTimeOffsetMaxCounter) {
        time_offset_ = std::min(time_offset_, time_offset);
      }
      ++time_offset_counter_;
    }
  }
  // Reset |time_incoming_packet_updated_| to enable a future measurement.
  time_incoming_packet_updated_ = false;
  // Compute the actual rtp_timestamp_in_ticks based on the current timestamp.
  if (!rtcp_->RtpTimestampInSenderTime(
           kVideoFrequency, rtp_timestamp, &rtp_timestamp_in_ticks)) {
    // This can fail if we have not received any RTCP packets in a long time.
    return now;
  }

  base::TimeTicks render_time =
      rtp_timestamp_in_ticks + time_offset_ + target_delay_delta_;
  if (last_render_time_ > render_time)
    render_time = last_render_time_;
  last_render_time_ = render_time;
  return render_time;
}

void VideoReceiver::IncomingPacket(scoped_ptr<Packet> packet) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (Rtcp::IsRtcpPacket(&packet->front(), packet->size())) {
    rtcp_->IncomingRtcpPacket(&packet->front(), packet->size());
  } else {
    ReceivedPacket(&packet->front(), packet->size());
  }
}

void VideoReceiver::OnReceivedPayloadData(const uint8* payload_data,
                                          size_t payload_size,
                                          const RtpCastHeader& rtp_header) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  if (time_incoming_packet_.is_null() ||
      now - time_incoming_packet_ >
          base::TimeDelta::FromMilliseconds(kMinTimeBetweenOffsetUpdatesMs)) {
    if (time_incoming_packet_.is_null())
      InitializeTimers();
    incoming_rtp_timestamp_ = rtp_header.webrtc.header.timestamp;
    // The following incoming packet info is used for syncing sender and
    // receiver clock. Use only the first packet of every frame to obtain a
    // minimal value.
    if (rtp_header.packet_id == 0) {
      time_incoming_packet_ = now;
      time_incoming_packet_updated_ = true;
    }
  }

  frame_id_to_rtp_timestamp_[rtp_header.frame_id & 0xff] =
      rtp_header.webrtc.header.timestamp;
  cast_environment_->Logging()->InsertPacketEvent(
      now,
      kVideoPacketReceived,
      rtp_header.webrtc.header.timestamp,
      rtp_header.frame_id,
      rtp_header.packet_id,
      rtp_header.max_packet_id,
      payload_size);

  bool duplicate = false;
  bool complete =
      framer_->InsertPacket(payload_data, payload_size, rtp_header, &duplicate);

  if (duplicate) {
    cast_environment_->Logging()->InsertPacketEvent(
        now,
        kDuplicateVideoPacketReceived,
        rtp_header.webrtc.header.timestamp,
        rtp_header.frame_id,
        rtp_header.packet_id,
        rtp_header.max_packet_id,
        payload_size);
    // Duplicate packets are ignored.
    return;
  }
  if (!complete)
    return;  // Video frame not complete; wait for more packets.
  if (queued_encoded_callbacks_.empty())
    return;  // No pending callback.

  VideoFrameEncodedCallback callback = queued_encoded_callbacks_.front();
  queued_encoded_callbacks_.pop_front();
  cast_environment_->PostTask(CastEnvironment::MAIN,
                              FROM_HERE,
                              base::Bind(&VideoReceiver::GetEncodedVideoFrame,
                                         weak_factory_.GetWeakPtr(),
                                         callback));
}

// Send a cast feedback message. Actual message created in the framer (cast
// message builder).
void VideoReceiver::CastFeedback(const RtcpCastMessage& cast_message) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  RtpTimestamp rtp_timestamp =
      frame_id_to_rtp_timestamp_[cast_message.ack_frame_id_ & 0xff];
  cast_environment_->Logging()->InsertFrameEvent(
      now, kVideoAckSent, rtp_timestamp, cast_message.ack_frame_id_);

  ReceiverRtcpEventSubscriber::RtcpEventMultiMap rtcp_events;
  event_subscriber_.GetRtcpEventsAndReset(&rtcp_events);
  rtcp_->SendRtcpFromRtpReceiver(&cast_message, &rtcp_events);
}

// Cast messages should be sent within a maximum interval. Schedule a call
// if not triggered elsewhere, e.g. by the cast message_builder.
void VideoReceiver::ScheduleNextCastMessage() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeTicks send_time;
  framer_->TimeToSendNextCastMessage(&send_time);

  base::TimeDelta time_to_send =
      send_time - cast_environment_->Clock()->NowTicks();
  time_to_send = std::max(
      time_to_send, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));
  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&VideoReceiver::SendNextCastMessage,
                 weak_factory_.GetWeakPtr()),
      time_to_send);
}

void VideoReceiver::SendNextCastMessage() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  framer_->SendCastMessage();  // Will only send a message if it is time.
  ScheduleNextCastMessage();
}

// Schedule the next RTCP report to be sent back to the sender.
void VideoReceiver::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_next = rtcp_->TimeToSendNextRtcpReport() -
                                 cast_environment_->Clock()->NowTicks();

  time_to_next = std::max(
      time_to_next, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&VideoReceiver::SendNextRtcpReport,
                 weak_factory_.GetWeakPtr()),
      time_to_next);
}

void VideoReceiver::SendNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  rtcp_->SendRtcpFromRtpReceiver(NULL, NULL);
  ScheduleNextRtcpReport();
}

void VideoReceiver::UpdateTargetDelay() {
  NOTIMPLEMENTED();
  rtcp_->SetTargetDelay(target_delay_delta_);
  target_delay_cb_.Run(target_delay_delta_);
}

}  // namespace cast
}  // namespace media
