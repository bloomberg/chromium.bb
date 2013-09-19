// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/audio_receiver/audio_receiver.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/audio_receiver/audio_decoder.h"
#include "media/cast/framer/framer.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_receiver/rtp_receiver.h"
#include "third_party/webrtc/modules/interface/module_common_types.h"
#include "third_party/webrtc/system_wrappers/interface/sleep.h"
#include "third_party/webrtc/system_wrappers/interface/tick_util.h"

static const int64 kMaxFrameWaitMs = 20;
const int64 kMinSchedulingDelayMs = 1;

namespace media {
namespace cast {


// Local implementation of RtpData (defined in rtp_rtcp_defines.h).
// Used to pass payload data into the audio receiver.
class LocalRtpAudioData : public RtpData {
 public:
  explicit LocalRtpAudioData(AudioReceiver* audio_receiver)
      : audio_receiver_(audio_receiver),
        time_first_incoming_packet_(),
        first_incoming_rtp_timestamp_(0),
        default_tick_clock_(new base::DefaultTickClock()),
        clock_(default_tick_clock_.get()) {}

  virtual void OnReceivedPayloadData(
      const uint8* payload_data,
      int payload_size,
      const RtpCastHeader* rtp_header) OVERRIDE {
    if (time_first_incoming_packet_.is_null()) {
      first_incoming_rtp_timestamp_ = rtp_header->webrtc.header.timestamp;
      time_first_incoming_packet_ = clock_->NowTicks();
    }
    audio_receiver_->IncomingParsedRtpPacket(payload_data, payload_size,
                                             *rtp_header);
  }

  void GetFirstPacketInformation(base::TimeTicks* time_incoming_packet,
                                 uint32* incoming_rtp_timestamp) {
    *time_incoming_packet = time_first_incoming_packet_;
    *incoming_rtp_timestamp = first_incoming_rtp_timestamp_;
  }

 private:
  AudioReceiver* audio_receiver_;
  base::TimeTicks time_first_incoming_packet_;
  uint32 first_incoming_rtp_timestamp_;
  scoped_ptr<base::TickClock> default_tick_clock_;
  base::TickClock* clock_;
};

// Local implementation of RtpPayloadFeedback (defined in rtp_defines.h)
// Used to convey cast-specific feedback from receiver to sender.
class LocalRtpAudioFeedback : public RtpPayloadFeedback {
 public:
  explicit LocalRtpAudioFeedback(AudioReceiver* audio_receiver)
      : audio_receiver_(audio_receiver) {
  }

  virtual void CastFeedback(const RtcpCastMessage& cast_message) OVERRIDE {
    audio_receiver_->CastFeedback(cast_message);
  }

  virtual void RequestKeyFrame() OVERRIDE {
    DCHECK(false) << "Invalid callback";
  }

