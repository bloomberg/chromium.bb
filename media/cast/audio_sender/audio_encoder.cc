// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/audio_sender/audio_encoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "third_party/webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "third_party/webrtc/modules/interface/module_common_types.h"

namespace media {
namespace cast {

// 48KHz, 2 channels and 100 ms.
static const int kMaxNumberOfSamples = 48 * 2 * 100;

// This class is only called from the cast audio encoder thread.
class WebrtEncodedDataCallback : public webrtc::AudioPacketizationCallback {
 public:
  WebrtEncodedDataCallback(scoped_refptr<CastEnvironment> cast_environment,
                           AudioCodec codec,
                           int frequency)
      : codec_(codec),
        frequency_(frequency),
        cast_environment_(cast_environment),
        last_timestamp_(0) {}

  virtual int32 SendData(
      webrtc::FrameType /*frame_type*/,
      uint8 /*payload_type*/,
      uint32 timestamp,
      const uint8* payload_data,
      uint16 payload_size,
      const webrtc::RTPFragmentationHeader* /*fragmentation*/) OVERRIDE {
    scoped_ptr<EncodedAudioFrame> audio_frame(new EncodedAudioFrame());
    audio_frame->codec = codec_;
    audio_frame->samples = timestamp - last_timestamp_;
    DCHECK(audio_frame->samples <= kMaxNumberOfSamples);
    last_timestamp_ = timestamp;
    audio_frame->data.insert(audio_frame->data.begin(),
                             payload_data,
                             payload_data + payload_size);

    cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
        base::Bind(*frame_encoded_callback_, base::Passed(&audio_frame),
                   recorded_time_));
    return 0;
  }

  void SetEncodedCallbackInfo(
      const base::TimeTicks& recorded_time,
      const AudioEncoder::FrameEncodedCallback* frame_encoded_callback) {
    recorded_time_ = recorded_time;
    frame_encoded_callback_ = frame_encoded_callback;
  }

 private:
  const AudioCodec codec_;
  const int frequency_;
  scoped_refptr<CastEnvironment> cast_environment_;
  uint32 last_timestamp_;
  base::TimeTicks recorded_time_;
  const AudioEncoder::FrameEncodedCallback* frame_encoded_callback_;
};

AudioEncoder::AudioEncoder(scoped_refptr<CastEnvironment> cast_environment,
                           const AudioSenderConfig& audio_config)
    : cast_environment_(cast_environment),
      audio_encoder_(webrtc::AudioCodingModule::Create(0)),
      webrtc_encoder_callback_(
          new WebrtEncodedDataCallback(cast_environment, audio_config.codec,
                                       audio_config.frequency)),
      timestamp_(0) {  // Must start at 0; used above.
  if (audio_encoder_->InitializeSender() != 0) {
    DCHECK(false) << "Invalid webrtc return value";
  }
  if (audio_encoder_->RegisterTransportCallback(
      webrtc_encoder_callback_.get()) != 0) {
    DCHECK(false) << "Invalid webrtc return value";
  }
  webrtc::CodecInst send_codec;
  send_codec.pltype = audio_config.rtp_payload_type;
  send_codec.plfreq = audio_config.frequency;
  send_codec.channels = audio_config.channels;

  switch (audio_config.codec) {
    case kOpus:
      strncpy(send_codec.plname, "opus", sizeof(send_codec.plname));
      send_codec.pacsize = audio_config.frequency / 50;  // 20 ms
      send_codec.rate = audio_config.bitrate;  // 64000
      break;
    case kPcm16:
      strncpy(send_codec.plname, "L16", sizeof(send_codec.plname));
      send_codec.pacsize = audio_config.frequency / 100;  // 10 ms
      // TODO(pwestin) bug in webrtc; it should take audio_config.channels into
      // account.
      send_codec.rate = 8 * 2 * audio_config.frequency;
      break;
    default:
      DCHECK(false) << "Codec must be specified for audio encoder";
      return;
  }
  if (audio_encoder_->RegisterSendCodec(send_codec) != 0) {
    DCHECK(false) << "Invalid webrtc return value; failed to register codec";
  }
}

AudioEncoder::~AudioEncoder() {}

// Called from main cast thread.
void AudioEncoder::InsertRawAudioFrame(
    const PcmAudioFrame* audio_frame,
    const base::TimeTicks& recorded_time,
    const FrameEncodedCallback& frame_encoded_callback,
    const base::Closure release_callback) {
  cast_environment_->PostTask(CastEnvironment::AUDIO_ENCODER, FROM_HERE,
      base::Bind(&AudioEncoder::EncodeAudioFrameThread, this, audio_frame,
          recorded_time, frame_encoded_callback, release_callback));
}

// Called from cast audio encoder thread.
void AudioEncoder::EncodeAudioFrameThread(
    const PcmAudioFrame* audio_frame,
    const base::TimeTicks& recorded_time,
    const FrameEncodedCallback& frame_encoded_callback,
    const base::Closure release_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::AUDIO_ENCODER));
  size_t samples_per_10ms = audio_frame->frequency / 100;
  size_t number_of_10ms_blocks = audio_frame->samples.size() /
      (samples_per_10ms * audio_frame->channels);
  DCHECK(webrtc::AudioFrame::kMaxDataSizeSamples > samples_per_10ms)
      << "webrtc sanity check failed";

  for (size_t i = 0; i < number_of_10ms_blocks; ++i) {
    webrtc::AudioFrame webrtc_audio_frame;
    webrtc_audio_frame.timestamp_ = timestamp_;

    // Due to the webrtc::AudioFrame declaration we need to copy our data into
    // the webrtc structure.
    memcpy(&webrtc_audio_frame.data_[0],
           &audio_frame->samples[i * samples_per_10ms * audio_frame->channels],
           samples_per_10ms * audio_frame->channels * sizeof(int16));

    // The webrtc API is int and we have a size_t; the cast should never be an
    // issue since the normal values are in the 480 range.
    DCHECK_GE(1000u, samples_per_10ms);
    webrtc_audio_frame.samples_per_channel_ =
        static_cast<int>(samples_per_10ms);
    webrtc_audio_frame.sample_rate_hz_ = audio_frame->frequency;
    webrtc_audio_frame.num_channels_ = audio_frame->channels;

    // webrtc::AudioCodingModule is thread safe.
    if (audio_encoder_->Add10MsData(webrtc_audio_frame) != 0) {
      DCHECK(false) << "Invalid webrtc return value";
    }
    timestamp_ += static_cast<uint32>(samples_per_10ms);
  }
  // We are done with the audio frame release it.
  cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
                              release_callback);

  // Note: Not all insert of 10 ms will generate a callback with encoded data.
  webrtc_encoder_callback_->SetEncodedCallbackInfo(recorded_time,
                                                   &frame_encoded_callback);
  for (size_t i = 0; i < number_of_10ms_blocks; ++i) {
    audio_encoder_->Process();
  }
}

}  // namespace cast
}  // namespace media
