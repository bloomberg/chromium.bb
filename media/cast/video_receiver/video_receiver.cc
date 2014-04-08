// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/video_receiver/video_receiver.h"

#include <algorithm>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/base/video_frame.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/video_receiver/video_decoder.h"

namespace {
const int kMinSchedulingDelayMs = 1;
const int kMinTimeBetweenOffsetUpdatesMs = 1000;
const int kTimeOffsetMaxCounter = 10;
}  // namespace

namespace media {
namespace cast {

VideoReceiver::VideoReceiver(scoped_refptr<CastEnvironment> cast_environment,
                             const VideoReceiverConfig& video_config,
                             transport::PacedPacketSender* const packet_sender)
    : RtpReceiver(cast_environment->Clock(), NULL, &video_config),
      cast_environment_(cast_environment),
      event_subscriber_(kReceiverRtcpEventHistorySize,
                        ReceiverRtcpEventSubscriber::kVideoEventSubscriber),
      codec_(video_config.codec),
      target_delay_delta_(
          base::TimeDelta::FromMilliseconds(video_config.rtp_max_delay_ms)),
      expected_frame_duration_(
          base::TimeDelta::FromSeconds(1) / video_config.max_frame_rate),
      framer_(cast_environment->Clock(),
              this,
              video_config.incoming_ssrc,
              video_config.decoder_faster_than_max_frame_rate,
              video_config.rtp_max_delay_ms * video_config.max_frame_rate /
                  1000),
      rtcp_(cast_environment_,
            NULL,
            NULL,
            packet_sender,
            GetStatistics(),
            video_config.rtcp_mode,
            base::TimeDelta::FromMilliseconds(video_config.rtcp_interval),
            video_config.feedback_ssrc,
            video_config.incoming_ssrc,
            video_config.rtcp_c_name),
      time_offset_counter_(0),
      time_incoming_packet_updated_(false),
      incoming_rtp_timestamp_(0),
      is_waiting_for_consecutive_frame_(false),
      weak_factory_(this) {
  DCHECK_GT(video_config.rtp_max_delay_ms, 0);
  DCHECK_GT(video_config.max_frame_rate, 0);
  if (!video_config.use_external_decoder) {
    video_decoder_.reset(new VideoDecoder(cast_environment, video_config));
  }
  decryptor_.Initialize(video_config.aes_key, video_config.aes_iv_mask);
  rtcp_.SetTargetDelay(target_delay_delta_);
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber_);
  memset(frame_id_to_rtp_timestamp_, 0, sizeof(frame_id_to_rtp_timestamp_));
}

VideoReceiver::~VideoReceiver() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  // If any callbacks for encoded video frames are queued, flush them out now.
  // This is critical because some Closures in |frame_request_queue_| may have
  // Unretained references to |this|.
  while (!frame_request_queue_.empty()) {
    frame_request_queue_.front().Run(
        make_scoped_ptr<transport::EncodedVideoFrame>(NULL), base::TimeTicks());
    frame_request_queue_.pop_front();
  }

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
  DCHECK(!callback.is_null());
  DCHECK(video_decoder_.get());
  GetEncodedVideoFrame(base::Bind(
      &VideoReceiver::DecodeEncodedVideoFrame,
      // Note: Use of Unretained is safe since this Closure is guaranteed to be
      // invoked before destruction of |this|.
      base::Unretained(this),
      callback));
}

void VideoReceiver::DecodeEncodedVideoFrame(
    const VideoFrameDecodedCallback& callback,
    scoped_ptr<transport::EncodedVideoFrame> encoded_frame,
    const base::TimeTicks& playout_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!encoded_frame) {
    callback.Run(make_scoped_refptr<VideoFrame>(NULL), playout_time, false);
    return;
  }
  const uint32 frame_id = encoded_frame->frame_id;
  const uint32 rtp_timestamp = encoded_frame->rtp_timestamp;
  video_decoder_->DecodeFrame(encoded_frame.Pass(),
                              base::Bind(&VideoReceiver::EmitRawVideoFrame,
                                         cast_environment_,
                                         callback,
                                         frame_id,
                                         rtp_timestamp,
                                         playout_time));
}

