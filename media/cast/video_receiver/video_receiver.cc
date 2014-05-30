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
}  // namespace

namespace media {
namespace cast {

VideoReceiver::VideoReceiver(scoped_refptr<CastEnvironment> cast_environment,
                             const FrameReceiverConfig& video_config,
                             transport::PacedPacketSender* const packet_sender)
    : RtpReceiver(cast_environment->Clock(), NULL, &video_config),
      cast_environment_(cast_environment),
      event_subscriber_(kReceiverRtcpEventHistorySize, VIDEO_EVENT),
      codec_(video_config.codec.video),
      target_playout_delay_(
          base::TimeDelta::FromMilliseconds(video_config.rtp_max_delay_ms)),
      expected_frame_duration_(
          base::TimeDelta::FromSeconds(1) / video_config.max_frame_rate),
      reports_are_scheduled_(false),
      framer_(cast_environment->Clock(),
              this,
              video_config.incoming_ssrc,
              true,
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
            video_config.rtcp_c_name,
            false),
      is_waiting_for_consecutive_frame_(false),
      lip_sync_drift_(ClockDriftSmoother::GetDefaultTimeConstant()),
      weak_factory_(this) {
  DCHECK_GT(video_config.rtp_max_delay_ms, 0);
  DCHECK_GT(video_config.max_frame_rate, 0);
  video_decoder_.reset(new VideoDecoder(cast_environment, video_config));
  decryptor_.Initialize(video_config.aes_key, video_config.aes_iv_mask);
  rtcp_.SetTargetDelay(target_playout_delay_);
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber_);
  memset(frame_id_to_rtp_timestamp_, 0, sizeof(frame_id_to_rtp_timestamp_));
}

VideoReceiver::~VideoReceiver() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber_);
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
    scoped_ptr<transport::EncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!encoded_frame) {
    callback.Run(
        make_scoped_refptr<VideoFrame>(NULL), base::TimeTicks(), false);
    return;
  }
  const uint32 frame_id = encoded_frame->frame_id;
  const uint32 rtp_timestamp = encoded_frame->rtp_timestamp;
  const base::TimeTicks playout_time = encoded_frame->reference_time;
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
        now, FRAME_DECODED, VIDEO_EVENT, rtp_timestamp, frame_id);
    cast_environment->Logging()->InsertFrameEventWithDelay(
        now, FRAME_PLAYOUT, VIDEO_EVENT, rtp_timestamp, frame_id,
        playout_time - now);
    // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
    TRACE_EVENT_INSTANT1(
        "cast_perf_test", "FrameDecoded",
        TRACE_EVENT_SCOPE_THREAD,
        "rtp_timestamp", rtp_timestamp);
  }
  callback.Run(video_frame, playout_time, is_continuous);
}

void VideoReceiver::GetEncodedVideoFrame(const FrameEncodedCallback& callback) {
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
    scoped_ptr<transport::EncodedFrame> encoded_frame(
        new transport::EncodedFrame());
    bool is_consecutively_next_frame = false;
    bool have_multiple_complete_frames = false;

    if (!framer_.GetEncodedFrame(encoded_frame.get(),
                                 &is_consecutively_next_frame,
                                 &have_multiple_complete_frames)) {
      VLOG(1) << "Wait for more video packets to produce a completed frame.";
      return;  // OnReceivedPayloadData() will invoke this method in the future.
    }

    const base::TimeTicks now = cast_environment_->Clock()->NowTicks();
    const base::TimeTicks playout_time =
        GetPlayoutTime(encoded_frame->rtp_timestamp);

    // If we have multiple decodable frames, and the current frame is
    // too old, then skip it and decode the next frame instead.
    if (have_multiple_complete_frames && now > playout_time) {
      framer_.ReleaseFrame(encoded_frame->frame_id);
      continue;
    }

    // If |framer_| has a frame ready that is out of sequence, examine the
    // playout time to determine whether it's acceptable to continue, thereby
    // skipping one or more frames.  Skip if the missing frame wouldn't complete
    // playing before the start of playback of the available frame.
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

    // At this point, we have a decrypted EncodedFrame ready to be emitted.
    encoded_frame->reference_time = playout_time;
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
                                           base::Passed(&encoded_frame)));
    frame_request_queue_.pop_front();
  }
}

