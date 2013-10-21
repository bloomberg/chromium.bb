// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/cast/audio_receiver/audio_decoder.h"

#include "third_party/webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "third_party/webrtc/modules/interface/module_common_types.h"

namespace media {
namespace cast {

AudioDecoder::AudioDecoder(const AudioReceiverConfig& audio_config)
    : audio_decoder_(webrtc::AudioCodingModule::Create(0)),
      have_received_packets_(false) {
  audio_decoder_->InitializeReceiver();

  webrtc::CodecInst receive_codec;
  switch (audio_config.codec) {
    case kPcm16:
      receive_codec.pltype = audio_config.rtp_payload_type;
      strncpy(receive_codec.plname, "L16", 4);
      receive_codec.plfreq = audio_config.frequency;
      receive_codec.pacsize = -1;
      receive_codec.channels = audio_config.channels;
      receive_codec.rate = -1;
      break;
    case kOpus:
      receive_codec.pltype = audio_config.rtp_payload_type;
      strncpy(receive_codec.plname, "opus", 5);
      receive_codec.plfreq = audio_config.frequency;
      receive_codec.pacsize = -1;
      receive_codec.channels = audio_config.channels;
      receive_codec.rate = -1;
      break;
    case kExternalAudio:
      DCHECK(false) << "Codec must be specified for audio decoder";
      break;
  }
  if (audio_decoder_->RegisterReceiveCodec(receive_codec) != 0) {
    DCHECK(false) << "Failed to register receive codec";
  }

  audio_decoder_->SetMaximumPlayoutDelay(audio_config.rtp_max_delay_ms);
  audio_decoder_->SetPlayoutMode(webrtc::streaming);
}

AudioDecoder::~AudioDecoder() {}

bool AudioDecoder::GetRawAudioFrame(int number_of_10ms_blocks,
                                    int desired_frequency,
                                    PcmAudioFrame* audio_frame,
                                    uint32* rtp_timestamp) {
  if (!have_received_packets_) return false;

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
  DCHECK_LE(payload_size, kIpPacketSize);
  audio_decoder_->IncomingPacket(payload_data, static_cast<int32>(payload_size),
                                 rtp_header.webrtc);
  have_received_packets_ = true;
}

}  // namespace cast
}  // namespace media
