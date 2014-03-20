// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/audio_receiver/audio_receiver.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_piece.h"
#include "media/cast/audio_receiver/audio_decoder.h"
#include "media/cast/framer/framer.h"
#include "media/cast/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_receiver/rtp_receiver.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace {

// Max time we wait until an audio frame is due to be played out is released.
static const int64 kMaxAudioFrameWaitMs = 20;
static const int64 kMinSchedulingDelayMs = 1;

}  // namespace

namespace media {
namespace cast {

DecodedAudioCallbackData::DecodedAudioCallbackData()
    : number_of_10ms_blocks(0), desired_frequency(0), callback() {}

DecodedAudioCallbackData::~DecodedAudioCallbackData() {}

// Local implementation of RtpData (defined in rtp_rtcp_defines.h).
// Used to pass payload data into the audio receiver.
class LocalRtpAudioData : public RtpData {
 public:
  explicit LocalRtpAudioData(AudioReceiver* audio_receiver)
      : audio_receiver_(audio_receiver) {}

  virtual void OnReceivedPayloadData(const uint8* payload_data,
                                     size_t payload_size,
                                     const RtpCastHeader* rtp_header) OVERRIDE {
    audio_receiver_->IncomingParsedRtpPacket(payload_data, payload_size,
                                             *rtp_header);
  }

 private:
  AudioReceiver* audio_receiver_;
};

// Local implementation of RtpPayloadFeedback (defined in rtp_defines.h)
// Used to convey cast-specific feedback from receiver to sender.
class LocalRtpAudioFeedback : public RtpPayloadFeedback {
 public:
  explicit LocalRtpAudioFeedback(AudioReceiver* audio_receiver)
      : audio_receiver_(audio_receiver) {}

  virtual void CastFeedback(const RtcpCastMessage& cast_message) OVERRIDE {
    audio_receiver_->CastFeedback(cast_message);
  }

 private:
  AudioReceiver* audio_receiver_;
};

class LocalRtpReceiverStatistics : public RtpReceiverStatistics {
 public:
  explicit LocalRtpReceiverStatistics(RtpReceiver* rtp_receiver)
      : rtp_receiver_(rtp_receiver) {}

  virtual void GetStatistics(uint8* fraction_lost,
                             uint32* cumulative_lost,  // 24 bits valid.
                             uint32* extended_high_sequence_number,
                             uint32* jitter) OVERRIDE {
    rtp_receiver_->GetStatistics(fraction_lost, cumulative_lost,
                                 extended_high_sequence_number, jitter);
  }

 private:
  RtpReceiver* rtp_receiver_;
};

AudioReceiver::AudioReceiver(scoped_refptr<CastEnvironment> cast_environment,
                             const AudioReceiverConfig& audio_config,
                             transport::PacedPacketSender* const packet_sender)
    : cast_environment_(cast_environment),
      event_subscriber_(kReceiverRtcpEventHistorySize,
                        ReceiverRtcpEventSubscriber::kAudioEventSubscriber),
      codec_(audio_config.codec),
      frequency_(audio_config.frequency),
      audio_buffer_(),
      audio_decoder_(),
      time_offset_(),
      weak_factory_(this) {
  target_delay_delta_ =
      base::TimeDelta::FromMilliseconds(audio_config.rtp_max_delay_ms);
  incoming_payload_callback_.reset(new LocalRtpAudioData(this));
  incoming_payload_feedback_.reset(new LocalRtpAudioFeedback(this));
  if (audio_config.use_external_decoder) {
    audio_buffer_.reset(new Framer(cast_environment->Clock(),
                                   incoming_payload_feedback_.get(),
                                   audio_config.incoming_ssrc, true, 0));
  } else {
    audio_decoder_.reset(new AudioDecoder(cast_environment, audio_config,
                                          incoming_payload_feedback_.get()));
  }
  decryptor_.Initialize(audio_config.aes_key, audio_config.aes_iv_mask);
  rtp_receiver_.reset(new RtpReceiver(cast_environment->Clock(),
                                      &audio_config,
                                      NULL,
                                      incoming_payload_callback_.get()));
  rtp_audio_receiver_statistics_.reset(
      new LocalRtpReceiverStatistics(rtp_receiver_.get()));
  base::TimeDelta rtcp_interval_delta =
      base::TimeDelta::FromMilliseconds(audio_config.rtcp_interval);
  rtcp_.reset(new Rtcp(cast_environment, NULL, NULL, packet_sender,
                       rtp_audio_receiver_statistics_.get(),
                       audio_config.rtcp_mode, rtcp_interval_delta,
                       audio_config.feedback_ssrc, audio_config.incoming_ssrc,
                       audio_config.rtcp_c_name));
  // Set the target delay that will be conveyed to the sender.
  rtcp_->SetTargetDelay(target_delay_delta_);
  cast_environment_->Logging()->AddRawEventSubscriber(&event_subscriber_);
  memset(frame_id_to_rtp_timestamp_, 0, sizeof(frame_id_to_rtp_timestamp_));
}

AudioReceiver::~AudioReceiver() {
  cast_environment_->Logging()->RemoveRawEventSubscriber(&event_subscriber_);
}

void AudioReceiver::InitializeTimers() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  ScheduleNextRtcpReport();
  ScheduleNextCastMessage();
}

void AudioReceiver::IncomingParsedRtpPacket(const uint8* payload_data,
                                            size_t payload_size,
                                            const RtpCastHeader& rtp_header) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();

