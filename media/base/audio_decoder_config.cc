// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_decoder_config.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "media/audio/sample_rates.h"
#include "media/base/limits.h"
#include "media/base/sample_format.h"

namespace media {

AudioDecoderConfig::AudioDecoderConfig()
    : codec_(kUnknownAudioCodec),
      sample_format_(kUnknownSampleFormat),
      bytes_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_UNSUPPORTED),
      samples_per_second_(0),
      bytes_per_frame_(0),
      is_encrypted_(false) {
}

AudioDecoderConfig::AudioDecoderConfig(AudioCodec codec,
                                       SampleFormat sample_format,
                                       ChannelLayout channel_layout,
                                       int samples_per_second,
                                       const uint8* extra_data,
                                       size_t extra_data_size,
                                       bool is_encrypted) {
  Initialize(codec, sample_format, channel_layout, samples_per_second,
             extra_data, extra_data_size, is_encrypted, true);
}

void AudioDecoderConfig::Initialize(AudioCodec codec,
                                    SampleFormat sample_format,
                                    ChannelLayout channel_layout,
                                    int samples_per_second,
                                    const uint8* extra_data,
                                    size_t extra_data_size,
                                    bool is_encrypted,
                                    bool record_stats) {
  CHECK((extra_data_size != 0) == (extra_data != NULL));

  if (record_stats) {
    UMA_HISTOGRAM_ENUMERATION("Media.AudioCodec", codec, kAudioCodecMax);
    UMA_HISTOGRAM_ENUMERATION("Media.AudioSampleFormat", sample_format,
                              kSampleFormatMax);
    UMA_HISTOGRAM_ENUMERATION("Media.AudioChannelLayout", channel_layout,
                              CHANNEL_LAYOUT_MAX);
    AudioSampleRate asr = media::AsAudioSampleRate(samples_per_second);
    if (asr != kUnexpectedAudioSampleRate) {
      UMA_HISTOGRAM_ENUMERATION("Media.AudioSamplesPerSecond", asr,
                                kUnexpectedAudioSampleRate);
    } else {
      UMA_HISTOGRAM_COUNTS(
          "Media.AudioSamplesPerSecondUnexpected", samples_per_second);
    }
  }

  codec_ = codec;
  channel_layout_ = channel_layout;
  samples_per_second_ = samples_per_second;
  sample_format_ = sample_format;
  bytes_per_channel_ = SampleFormatToBytesPerChannel(sample_format);
  extra_data_.assign(extra_data, extra_data + extra_data_size);
  is_encrypted_ = is_encrypted;

  int channels = ChannelLayoutToChannelCount(channel_layout_);
  bytes_per_frame_ = channels * bytes_per_channel_;
}

AudioDecoderConfig::~AudioDecoderConfig() {}

bool AudioDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownAudioCodec &&
         channel_layout_ != CHANNEL_LAYOUT_UNSUPPORTED &&
         bytes_per_channel_ > 0 &&
         bytes_per_channel_ <= limits::kMaxBytesPerSample &&
         samples_per_second_ > 0 &&
         samples_per_second_ <= limits::kMaxSampleRate &&
         sample_format_ != kUnknownSampleFormat;
}

bool AudioDecoderConfig::Matches(const AudioDecoderConfig& config) const {
  return ((codec() == config.codec()) &&
          (bytes_per_channel() == config.bytes_per_channel()) &&
          (channel_layout() == config.channel_layout()) &&
          (samples_per_second() == config.samples_per_second()) &&
          (extra_data_size() == config.extra_data_size()) &&
          (!extra_data() || !memcmp(extra_data(), config.extra_data(),
                                    extra_data_size())) &&
          (is_encrypted() == config.is_encrypted()) &&
          (sample_format() == config.sample_format()));
}

}  // namespace media