// static
void VideoReceiver::EmitRawVideoFrame(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const VideoFrameDecodedCallback& callback,
    uint32 frame_id,
    uint32 rtp_timestamp,
    const base::TimeTicks& playout_time,
    const scoped_refptr<VideoFrame>& video_frame,
    bool is_continuous) {
  DCHECK(cast_environment->CurrentlyOn(CastEnvironment::MAIN));
  if (video_frame) {
    const base::TimeTicks now = cast_environment->Clock()->NowTicks();
    cast_environment->Logging()->InsertFrameEvent(
        now, kVideoFrameDecoded, rtp_timestamp, frame_id);
    cast_environment->Logging()->InsertFrameEventWithDelay(
        now, kVideoRenderDelay, rtp_timestamp, frame_id,
        playout_time - now);
    // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
    TRACE_EVENT_INSTANT1(
        "cast_perf_test", "FrameDecoded",
        TRACE_EVENT_SCOPE_THREAD,
        "rtp_timestamp", rtp_timestamp);
  }
  callback.Run(video_frame, playout_time, is_continuous);
}

void VideoReceiver::GetEncodedVideoFrame(
    const VideoFrameEncodedCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  frame_request_queue_.push_back(callback);
  EmitAvailableEncodedFrames();
}

void VideoReceiver::EmitAvailableEncodedFrames() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  while (!frame_request_queue_.empty()) {
    // Attempt to peek at the next completed frame from the |framer_|.
    // TODO(miu): We should only be peeking at the metadata, and not copying the
    // payload yet!  Or, at least, peek using a StringPiece instead of a copy.
    scoped_ptr<transport::EncodedVideoFrame> encoded_frame(
        new transport::EncodedVideoFrame());
    bool is_consecutively_next_frame = false;
    if (!framer_.GetEncodedVideoFrame(encoded_frame.get(),
                                      &is_consecutively_next_frame)) {
      VLOG(1) << "Wait for more video packets to produce a completed frame.";
      return;  // OnReceivedPayloadData() will invoke this method in the future.
    }

    // If |framer_| has a frame ready that is out of sequence, examine the
    // playout time to determine whether it's acceptable to continue, thereby
    // skipping one or more frames.  Skip if the missing frame wouldn't complete
    // playing before the start of playback of the available frame.
    const base::TimeTicks now = cast_environment_->Clock()->NowTicks();
    const base::TimeTicks playout_time =
        GetPlayoutTime(now, encoded_frame->rtp_timestamp);
    if (!is_consecutively_next_frame) {
      // TODO(miu): Also account for expected decode time here?
      const base::TimeTicks earliest_possible_end_time_of_missing_frame =
          now + expected_frame_duration_;
      if (earliest_possible_end_time_of_missing_frame < playout_time) {
        VLOG(1) << "Wait for next consecutive frame instead of skipping.";
        if (!is_waiting_for_consecutive_frame_) {
          is_waiting_for_consecutive_frame_ = true;
          cast_environment_->PostDelayedTask(
              CastEnvironment::MAIN,
              FROM_HERE,
              base::Bind(&VideoReceiver::EmitAvailableEncodedFramesAfterWaiting,
                         weak_factory_.GetWeakPtr()),
              playout_time - now);
        }
        return;
      }
    }

    // Decrypt the payload data in the frame, if crypto is being used.
    if (decryptor_.initialized()) {
      std::string decrypted_video_data;
      if (!decryptor_.Decrypt(encoded_frame->frame_id,
                              encoded_frame->data,
                              &decrypted_video_data)) {
        // Decryption failed.  Give up on this frame, releasing it from the
        // jitter buffer.
        framer_.ReleaseFrame(encoded_frame->frame_id);
        continue;
      }
      encoded_frame->data.swap(decrypted_video_data);
    }

    // At this point, we have a decrypted EncodedVideoFrame ready to be emitted.
    encoded_frame->codec = codec_;
    framer_.ReleaseFrame(encoded_frame->frame_id);
    // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
    TRACE_EVENT_INSTANT2(
        "cast_perf_test", "PullEncodedVideoFrame",
        TRACE_EVENT_SCOPE_THREAD,
        "rtp_timestamp", encoded_frame->rtp_timestamp,
        // TODO(miu): Need to find an alternative to using ToInternalValue():
        "render_time", playout_time.ToInternalValue());
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(frame_request_queue_.front(),
                                           base::Passed(&encoded_frame),
                                           playout_time));
    frame_request_queue_.pop_front();
  }
}