  frame_id_to_rtp_timestamp_[rtp_header.frame_id & 0xff] =
      rtp_header.webrtc.header.timestamp;
  cast_environment_->Logging()->InsertPacketEvent(
      now, kAudioPacketReceived, rtp_header.webrtc.header.timestamp,
      rtp_header.frame_id, rtp_header.packet_id, rtp_header.max_packet_id,
      payload_size);

  // TODO(pwestin): update this as video to refresh over time.
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (time_first_incoming_packet_.is_null()) {
    InitializeTimers();
    first_incoming_rtp_timestamp_ = rtp_header.webrtc.header.timestamp;
    time_first_incoming_packet_ = now;
  }

  if (audio_decoder_) {
    DCHECK(!audio_buffer_) << "Invalid internal state";
    std::string plaintext;
    if (decryptor_.initialized()) {
      if (!decryptor_.Decrypt(
               rtp_header.frame_id,
               base::StringPiece(reinterpret_cast<const char*>(payload_data),
                                 payload_size),
               &plaintext))
        return;
    } else {
      plaintext.append(reinterpret_cast<const char*>(payload_data),
                       payload_size);
    }
    audio_decoder_->IncomingParsedRtpPacket(
        reinterpret_cast<const uint8*>(plaintext.data()), plaintext.size(),
        rtp_header);
    if (!queued_decoded_callbacks_.empty()) {
      DecodedAudioCallbackData decoded_data = queued_decoded_callbacks_.front();
      queued_decoded_callbacks_.pop_front();
      cast_environment_->PostTask(
          CastEnvironment::AUDIO_DECODER, FROM_HERE,
          base::Bind(&AudioReceiver::DecodeAudioFrameThread,
                     base::Unretained(this), decoded_data.number_of_10ms_blocks,
                     decoded_data.desired_frequency, decoded_data.callback));
    }
    return;
  }

  DCHECK(audio_buffer_) << "Invalid internal state";
  DCHECK(!audio_decoder_) << "Invalid internal state";

  bool duplicate = false;
  bool complete = audio_buffer_->InsertPacket(payload_data, payload_size,
                                              rtp_header, &duplicate);
  if (duplicate) {
    cast_environment_->Logging()->InsertPacketEvent(
        now, kDuplicateAudioPacketReceived, rtp_header.webrtc.header.timestamp,
        rtp_header.frame_id, rtp_header.packet_id, rtp_header.max_packet_id,
        payload_size);
    // Duplicate packets are ignored.
    return;
  }
  if (!complete) return;  // Audio frame not complete; wait for more packets.
  if (queued_encoded_callbacks_.empty()) return;
  AudioFrameEncodedCallback callback = queued_encoded_callbacks_.front();
  queued_encoded_callbacks_.pop_front();
  cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
                              base::Bind(&AudioReceiver::GetEncodedAudioFrame,
                                         weak_factory_.GetWeakPtr(), callback));
}

void AudioReceiver::GetRawAudioFrame(
    int number_of_10ms_blocks, int desired_frequency,
    const AudioFrameDecodedCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(audio_decoder_) << "Invalid function call in this configuration";
  // TODO(pwestin): we can skip this function by posting direct to the decoder.
  cast_environment_->PostTask(
      CastEnvironment::AUDIO_DECODER, FROM_HERE,
      base::Bind(&AudioReceiver::DecodeAudioFrameThread, base::Unretained(this),
                 number_of_10ms_blocks, desired_frequency, callback));
}

void AudioReceiver::DecodeAudioFrameThread(
    int number_of_10ms_blocks, int desired_frequency,
    const AudioFrameDecodedCallback callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::AUDIO_DECODER));
  // TODO(mikhal): Allow the application to allocate this memory.
  scoped_ptr<PcmAudioFrame> audio_frame(new PcmAudioFrame());

  uint32 rtp_timestamp = 0;
  if (!audio_decoder_->GetRawAudioFrame(number_of_10ms_blocks,
                                        desired_frequency, audio_frame.get(),
                                        &rtp_timestamp)) {
    DecodedAudioCallbackData callback_data;
    callback_data.number_of_10ms_blocks = number_of_10ms_blocks;
    callback_data.desired_frequency = desired_frequency;
    callback_data.callback = callback;
    queued_decoded_callbacks_.push_back(callback_data);
    return;
  }

  cast_environment_->PostTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(&AudioReceiver::ReturnDecodedFrameWithPlayoutDelay,
                 base::Unretained(this), base::Passed(&audio_frame),
                 rtp_timestamp, callback));
}

