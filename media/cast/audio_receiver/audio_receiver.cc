// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/audio_receiver/audio_receiver.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/audio_receiver/audio_decoder.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace {
const int kMinSchedulingDelayMs = 1;
// TODO(miu): This should go in AudioReceiverConfig.
const int kTypicalAudioFrameDurationMs = 10;
}  // namespace

namespace media {
namespace cast {

AudioReceiver::AudioReceiver(scoped_refptr<CastEnvironment> cast_environment,
                             const AudioReceiverConfig& audio_config,
                             transport::PacedPacketSender* const packet_sender)
    : RtpReceiver(cast_environment->Clock(), &audio_config, NULL),
      cast_environment_(cast_environment),
      event_subscriber_(kReceiverRtcpEventHistorySize, AUDIO_EVENT),
      codec_(audio_config.codec),
      frequency_(audio_config.frequency),
      target_playout_delay_(
          base::TimeDelta::FromMilliseconds(audio_config.rtp_max_delay_ms)),
      reports_are_scheduled_(false),
      framer_(cast_environment->Clock(),
              this,
              audio_config.incoming_ssrc,
              true,
              audio_config.rtp_max_delay_ms / kTypicalAudioFrameDurationMs),
      rtcp_(cast_environment,
            NULL,
            NULL,
            packet_sender,
            GetStatistics(),
            audio_config.rtcp_mode,
            base::TimeDelta::FromMilliseconds(audio_config.rtcp_interval),
            audio_config.feedback_ssrc,
            audio_config.incoming_ssrc,
            audio_config.rtcp_c_name,
            true),
      is_waiting_for_consecutive_frame_(false),
      lip_sync_drift_(ClockDriftSmoother::GetDefaultTimeConstant()),
      weak_factory_(this) {
  if (!audio_config.use_external_decoder)
    audio_decoder_.reset(new AudioDecoder(cast_environment, audio_config));
  decryptor_.Initialize(audio_config.aes_key, audio_config.aes_iv_mask);
  rtcp_.SetTargetDelay(target_playout_delay_);
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber_);
  memset(frame_id_to_rtp_timestamp_, 0, sizeof(frame_id_to_rtp_timestamp_));
}

AudioReceiver::~AudioReceiver() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber_);
}

void AudioReceiver::OnReceivedPayloadData(const uint8* payload_data,
                                          size_t payload_size,
                                          const RtpCastHeader& rtp_header) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  const base::TimeTicks now = cast_environment_->Clock()->NowTicks();

  frame_id_to_rtp_timestamp_[rtp_header.frame_id & 0xff] =
      rtp_header.rtp_timestamp;
  cast_environment_->Logging()->InsertPacketEvent(
      now, PACKET_RECEIVED, AUDIO_EVENT, rtp_header.rtp_timestamp,
      rtp_header.frame_id, rtp_header.packet_id, rtp_header.max_packet_id,
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
          frequency_);
    }
    lip_sync_rtp_timestamp_ = fresh_sync_rtp;
    lip_sync_drift_.Update(
        now, fresh_sync_reference - lip_sync_reference_time_);
  }

  // Frame not complete; wait for more packets.
  if (!complete)
    return;

  EmitAvailableEncodedFrames();
}

void AudioReceiver::GetRawAudioFrame(
    const AudioFrameDecodedCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!callback.is_null());
  DCHECK(audio_decoder_.get());
  GetEncodedAudioFrame(base::Bind(
      &AudioReceiver::DecodeEncodedAudioFrame,
      // Note: Use of Unretained is safe since this Closure is guaranteed to be
      // invoked before destruction of |this|.
      base::Unretained(this),
      callback));
}

void AudioReceiver::DecodeEncodedAudioFrame(
    const AudioFrameDecodedCallback& callback,
    scoped_ptr<transport::EncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!encoded_frame) {
    callback.Run(make_scoped_ptr<AudioBus>(NULL), base::TimeTicks(), false);
    return;
  }
  const uint32 frame_id = encoded_frame->frame_id;
  const uint32 rtp_timestamp = encoded_frame->rtp_timestamp;
  const base::TimeTicks playout_time = encoded_frame->reference_time;
  audio_decoder_->DecodeFrame(encoded_frame.Pass(),
                              base::Bind(&AudioReceiver::EmitRawAudioFrame,
                                         cast_environment_,
                                         callback,
                                         frame_id,
                                         rtp_timestamp,
                                         playout_time));
}

