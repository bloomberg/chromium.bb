// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/decoder_config_adapter.h"

#include "base/logging.h"
#include "media/base/channel_layout.h"

namespace chromecast {
namespace media {

namespace {

// Converts ::media::AudioCodec to chromecast::media::AudioCodec. Any unknown or
// unsupported codec will be converted to chromecast::media::kCodecUnknown.
AudioCodec ToAudioCodec(const ::media::AudioCodec audio_codec) {
  switch (audio_codec) {
    case ::media::kCodecAAC:
      return kCodecAAC;
    case ::media::kCodecMP3:
      return kCodecMP3;
    case ::media::kCodecPCM:
      return kCodecPCM;
    case ::media::kCodecPCM_S16BE:
      return kCodecPCM_S16BE;
    case ::media::kCodecVorbis:
      return kCodecVorbis;
    case ::media::kCodecOpus:
      return kCodecOpus;
    case ::media::kCodecFLAC:
      return kCodecFLAC;
    default:
      LOG(ERROR) << "Unsupported audio codec " << audio_codec;
  }
  return kAudioCodecUnknown;
}

SampleFormat ToSampleFormat(const ::media::SampleFormat sample_format) {
  switch (sample_format) {
    case ::media::kUnknownSampleFormat:
      return kUnknownSampleFormat;
    case ::media::kSampleFormatU8:
      return kSampleFormatU8;
    case ::media::kSampleFormatS16:
      return kSampleFormatS16;
    case ::media::kSampleFormatS24:
      return kSampleFormatS24;
    case ::media::kSampleFormatS32:
      return kSampleFormatS32;
    case ::media::kSampleFormatF32:
      return kSampleFormatF32;
    case ::media::kSampleFormatPlanarS16:
      return kSampleFormatPlanarS16;
    case ::media::kSampleFormatPlanarF32:
      return kSampleFormatPlanarF32;
    case ::media::kSampleFormatPlanarS32:
      return kSampleFormatPlanarS32;
  }
  NOTREACHED();
  return kUnknownSampleFormat;
}

// Converts ::media::VideoCodec to chromecast::media::VideoCodec. Any unknown or
// unsupported codec will be converted to chromecast::media::kCodecUnknown.
VideoCodec ToVideoCodec(const ::media::VideoCodec video_codec) {
  switch (video_codec) {
    case ::media::kCodecH264:
      return kCodecH264;
    case ::media::kCodecVP8:
      return kCodecVP8;
    case ::media::kCodecVP9:
      return kCodecVP9;
    case ::media::kCodecHEVC:
      return kCodecHEVC;
    default:
      LOG(ERROR) << "Unsupported video codec " << video_codec;
  }
  return kVideoCodecUnknown;
}

// Converts ::media::VideoCodecProfile to chromecast::media::VideoProfile.
VideoProfile ToVideoProfile(const ::media::VideoCodecProfile codec_profile) {
  switch(codec_profile) {
    case ::media::H264PROFILE_BASELINE:
      return kH264Baseline;
    case ::media::H264PROFILE_MAIN:
      return kH264Main;
    case ::media::H264PROFILE_EXTENDED:
      return kH264Extended;
    case ::media::H264PROFILE_HIGH:
      return kH264High;
    case ::media::H264PROFILE_HIGH10PROFILE:
      return kH264High10;
    case ::media::H264PROFILE_HIGH422PROFILE:
      return kH264High422;
    case ::media::H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return kH264High444Predictive;
    case ::media::H264PROFILE_SCALABLEBASELINE:
      return kH264ScalableBaseline;
    case ::media::H264PROFILE_SCALABLEHIGH:
      return kH264ScalableHigh;
    case ::media::H264PROFILE_STEREOHIGH:
      return kH264Stereohigh;
    case ::media::H264PROFILE_MULTIVIEWHIGH:
      return kH264MultiviewHigh;
    case ::media::VP8PROFILE_ANY:
      return kVP8ProfileAny;
    case ::media::VP9PROFILE_ANY:
      return kVP9ProfileAny;
    default:
      LOG(INFO) << "Unsupported video codec profile " << codec_profile;
  }
  return kVideoProfileUnknown;
}

::media::ChannelLayout ToMediaChannelLayout(int channel_number) {
  switch (channel_number) {
    case 1:
      return ::media::ChannelLayout::CHANNEL_LAYOUT_MONO;
    case 2:
      return ::media::ChannelLayout::CHANNEL_LAYOUT_STEREO;
    default:
      return ::media::ChannelLayout::CHANNEL_LAYOUT_UNSUPPORTED;
  }
}

::media::SampleFormat ToMediaSampleFormat(const SampleFormat sample_format) {
  switch (sample_format) {
    case kUnknownSampleFormat:
      return ::media::kUnknownSampleFormat;
    case kSampleFormatU8:
      return ::media::kSampleFormatU8;
    case kSampleFormatS16:
      return ::media::kSampleFormatS16;
    case kSampleFormatS24:
      return ::media::kSampleFormatS24;
    case kSampleFormatS32:
      return ::media::kSampleFormatS32;
    case kSampleFormatF32:
      return ::media::kSampleFormatF32;
    case kSampleFormatPlanarS16:
      return ::media::kSampleFormatPlanarS16;
    case kSampleFormatPlanarF32:
      return ::media::kSampleFormatPlanarF32;
    case kSampleFormatPlanarS32:
      return ::media::kSampleFormatPlanarS32;
    default:
      NOTREACHED();
      return ::media::kUnknownSampleFormat;
  }
}

::media::AudioCodec ToMediaAudioCodec(
    const chromecast::media::AudioCodec codec) {
  switch (codec) {
    case kAudioCodecUnknown:
      return ::media::kUnknownAudioCodec;
    case kCodecAAC:
      return ::media::kCodecAAC;
    case kCodecMP3:
      return ::media::kCodecMP3;
    case kCodecPCM:
      return ::media::kCodecPCM;
    case kCodecPCM_S16BE:
      return ::media::kCodecPCM_S16BE;
    case kCodecVorbis:
      return ::media::kCodecVorbis;
    case kCodecOpus:
      return ::media::kCodecOpus;
    case kCodecFLAC:
      return ::media::kCodecFLAC;
    default:
      return ::media::kUnknownAudioCodec;
  }
}

}  // namespace

// static
AudioConfig DecoderConfigAdapter::ToCastAudioConfig(
    StreamId id,
    const ::media::AudioDecoderConfig& config) {
  AudioConfig audio_config;
  if (!config.IsValidConfig())
    return audio_config;

  audio_config.id = id;
  audio_config.codec = ToAudioCodec(config.codec());
  audio_config.sample_format = ToSampleFormat(config.sample_format());
  audio_config.bytes_per_channel = config.bytes_per_channel();
  audio_config.channel_number =
      ::media::ChannelLayoutToChannelCount(config.channel_layout()),
  audio_config.samples_per_second = config.samples_per_second();
  audio_config.extra_data = config.extra_data();
  audio_config.is_encrypted = config.is_encrypted();
  return audio_config;
}

// static
::media::AudioDecoderConfig DecoderConfigAdapter::ToMediaAudioDecoderConfig(
    const AudioConfig& config) {
  return ::media::AudioDecoderConfig(
      ToMediaAudioCodec(config.codec),
      ToMediaSampleFormat(config.sample_format),
      ToMediaChannelLayout(config.channel_number), config.samples_per_second,
      config.extra_data, config.is_encrypted);
}

// static
VideoConfig DecoderConfigAdapter::ToCastVideoConfig(
    StreamId id,
    const ::media::VideoDecoderConfig& config) {
  VideoConfig video_config;
  if (!config.IsValidConfig()) {
    return video_config;
  }

  video_config.id = id;
  video_config.codec = ToVideoCodec(config.codec());
  video_config.profile = ToVideoProfile(config.profile());
  video_config.extra_data = config.extra_data();
  video_config.is_encrypted = config.is_encrypted();
  return video_config;
}

}  // namespace media
}  // namespace chromecast