void AudioReceiver::ReturnDecodedFrameWithPlayoutDelay(
    scoped_ptr<PcmAudioFrame> audio_frame, uint32 rtp_timestamp,
    const AudioFrameDecodedCallback callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  cast_environment_->Logging()->InsertFrameEvent(
      now, kAudioFrameDecoded, rtp_timestamp, kFrameIdUnknown);

  base::TimeTicks playout_time = GetPlayoutTime(now, rtp_timestamp);

  cast_environment_->Logging()->InsertFrameEventWithDelay(
      now, kAudioPlayoutDelay, rtp_timestamp, kFrameIdUnknown,
      playout_time - now);

  // Frame is ready - Send back to the caller.
  cast_environment_->PostTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(callback, base::Passed(&audio_frame), playout_time));
}

void AudioReceiver::PlayoutTimeout() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(audio_buffer_) << "Invalid function call in this configuration";
  if (queued_encoded_callbacks_.empty()) {
    // Already released by incoming packet.
    return;
  }
  bool next_frame = false;
  scoped_ptr<transport::EncodedAudioFrame> encoded_frame(
      new transport::EncodedAudioFrame());

  if (!audio_buffer_->GetEncodedAudioFrame(encoded_frame.get(), &next_frame)) {
    // We have no audio frames. Wait for new packet(s).
    // Since the application can post multiple AudioFrameEncodedCallback and
    // we only check the next frame to play out we might have multiple timeout
    // events firing after each other; however this should be a rare event.
    VLOG(1) << "Failed to retrieved a complete frame at this point in time";
    return;
  }

  if (decryptor_.initialized() && !DecryptAudioFrame(&encoded_frame)) {
    // Logging already done.
    return;
  }

  if (PostEncodedAudioFrame(
          queued_encoded_callbacks_.front(), next_frame, &encoded_frame)) {
    // Call succeed remove callback from list.
    queued_encoded_callbacks_.pop_front();
  }
}

void AudioReceiver::GetEncodedAudioFrame(
    const AudioFrameEncodedCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(audio_buffer_) << "Invalid function call in this configuration";

  bool next_frame = false;
  scoped_ptr<transport::EncodedAudioFrame> encoded_frame(
      new transport::EncodedAudioFrame());

  if (!audio_buffer_->GetEncodedAudioFrame(encoded_frame.get(), &next_frame)) {
    // We have no audio frames. Wait for new packet(s).
    VLOG(1) << "Wait for more audio packets in frame";
    queued_encoded_callbacks_.push_back(callback);
    return;
  }
  if (decryptor_.initialized() && !DecryptAudioFrame(&encoded_frame)) {
    // Logging already done.
    queued_encoded_callbacks_.push_back(callback);
    return;
  }
  if (!PostEncodedAudioFrame(callback, next_frame, &encoded_frame)) {
    // We have an audio frame; however we are missing packets and we have time
    // to wait for new packet(s).
    queued_encoded_callbacks_.push_back(callback);
  }
}

bool AudioReceiver::PostEncodedAudioFrame(
    const AudioFrameEncodedCallback& callback,
    bool next_frame,
    scoped_ptr<transport::EncodedAudioFrame>* encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(audio_buffer_) << "Invalid function call in this configuration";
  DCHECK(encoded_frame) << "Invalid encoded_frame";

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  base::TimeTicks playout_time =
      GetPlayoutTime(now, (*encoded_frame)->rtp_timestamp);
  base::TimeDelta time_until_playout = playout_time - now;
  base::TimeDelta min_wait_delta =
      base::TimeDelta::FromMilliseconds(kMaxAudioFrameWaitMs);

  if (!next_frame && (time_until_playout > min_wait_delta)) {
    base::TimeDelta time_until_release = time_until_playout - min_wait_delta;
    cast_environment_->PostDelayedTask(
        CastEnvironment::MAIN, FROM_HERE,
        base::Bind(&AudioReceiver::PlayoutTimeout, weak_factory_.GetWeakPtr()),
        time_until_release);
    VLOG(1) << "Wait until time to playout:"
            << time_until_release.InMilliseconds();
    return false;
  }
  (*encoded_frame)->codec = codec_;
  audio_buffer_->ReleaseFrame((*encoded_frame)->frame_id);

  cast_environment_->PostTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(callback, base::Passed(encoded_frame), playout_time));
  return true;
}