 private:
  AudioReceiver* audio_receiver_;
};

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


AudioReceiver::AudioReceiver(scoped_refptr<CastThread> cast_thread,
                             const AudioReceiverConfig& audio_config,
                             PacedPacketSender* const packet_sender)
    : cast_thread_(cast_thread),
      codec_(audio_config.codec),
      incoming_ssrc_(audio_config.incoming_ssrc),
      frequency_(audio_config.frequency),
      audio_buffer_(),
      audio_decoder_(),
      time_offset_(),
      default_tick_clock_(new base::DefaultTickClock()),
      clock_(default_tick_clock_.get()),
      weak_factory_(this) {
  target_delay_delta_ =
      base::TimeDelta::FromMilliseconds(audio_config.rtp_max_delay_ms);
  incoming_payload_callback_.reset(new LocalRtpAudioData(this));
  incoming_payload_feedback_.reset(new LocalRtpAudioFeedback(this));
  if (audio_config.use_external_decoder) {
    audio_buffer_.reset(new Framer(incoming_payload_feedback_.get(),
                               audio_config.incoming_ssrc,
                               true,
                               0));
  } else {
    audio_decoder_ = new AudioDecoder(cast_thread_, audio_config);
  }
  rtp_receiver_.reset(new RtpReceiver(&audio_config,
                                  NULL,
                                  incoming_payload_callback_.get()));
  rtp_audio_receiver_statistics_.reset(
      new LocalRtpReceiverStatistics(rtp_receiver_.get()));
  base::TimeDelta rtcp_interval_delta =
      base::TimeDelta::FromMilliseconds(audio_config.rtcp_interval);
  rtcp_.reset(new Rtcp(NULL,
                   packet_sender,
                   NULL,
                   rtp_audio_receiver_statistics_.get(),
                   audio_config.rtcp_mode,
                   rtcp_interval_delta,
                   false,
                   audio_config.feedback_ssrc,
                   audio_config.rtcp_c_name));
  rtcp_->SetRemoteSSRC(audio_config.incoming_ssrc);
  ScheduleNextRtcpReport();
}

AudioReceiver::~AudioReceiver() {}

void AudioReceiver::IncomingParsedRtpPacket(const uint8* payload_data,
                                            int payload_size,
                                            const RtpCastHeader& rtp_header) {
  if (audio_decoder_) {
    DCHECK(!audio_buffer_) << "Invalid internal state";
    audio_decoder_->IncomingParsedRtpPacket(payload_data, payload_size,
                                            rtp_header);
    return;
  }
  if (audio_buffer_) {
    DCHECK(!audio_decoder_) << "Invalid internal state";
    audio_buffer_->InsertPacket(payload_data, payload_size, rtp_header);
  }
}

void AudioReceiver::GetRawAudioFrame(int number_of_10ms_blocks,
                                     int desired_frequency,
                                     const AudioFrameDecodedCallback callback) {
  DCHECK(audio_decoder_) << "Invalid function call in this configuration";

  cast_thread_->PostTask(CastThread::AUDIO_DECODER, FROM_HERE, base::Bind(
      &AudioReceiver::DecodeAudioFrameThread, weak_factory_.GetWeakPtr(),
      number_of_10ms_blocks, desired_frequency, callback));
}

void AudioReceiver::DecodeAudioFrameThread(
    int number_of_10ms_blocks,
    int desired_frequency,
    const AudioFrameDecodedCallback callback) {
  // TODO(mikhal): Allow the application to allocate this memory.
  scoped_ptr<PcmAudioFrame> audio_frame(new PcmAudioFrame());

  uint32 rtp_timestamp = 0;
  if (!audio_decoder_->GetRawAudioFrame(number_of_10ms_blocks,
                                        desired_frequency,
                                        audio_frame.get(),
                                        &rtp_timestamp)) {
    return;
  }
  base::TimeTicks now = clock_->NowTicks();
  base::TimeTicks playout_time;
  playout_time = GetPlayoutTime(now, rtp_timestamp);

  // Frame is ready - Send back to the main thread.
  cast_thread_->PostTask(CastThread::MAIN, FROM_HERE,
      base::Bind(callback,
      base::Passed(&audio_frame), playout_time));
}

bool AudioReceiver::GetEncodedAudioFrame(EncodedAudioFrame* encoded_frame,
                                         base::TimeTicks* playout_time) {
  DCHECK(audio_buffer_) << "Invalid function call in this configuration";

  uint32 rtp_timestamp = 0;
  bool next_frame = false;
  base::TimeTicks timeout = clock_->NowTicks() +
      base::TimeDelta::FromMilliseconds(kMaxFrameWaitMs);
  if (!audio_buffer_->GetEncodedAudioFrame(timeout, encoded_frame,
                                           &rtp_timestamp, &next_frame)) {
    return false;
  }
  base::TimeTicks now = clock_->NowTicks();
  *playout_time = GetPlayoutTime(now, rtp_timestamp);

  base::TimeDelta time_until_playout = now - *playout_time;
  base::TimeDelta time_until_release = time_until_playout -
      base::TimeDelta::FromMilliseconds(kMaxFrameWaitMs);
  base::TimeDelta zero_delta = base::TimeDelta::FromMilliseconds(0);
  if (!next_frame && (time_until_release  > zero_delta)) {
    // Relying on the application to keep polling.
    return false;
  }
  encoded_frame->codec = codec_;
  return true;
}

void AudioReceiver::ReleaseFrame(uint8 frame_id) {
  audio_buffer_->ReleaseFrame(frame_id);
}

void AudioReceiver::IncomingPacket(const uint8* packet, int length) {
  bool rtcp_packet = Rtcp::IsRtcpPacket(packet, length);
  if (!rtcp_packet) {
    rtp_receiver_->ReceivedPacket(packet, length);
  } else {
    rtcp_->IncomingRtcpPacket(packet, length);
  }
}

void AudioReceiver::CastFeedback(const RtcpCastMessage& cast_message) {
  rtcp_->SendRtcpCast(cast_message);
}

base::TimeTicks AudioReceiver::GetPlayoutTime(base::TimeTicks now,
                                              uint32 rtp_timestamp) {
  // Senders time in ms when this frame was recorded.
  // Note: the senders clock and our local clock might not be synced.
  base::TimeTicks rtp_timestamp_in_ticks;
  base::TimeDelta zero_delta = base::TimeDelta::FromMilliseconds(0);
  if (time_offset_ == zero_delta) {
    base::TimeTicks time_first_incoming_packet;
    uint32 first_incoming_rtp_timestamp;

    incoming_payload_callback_->GetFirstPacketInformation(
        &time_first_incoming_packet, &first_incoming_rtp_timestamp);

    if (rtcp_->RtpTimestampInSenderTime(frequency_,
                                        first_incoming_rtp_timestamp,
                                        &rtp_timestamp_in_ticks)) {
      time_offset_ = time_first_incoming_packet - rtp_timestamp_in_ticks;
    } else {
      // We have not received any RTCP to sync the stream play it out as soon as
      // possible.
      uint32 rtp_timestamp_diff =
          rtp_timestamp - first_incoming_rtp_timestamp;

      int frequency_khz = frequency_ / 1000;
      base::TimeDelta rtp_time_diff_delta =
          base::TimeDelta::FromMilliseconds(rtp_timestamp_diff / frequency_khz);
      base::TimeDelta time_diff_delta = now - time_first_incoming_packet;
      if (rtp_time_diff_delta > time_diff_delta) {
        return (now + (rtp_time_diff_delta - time_diff_delta));
      } else {
        return now;
      }
    }
  }
  // This can fail if we have not received any RTCP packets in a long time.
  if (rtcp_->RtpTimestampInSenderTime(frequency_, rtp_timestamp,
                                      &rtp_timestamp_in_ticks)) {
    return (rtp_timestamp_in_ticks + time_offset_ + target_delay_delta_);
  } else {
    return now;
  }
}

void AudioReceiver::ScheduleNextRtcpReport() {
  base::TimeDelta time_to_send =
      rtcp_->TimeToSendNextRtcpReport() - clock_->NowTicks();

  time_to_send = std::max(time_to_send,
      base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));

  cast_thread_->PostDelayedTask(CastThread::MAIN, FROM_HERE,
      base::Bind(&AudioReceiver::SendNextRtcpReport,
      weak_factory_.GetWeakPtr()), time_to_send);
}

void AudioReceiver::SendNextRtcpReport() {
  rtcp_->SendRtcpReport(incoming_ssrc_);
  ScheduleNextRtcpReport();
}

}  // namespace cast
}  // namespace media