void VideoReceiver::EmitAvailableEncodedFramesAfterWaiting() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(is_waiting_for_consecutive_frame_);
  is_waiting_for_consecutive_frame_ = false;
  EmitAvailableEncodedFrames();
}

base::TimeTicks VideoReceiver::GetPlayoutTime(base::TimeTicks now,
                                              uint32 rtp_timestamp) {
  // TODO(miu): This and AudioReceiver::GetPlayoutTime() need to be reconciled!

  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  // Senders time in ms when this frame was captured.
  // Note: the senders clock and our local clock might not be synced.
  base::TimeTicks rtp_timestamp_in_ticks;

  // Compute the time offset_in_ticks based on the incoming_rtp_timestamp_.
  if (time_offset_counter_ == 0) {
    // Check for received RTCP to sync the stream play it out asap.
    if (rtcp_.RtpTimestampInSenderTime(kVideoFrequency,
                                       incoming_rtp_timestamp_,
                                       &rtp_timestamp_in_ticks)) {
       ++time_offset_counter_;
    }
  } else if (time_incoming_packet_updated_) {
    if (rtcp_.RtpTimestampInSenderTime(kVideoFrequency,
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
      else if (time_offset_counter_ < kTimeOffsetMaxCounter) {
        time_offset_ = std::min(time_offset_, time_offset);
      }
      if (time_offset_counter_ < kTimeOffsetMaxCounter)
        ++time_offset_counter_;
    }
  }
  // Reset |time_incoming_packet_updated_| to enable a future measurement.
  time_incoming_packet_updated_ = false;
  // Compute the actual rtp_timestamp_in_ticks based on the current timestamp.
  if (!rtcp_.RtpTimestampInSenderTime(
           kVideoFrequency, rtp_timestamp, &rtp_timestamp_in_ticks)) {
    // This can fail if we have not received any RTCP packets in a long time.
    // BUG: These calculations are a placeholder, and to be revisited in a
    // soon-upcoming change.  http://crbug.com/356942
    const int frequency_khz = kVideoFrequency / 1000;
    const base::TimeDelta delta_based_on_rtp_timestamps =
        base::TimeDelta::FromMilliseconds(
            static_cast<int32>(rtp_timestamp - incoming_rtp_timestamp_) /
                frequency_khz);
    return time_incoming_packet_ + delta_based_on_rtp_timestamps;
  }

  base::TimeTicks render_time =
      rtp_timestamp_in_ticks + time_offset_ + target_delay_delta_;
  // TODO(miu): This is broken since this "getter" method may be called on
  // frames received out-of-order, which means the playout times for earlier
  // frames will be computed incorrectly.
#if 0
  if (last_render_time_ > render_time)
    render_time = last_render_time_;
  last_render_time_ = render_time;
#endif

  return render_time;
}

void VideoReceiver::IncomingPacket(scoped_ptr<Packet> packet) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (Rtcp::IsRtcpPacket(&packet->front(), packet->size())) {
    rtcp_.IncomingRtcpPacket(&packet->front(), packet->size());
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
  const bool complete =
      framer_.InsertPacket(payload_data, payload_size, rtp_header, &duplicate);
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

  EmitAvailableEncodedFrames();
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
  rtcp_.SendRtcpFromRtpReceiver(&cast_message, &rtcp_events);
}

// Cast messages should be sent within a maximum interval. Schedule a call
// if not triggered elsewhere, e.g. by the cast message_builder.
void VideoReceiver::ScheduleNextCastMessage() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeTicks send_time;
  framer_.TimeToSendNextCastMessage(&send_time);
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
  framer_.SendCastMessage();  // Will only send a message if it is time.
  ScheduleNextCastMessage();
}

void VideoReceiver::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_next = rtcp_.TimeToSendNextRtcpReport() -
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
  rtcp_.SendRtcpFromRtpReceiver(NULL, NULL);
  ScheduleNextRtcpReport();
}

}  // namespace cast
}  // namespace media