void AudioReceiver::IncomingPacket(scoped_ptr<Packet> packet) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  bool rtcp_packet = Rtcp::IsRtcpPacket(&packet->front(), packet->size());
  if (!rtcp_packet) {
    rtp_receiver_->ReceivedPacket(&packet->front(), packet->size());
  } else {
    rtcp_->IncomingRtcpPacket(&packet->front(), packet->size());
  }
}

void AudioReceiver::SetTargetDelay(base::TimeDelta target_delay) {
  target_delay_delta_ = target_delay;
  rtcp_->SetTargetDelay(target_delay_delta_);
}

void AudioReceiver::CastFeedback(const RtcpCastMessage& cast_message) {
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  RtpTimestamp rtp_timestamp =
      frame_id_to_rtp_timestamp_[cast_message.ack_frame_id_ & 0xff];
  cast_environment_->Logging()->InsertFrameEvent(
      now, kAudioAckSent, rtp_timestamp, cast_message.ack_frame_id_);

  rtcp_->SendRtcpFromRtpReceiver(&cast_message, &event_subscriber_);
}

base::TimeTicks AudioReceiver::GetPlayoutTime(base::TimeTicks now,
                                              uint32 rtp_timestamp) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  // Senders time in ms when this frame was recorded.
  // Note: the senders clock and our local clock might not be synced.
  base::TimeTicks rtp_timestamp_in_ticks;
  base::TimeTicks playout_time;
  if (time_offset_ == base::TimeDelta()) {
    if (rtcp_->RtpTimestampInSenderTime(frequency_,
                                        first_incoming_rtp_timestamp_,
                                        &rtp_timestamp_in_ticks)) {
      time_offset_ = time_first_incoming_packet_ - rtp_timestamp_in_ticks;
    } else {
      // We have not received any RTCP to sync the stream play it out as soon as
      // possible.
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
    if (rtcp_->RtpTimestampInSenderTime(frequency_, rtp_timestamp,
                                        &rtp_timestamp_in_ticks)) {
      playout_time =
          rtp_timestamp_in_ticks + time_offset_ + target_delay_delta_;
    } else {
      playout_time = now;
    }
  }
  // Don't allow the playout time to go backwards.
  if (last_playout_time_ > playout_time) playout_time = last_playout_time_;
  last_playout_time_ = playout_time;
  return playout_time;
}

bool AudioReceiver::DecryptAudioFrame(
    scoped_ptr<transport::EncodedAudioFrame>* audio_frame) {
  if (!decryptor_.initialized())
    return false;

  std::string decrypted_audio_data;
  if (!decryptor_.Decrypt((*audio_frame)->frame_id,
                          (*audio_frame)->data,
                          &decrypted_audio_data)) {
    // Give up on this frame, release it from the jitter buffer.
    audio_buffer_->ReleaseFrame((*audio_frame)->frame_id);
    return false;
  }
  (*audio_frame)->data.swap(decrypted_audio_data);
  return true;
}

void AudioReceiver::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeDelta time_to_send = rtcp_->TimeToSendNextRtcpReport() -
                                 cast_environment_->Clock()->NowTicks();

  time_to_send = std::max(
      time_to_send, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(&AudioReceiver::SendNextRtcpReport,
                 weak_factory_.GetWeakPtr()),
      time_to_send);
}

void AudioReceiver::SendNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  // TODO(pwestin): add logging.
  rtcp_->SendRtcpFromRtpReceiver(NULL, NULL);
  ScheduleNextRtcpReport();
}

// Cast messages should be sent within a maximum interval. Schedule a call
// if not triggered elsewhere, e.g. by the cast message_builder.
void AudioReceiver::ScheduleNextCastMessage() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeTicks send_time;
  if (audio_buffer_) {
    audio_buffer_->TimeToSendNextCastMessage(&send_time);
  } else if (audio_decoder_) {
    audio_decoder_->TimeToSendNextCastMessage(&send_time);
  } else {
    NOTREACHED();
  }
  base::TimeDelta time_to_send =
      send_time - cast_environment_->Clock()->NowTicks();
  time_to_send = std::max(
      time_to_send, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));
  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(&AudioReceiver::SendNextCastMessage,
                 weak_factory_.GetWeakPtr()),
      time_to_send);
}

void AudioReceiver::SendNextCastMessage() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  if (audio_buffer_) {
    // Will only send a message if it is time.
    audio_buffer_->SendCastMessage();
  }
  if (audio_decoder_) {
    // Will only send a message if it is time.
    audio_decoder_->SendCastMessage();
  }
  ScheduleNextCastMessage();
}

}  // namespace cast
}  // namespace media
