// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_decoder_config.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "media/audio/sample_rates.h"
#include "media/base/limits.h"

namespace media {

static int SampleFormatToBitsPerChannel(SampleFormat sample_format) {
  switch (sample_format) {
    case kUnknownSampleFormat:
      return 0;
    case kSampleFormatU8:
      return 8;
    case kSampleFormatS16:
    case kSampleFormatPlanarS16:
      return 16;
    case kSampleFormatS32:
    case kSampleFormatF32:
    case kSampleFormatPlanarF32:
      return 32;
    case kSampleFormatMax:
      break;
  }

  NOTREACHED() << "Invalid sample format provided: " << sample_format;
  return 0;
}

AudioDecoderConfig::AudioDecoderConfig()
    : codec_(kUnknownAudioCodec),
      sample_format_(kUnknownSampleFormat),
      bits_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_UNSUPPORTED),
      samples_per_second_(0),
      bytes_per_frame_(0),
      extra_data_size_(0),
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
  extra_data_size_ = extra_data_size;
  sample_format_ = sample_format;
  bits_per_channel_ = SampleFormatToBitsPerChannel(sample_format);

  if (extra_data_size_ > 0) {
    extra_data_.reset(new uint8[extra_data_size_]);
    memcpy(extra_data_.get(), extra_data, extra_data_size_);
  } else {
    extra_data_.reset();
  }

  is_encrypted_ = is_encrypted;

  int channels = ChannelLayoutToChannelCount(channel_layout_);
  bytes_per_frame_ = channels * bits_per_channel_ / 8;
}

AudioDecoderConfig::~AudioDecoderConfig() {}

bool AudioDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownAudioCodec &&
         channel_layout_ != CHANNEL_LAYOUT_UNSUPPORTED &&
         bits_per_channel_ > 0 &&
         bits_per_channel_ <= limits::kMaxBitsPerSample &&
         samples_per_second_ > 0 &&
         samples_per_second_ <= limits::kMaxSampleRate &&
         sample_format_ != kUnknownSampleFormat;
}

bool AudioDecoderConfig::Matches(const AudioDecoderConfig& config) const {
  return ((codec() == config.codec()) &&
          (bits_per_channel() == config.bits_per_channel()) &&
          (channel_layout() == config.channel_layout()) &&
          (samples_per_second() == config.samples_per_second()) &&
          (extra_data_size() == config.extra_data_size()) &&
          (!extra_data() || !memcmp(extra_data(), config.extra_data(),
                                    extra_data_size())) &&
          (is_encrypted() == config.is_encrypted()) &&
          (sample_format() == config.sample_format()));
}

void AudioDecoderConfig::CopyFrom(const AudioDecoderConfig& audio_config) {
  Initialize(audio_config.codec(),
             audio_config.sample_format(),
             audio_config.channel_layout(),
             audio_config.samples_per_second(),
             audio_config.extra_data(),
             audio_config.extra_data_size(),
             audio_config.is_encrypted(),
             false);
}

}  // namespace media
