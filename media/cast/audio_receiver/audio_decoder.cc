// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/cast/audio_receiver/audio_decoder.h"

#include "third_party/webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "third_party/webrtc/modules/interface/module_common_types.h"

namespace media {
namespace cast {

AudioDecoder::AudioDecoder(scoped_refptr<CastEnvironment> cast_environment,
                           const AudioReceiverConfig& audio_config,
                           RtpPayloadFeedback* incoming_payload_feedback)
    : cast_environment_(cast_environment),
      audio_decoder_(webrtc::AudioCodingModule::Create(0)),
      cast_message_builder_(cast_environment->Clock(),
                            incoming_payload_feedback,
                            &frame_id_map_,
                            audio_config.incoming_ssrc,
                            true,
                            0),
      have_received_packets_(false),
      last_played_out_timestamp_(0) {
  audio_decoder_->InitializeReceiver();

  webrtc::CodecInst receive_codec;
  switch (audio_config.codec) {
    case transport::kPcm16:
      receive_codec.pltype = audio_config.rtp_payload_type;
      strncpy(receive_codec.plname, "L16", 4);
      receive_codec.plfreq = audio_config.frequency;
      receive_codec.pacsize = -1;
      receive_codec.channels = audio_config.channels;
      receive_codec.rate = -1;
      break;
    case transport::kOpus:
      receive_codec.pltype = audio_config.rtp_payload_type;
      strncpy(receive_codec.plname, "opus", 5);
      receive_codec.plfreq = audio_config.frequency;
      receive_codec.pacsize = -1;
      receive_codec.channels = audio_config.channels;
      receive_codec.rate = -1;
      break;
    case transport::kExternalAudio:
      NOTREACHED() << "Codec must be specified for audio decoder";
      break;
  }
  if (audio_decoder_->RegisterReceiveCodec(receive_codec) != 0) {
    NOTREACHED() << "Failed to register receive codec";
  }

  audio_decoder_->SetMaximumPlayoutDelay(audio_config.rtp_max_delay_ms);
  audio_decoder_->SetPlayoutMode(webrtc::streaming);
}

AudioDecoder::~AudioDecoder() {}

bool AudioDecoder::GetRawAudioFrame(int number_of_10ms_blocks,
                                    int desired_frequency,
                                    PcmAudioFrame* audio_frame,
                                    uint32* rtp_timestamp) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::AUDIO_DECODER));
  // We don't care about the race case where a packet arrives at the same time
  // as this function in called. The data will be there the next time this
  // function is called.
  lock_.Acquire();
  // Get a local copy under lock.
  bool have_received_packets = have_received_packets_;
  lock_.Release();

  if (!have_received_packets)
    return false;

  audio_frame->samples.clear();

  for (int i = 0; i < number_of_10ms_blocks; ++i) {
    webrtc::AudioFrame webrtc_audio_frame;
    if (0 != audio_decoder_->PlayoutData10Ms(desired_frequency,
                                             &webrtc_audio_frame)) {
      return false;
    }
    if (webrtc_audio_frame.speech_type_ == webrtc::AudioFrame::kPLCCNG ||
        webrtc_audio_frame.speech_type_ == webrtc::AudioFrame::kUndefined) {
      // We are only interested in real decoded audio.
      return false;
    }
    audio_frame->frequency = webrtc_audio_frame.sample_rate_hz_;
    audio_frame->channels = webrtc_audio_frame.num_channels_;

    if (i == 0) {
      // Use the timestamp from the first 10ms block.
      if (0 != audio_decoder_->PlayoutTimestamp(rtp_timestamp)) {
        return false;
      }
      lock_.Acquire();
      last_played_out_timestamp_ = *rtp_timestamp;
      lock_.Release();
    }
    int samples_per_10ms = webrtc_audio_frame.samples_per_channel_;

    audio_frame->samples.insert(
        audio_frame->samples.end(),
        &webrtc_audio_frame.data_[0],
        &webrtc_audio_frame.data_[samples_per_10ms * audio_frame->channels]);
  }
  return true;
}

void AudioDecoder::IncomingParsedRtpPacket(const uint8* payload_data,
                                           size_t payload_size,
                                           const RtpCastHeader& rtp_header) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK_LE(payload_size, kMaxIpPacketSize);
  audio_decoder_->IncomingPacket(
      payload_data, static_cast<int32>(payload_size), rtp_header.webrtc);
  lock_.Acquire();
  have_received_packets_ = true;
  uint32 last_played_out_timestamp = last_played_out_timestamp_;
  lock_.Release();

  PacketType packet_type = frame_id_map_.InsertPacket(rtp_header);
  if (packet_type != kNewPacketCompletingFrame)
    return;

  cast_message_builder_.CompleteFrameReceived(rtp_header.frame_id,
                                              rtp_header.is_key_frame);

  frame_id_rtp_timestamp_map_[rtp_header.frame_id] =
      rtp_header.webrtc.header.timestamp;

  if (last_played_out_timestamp == 0)
    return;  // Nothing is played out yet.

  uint32 latest_frame_id_to_remove = 0;
  bool frame_to_remove = false;

  FrameIdRtpTimestampMap::iterator it = frame_id_rtp_timestamp_map_.begin();
  while (it != frame_id_rtp_timestamp_map_.end()) {
    if (IsNewerRtpTimestamp(it->second, last_played_out_timestamp)) {
      break;
    }
    frame_to_remove = true;
    latest_frame_id_to_remove = it->first;
    frame_id_rtp_timestamp_map_.erase(it);
    it = frame_id_rtp_timestamp_map_.begin();
  }
  if (!frame_to_remove)
    return;

  frame_id_map_.RemoveOldFrames(latest_frame_id_to_remove);
}

bool AudioDecoder::TimeToSendNextCastMessage(base::TimeTicks* time_to_send) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  return cast_message_builder_.TimeToSendNextCastMessage(time_to_send);
}

void AudioDecoder::SendCastMessage() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_message_builder_.UpdateCastMessage();
}

}  // namespace cast
}  // namespace media