// static
void AudioReceiver::EmitRawAudioFrame(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const AudioFrameDecodedCallback& callback,
    uint32 frame_id,
    uint32 rtp_timestamp,
    const base::TimeTicks& playout_time,
    scoped_ptr<AudioBus> audio_bus,
    bool is_continuous) {
  DCHECK(cast_environment->CurrentlyOn(CastEnvironment::MAIN));
  if (audio_bus.get()) {
    const base::TimeTicks now = cast_environment->Clock()->NowTicks();
    cast_environment->Logging()->InsertFrameEvent(
        now, FRAME_DECODED, AUDIO_EVENT, rtp_timestamp, frame_id);
    cast_environment->Logging()->InsertFrameEventWithDelay(
        now, FRAME_PLAYOUT, AUDIO_EVENT, rtp_timestamp, frame_id,
        playout_time - now);
  }
  callback.Run(audio_bus.Pass(), playout_time, is_continuous);
}

void AudioReceiver::GetEncodedAudioFrame(const FrameEncodedCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  frame_request_queue_.push_back(callback);
  EmitAvailableEncodedFrames();
}

void AudioReceiver::EmitAvailableEncodedFrames() {
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
      VLOG(1) << "Wait for more audio packets to produce a completed frame.";
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
          now + base::TimeDelta::FromMilliseconds(kTypicalAudioFrameDurationMs);
      if (earliest_possible_end_time_of_missing_frame < playout_time) {
        VLOG(1) << "Wait for next consecutive frame instead of skipping.";
        if (!is_waiting_for_consecutive_frame_) {
          is_waiting_for_consecutive_frame_ = true;
          cast_environment_->PostDelayedTask(
              CastEnvironment::MAIN,
              FROM_HERE,
              base::Bind(&AudioReceiver::EmitAvailableEncodedFramesAfterWaiting,
                         weak_factory_.GetWeakPtr()),
              playout_time - now);
        }
        return;
      }
    }

    // Decrypt the payload data in the frame, if crypto is being used.
    if (decryptor_.initialized()) {
      std::string decrypted_audio_data;
      if (!decryptor_.Decrypt(encoded_frame->frame_id,
                              encoded_frame->data,
                              &decrypted_audio_data)) {
        // Decryption failed.  Give up on this frame, releasing it from the
        // jitter buffer.
        framer_.ReleaseFrame(encoded_frame->frame_id);
        continue;
      }
      encoded_frame->data.swap(decrypted_audio_data);
    }

    // At this point, we have a decrypted EncodedFrame ready to be emitted.
    encoded_frame->reference_time = playout_time;
    framer_.ReleaseFrame(encoded_frame->frame_id);
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(frame_request_queue_.front(),
                                           base::Passed(&encoded_frame)));
    frame_request_queue_.pop_front();
  }
}

void AudioReceiver::EmitAvailableEncodedFramesAfterWaiting() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(is_waiting_for_consecutive_frame_);
  is_waiting_for_consecutive_frame_ = false;
  EmitAvailableEncodedFrames();
}

base::TimeTicks AudioReceiver::GetPlayoutTime(uint32 rtp_timestamp) const {
  return lip_sync_reference_time_ +
      lip_sync_drift_.Current() +
      RtpDeltaToTimeDelta(
          static_cast<int32>(rtp_timestamp - lip_sync_rtp_timestamp_),
          frequency_) +
      target_playout_delay_;
}

void AudioReceiver::IncomingPacket(scoped_ptr<Packet> packet) {
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

void AudioReceiver::CastFeedback(const RtcpCastMessage& cast_message) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  RtpTimestamp rtp_timestamp =
      frame_id_to_rtp_timestamp_[cast_message.ack_frame_id_ & 0xff];
  cast_environment_->Logging()->InsertFrameEvent(
      now, FRAME_ACK_SENT, AUDIO_EVENT, rtp_timestamp,
      cast_message.ack_frame_id_);

  ReceiverRtcpEventSubscriber::RtcpEventMultiMap rtcp_events;
  event_subscriber_.GetRtcpEventsAndReset(&rtcp_events);
  rtcp_.SendRtcpFromRtpReceiver(&cast_message, &rtcp_events);
}

void AudioReceiver::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_send = rtcp_.TimeToSendNextRtcpReport() -
                                 cast_environment_->Clock()->NowTicks();

  time_to_send = std::max(
      time_to_send, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&AudioReceiver::SendNextRtcpReport,
                 weak_factory_.GetWeakPtr()),
      time_to_send);
}

void AudioReceiver::SendNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  // TODO(pwestin): add logging.
  rtcp_.SendRtcpFromRtpReceiver(NULL, NULL);
  ScheduleNextRtcpReport();
}

// Cast messages should be sent within a maximum interval. Schedule a call
// if not triggered elsewhere, e.g. by the cast message_builder.
void AudioReceiver::ScheduleNextCastMessage() {
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
      base::Bind(&AudioReceiver::SendNextCastMessage,
                 weak_factory_.GetWeakPtr()),
      time_to_send);
}

void AudioReceiver::SendNextCastMessage() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  framer_.SendCastMessage();  // Will only send a message if it is time.
  ScheduleNextCastMessage();
}

}  // namespace cast
}  // namespace media
