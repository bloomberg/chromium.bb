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
      event_subscriber_(kReceiverRtcpEventHistorySize,
                        ReceiverRtcpEventSubscriber::kAudioEventSubscriber),
      codec_(audio_config.codec),
      frequency_(audio_config.frequency),
      target_delay_delta_(
          base::TimeDelta::FromMilliseconds(audio_config.rtp_max_delay_ms)),
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
            audio_config.rtcp_c_name),
      is_waiting_for_consecutive_frame_(false),
      weak_factory_(this) {
  if (!audio_config.use_external_decoder)
    audio_decoder_.reset(new AudioDecoder(cast_environment, audio_config));
  decryptor_.Initialize(audio_config.aes_key, audio_config.aes_iv_mask);
  rtcp_.SetTargetDelay(target_delay_delta_);
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber_);
  memset(frame_id_to_rtp_timestamp_, 0, sizeof(frame_id_to_rtp_timestamp_));
}

AudioReceiver::~AudioReceiver() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  // If any callbacks for encoded audio frames are queued, flush them out now.
  // This is critical because some Closures in |frame_request_queue_| may have
  // Unretained references to |this|.
  while (!frame_request_queue_.empty()) {
    frame_request_queue_.front().Run(
        make_scoped_ptr<transport::EncodedAudioFrame>(NULL), base::TimeTicks());
    frame_request_queue_.pop_front();
  }

  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber_);
}

void AudioReceiver::InitializeTimers() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  ScheduleNextRtcpReport();
  ScheduleNextCastMessage();
}

void AudioReceiver::OnReceivedPayloadData(const uint8* payload_data,
                                          size_t payload_size,
                                          const RtpCastHeader& rtp_header) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();

  // TODO(pwestin): update this as video to refresh over time.
  if (time_first_incoming_packet_.is_null()) {
    InitializeTimers();
    first_incoming_rtp_timestamp_ = rtp_header.webrtc.header.timestamp;
    time_first_incoming_packet_ = now;
  }

  frame_id_to_rtp_timestamp_[rtp_header.frame_id & 0xff] =
      rtp_header.webrtc.header.timestamp;
  cast_environment_->Logging()->InsertPacketEvent(
      now, kAudioPacketReceived, rtp_header.webrtc.header.timestamp,
      rtp_header.frame_id, rtp_header.packet_id, rtp_header.max_packet_id,
      payload_size);

  bool duplicate = false;
  const bool complete =
      framer_.InsertPacket(payload_data, payload_size, rtp_header, &duplicate);
  if (duplicate) {
    cast_environment_->Logging()->InsertPacketEvent(
        now,
        kDuplicateAudioPacketReceived,
        rtp_header.webrtc.header.timestamp,
        rtp_header.frame_id,
        rtp_header.packet_id,
        rtp_header.max_packet_id,
        payload_size);
    // Duplicate packets are ignored.
    return;
  }
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
    scoped_ptr<transport::EncodedAudioFrame> encoded_frame,
    const base::TimeTicks& playout_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!encoded_frame) {
    callback.Run(make_scoped_ptr<AudioBus>(NULL), playout_time, false);
    return;
  }
  const uint32 frame_id = encoded_frame->frame_id;
  const uint32 rtp_timestamp = encoded_frame->rtp_timestamp;
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
        now, kAudioFrameDecoded, rtp_timestamp, frame_id);
    cast_environment->Logging()->InsertFrameEventWithDelay(
        now, kAudioPlayoutDelay, rtp_timestamp, frame_id,
        playout_time - now);
  }
  callback.Run(audio_bus.Pass(), playout_time, is_continuous);
}

void AudioReceiver::GetEncodedAudioFrame(
    const AudioFrameEncodedCallback& callback) {
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
    scoped_ptr<transport::EncodedAudioFrame> encoded_frame(
        new transport::EncodedAudioFrame());
    bool is_consecutively_next_frame = false;
    if (!framer_.GetEncodedAudioFrame(encoded_frame.get(),
                                      &is_consecutively_next_frame)) {
      VLOG(1) << "Wait for more audio packets to produce a completed frame.";
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

    // At this point, we have a decrypted EncodedAudioFrame ready to be emitted.
    encoded_frame->codec = codec_;
    framer_.ReleaseFrame(encoded_frame->frame_id);
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(frame_request_queue_.front(),
                                           base::Passed(&encoded_frame),
                                           playout_time));
    frame_request_queue_.pop_front();
  }
}