void VideoReceiver::EmitAvailableEncodedFramesAfterWaiting() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(is_waiting_for_consecutive_frame_);
  is_waiting_for_consecutive_frame_ = false;
  EmitAvailableEncodedFrames();
}

base::TimeTicks VideoReceiver::GetPlayoutTime(uint32 rtp_timestamp) const {
  return lip_sync_reference_time_ +
      lip_sync_drift_.Current() +
      RtpDeltaToTimeDelta(
          static_cast<int32>(rtp_timestamp - lip_sync_rtp_timestamp_),
          kVideoFrequency) +
      target_playout_delay_;
}

void VideoReceiver::IncomingPacket(scoped_ptr<Packet> packet) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (Rtcp::IsRtcpPacket(&packet->front(), packet->size())) {
    rtcp_.IncomingRtcpPacket(&packet->front(), packet->size());
  } else {
    ReceivedPacket(&packet->front(), packet->size());
  }
  if (!reports_are_scheduled_) {
    ScheduleNextRtcpReport();
    ScheduleNextCastMessage();
    reports_are_scheduled_ = true;
  }
}

void VideoReceiver::OnReceivedPayloadData(const uint8* payload_data,
                                          size_t payload_size,
                                          const RtpCastHeader& rtp_header) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  const base::TimeTicks now = cast_environment_->Clock()->NowTicks();

  frame_id_to_rtp_timestamp_[rtp_header.frame_id & 0xff] =
      rtp_header.rtp_timestamp;
  cast_environment_->Logging()->InsertPacketEvent(
      now,
      PACKET_RECEIVED,
      VIDEO_EVENT,
      rtp_header.rtp_timestamp,
      rtp_header.frame_id,
      rtp_header.packet_id,
      rtp_header.max_packet_id,
      payload_size);

  bool duplicate = false;
  const bool complete =
      framer_.InsertPacket(payload_data, payload_size, rtp_header, &duplicate);

  // Duplicate packets are ignored.
  if (duplicate)
    return;

  // Update lip-sync values upon receiving the first packet of each frame, or if
  // they have never been set yet.
  if (rtp_header.packet_id == 0 || lip_sync_reference_time_.is_null()) {
    RtpTimestamp fresh_sync_rtp;
    base::TimeTicks fresh_sync_reference;
    if (!rtcp_.GetLatestLipSyncTimes(&fresh_sync_rtp, &fresh_sync_reference)) {
      // HACK: The sender should have provided Sender Reports before the first
      // frame was sent.  However, the spec does not currently require this.
      // Therefore, when the data is missing, the local clock is used to
      // generate reference timestamps.
      VLOG(2) << "Lip sync info missing.  Falling-back to local clock.";
      fresh_sync_rtp = rtp_header.rtp_timestamp;
      fresh_sync_reference = now;
    }
    // |lip_sync_reference_time_| is always incremented according to the time
    // delta computed from the difference in RTP timestamps.  Then,
    // |lip_sync_drift_| accounts for clock drift and also smoothes-out any
    // sudden/discontinuous shifts in the series of reference time values.
    if (lip_sync_reference_time_.is_null()) {
      lip_sync_reference_time_ = fresh_sync_reference;
    } else {
      lip_sync_reference_time_ += RtpDeltaToTimeDelta(
          static_cast<int32>(fresh_sync_rtp - lip_sync_rtp_timestamp_),
          kVideoFrequency);
    }
    lip_sync_rtp_timestamp_ = fresh_sync_rtp;
    lip_sync_drift_.Update(
        now, fresh_sync_reference - lip_sync_reference_time_);
  }

  // Video frame not complete; wait for more packets.
  if (!complete)
    return;

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
      now, FRAME_ACK_SENT, VIDEO_EVENT,
      rtp_timestamp, cast_message.ack_frame_id_);

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