void AudioReceiver::EmitAvailableEncodedFramesAfterWaiting() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(is_waiting_for_consecutive_frame_);
  is_waiting_for_consecutive_frame_ = false;
  EmitAvailableEncodedFrames();
}

void AudioReceiver::IncomingPacket(scoped_ptr<Packet> packet) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (Rtcp::IsRtcpPacket(&packet->front(), packet->size())) {
    rtcp_.IncomingRtcpPacket(&packet->front(), packet->size());
  } else {
    ReceivedPacket(&packet->front(), packet->size());
  }
}

void AudioReceiver::SetTargetDelay(base::TimeDelta target_delay) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  target_delay_delta_ = target_delay;
  rtcp_.SetTargetDelay(target_delay_delta_);
}

void AudioReceiver::CastFeedback(const RtcpCastMessage& cast_message) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  RtpTimestamp rtp_timestamp =
      frame_id_to_rtp_timestamp_[cast_message.ack_frame_id_ & 0xff];
  cast_environment_->Logging()->InsertFrameEvent(
      now, kAudioAckSent, rtp_timestamp, cast_message.ack_frame_id_);

  ReceiverRtcpEventSubscriber::RtcpEventMultiMap rtcp_events;
  event_subscriber_.GetRtcpEventsAndReset(&rtcp_events);
  rtcp_.SendRtcpFromRtpReceiver(&cast_message, &rtcp_events);
}

base::TimeTicks AudioReceiver::GetPlayoutTime(base::TimeTicks now,
                                              uint32 rtp_timestamp) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  // Senders time in ms when this frame was recorded.
  // Note: the senders clock and our local clock might not be synced.
  base::TimeTicks rtp_timestamp_in_ticks;
  base::TimeTicks playout_time;
  if (time_offset_ == base::TimeDelta()) {
    if (rtcp_.RtpTimestampInSenderTime(frequency_,
                                       first_incoming_rtp_timestamp_,
                                       &rtp_timestamp_in_ticks)) {
      time_offset_ = time_first_incoming_packet_ - rtp_timestamp_in_ticks;
      // TODO(miu): As clocks drift w.r.t. each other, and other factors take
      // effect, |time_offset_| should be updated.  Otherwise, we might as well
      // always compute the time offsets agnostic of RTCP's time data.
    } else {
      // We have not received any RTCP to sync the stream play it out as soon as
      // possible.

      // BUG: This means we're literally switching to a different timeline a
      // short time after a cast receiver has been running.  Re-enable
      // End2EndTest.StartSenderBeforeReceiver once this is fixed.
      // http://crbug.com/356942
      uint32 rtp_timestamp_diff = rtp_timestamp - first_incoming_rtp_timestamp_;

      int frequency_khz = frequency_ / 1000;
      base::TimeDelta rtp_time_diff_delta =
          base::TimeDelta::FromMilliseconds(rtp_timestamp_diff / frequency_khz);
      base::TimeDelta time_diff_delta = now - time_first_incoming_packet_;

      playout_time = now + std::max(rtp_time_diff_delta - time_diff_delta,
                                    base::TimeDelta());
    }
  }
  if (playout_time.is_null()) {
    // This can fail if we have not received any RTCP packets in a long time.
    if (rtcp_.RtpTimestampInSenderTime(frequency_, rtp_timestamp,
                                       &rtp_timestamp_in_ticks)) {
      playout_time =
          rtp_timestamp_in_ticks + time_offset_ + target_delay_delta_;
    } else {
      playout_time = now;
    }
  }

  // TODO(miu): This is broken since we literally switch timelines once |rtcp_|
  // can provide us the |time_offset_|.  Furthermore, this "getter" method may
  // be called on frames received out-of-order, which means the playout times
  // for earlier frames will be computed incorrectly.
#if 0
  // Don't allow the playout time to go backwards.
  if (last_playout_time_ > playout_time) playout_time = last_playout_time_;
  last_playout_time_ = playout_time;
#endif

  return playout_time;
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
