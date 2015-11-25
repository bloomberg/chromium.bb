// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_DECODER_CONFIG_H_
#define CHROMECAST_PUBLIC_MEDIA_DECODER_CONFIG_H_

#include <stdint.h>
#include <vector>

#include "stream_id.h"

namespace chromecast {
namespace media {

// Maximum audio bytes per sample.
static const int kMaxBytesPerSample = 4;

// Maximum audio sampling rate.
static const int kMaxSampleRate = 192000;

enum AudioCodec {
  kAudioCodecUnknown = 0,
  kCodecAAC,
  kCodecMP3,
  kCodecPCM,
  kCodecPCM_S16BE,
  kCodecVorbis,
  kCodecOpus,
  kCodecEAC3,
  kCodecAC3,
  kCodecDTS,
  kCodecFLAC,

  kAudioCodecMin = kAudioCodecUnknown,
  kAudioCodecMax = kCodecFLAC,
};

enum SampleFormat {
  kUnknownSampleFormat = 0,
  kSampleFormatU8,         // Unsigned 8-bit w/ bias of 128.
  kSampleFormatS16,        // Signed 16-bit.
  kSampleFormatS32,        // Signed 32-bit.
  kSampleFormatF32,        // Float 32-bit.
  kSampleFormatPlanarS16,  // Signed 16-bit planar.
  kSampleFormatPlanarF32,  // Float 32-bit planar.
  kSampleFormatPlanarS32,  // Signed 32-bit planar.
  kSampleFormatS24,        // Signed 24-bit.

  kSampleFormatMin = kUnknownSampleFormat,
  kSampleFormatMax = kSampleFormatS24,
};

enum VideoCodec {
  kVideoCodecUnknown = 0,
  kCodecH264,
  kCodecVC1,
  kCodecMPEG2,
  kCodecMPEG4,
  kCodecTheora,
  kCodecVP8,
  kCodecVP9,
  kCodecHEVC,

  kVideoCodecMin = kVideoCodecUnknown,
  kVideoCodecMax = kCodecHEVC,
};

// Profile for Video codec.
enum VideoProfile {
  kVideoProfileUnknown = 0,
  kH264Baseline,
  kH264Main,
  kH264Extended,
  kH264High,
  kH264High10,
  kH264High422,
  kH264High444Predictive,
  kH264ScalableBaseline,
  kH264ScalableHigh,
  kH264Stereohigh,
  kH264MultiviewHigh,
  kVP8ProfileAny,
  kVP9ProfileAny,

  kVideoProfileMin = kVideoProfileUnknown,
  kVideoProfileMax = kVP9ProfileAny,
};

// TODO(erickung): Remove constructor once CMA backend implementation does't
// create a new object to reset the configuration and use IsValidConfig() to
// determine if the configuration is still valid or not.
struct AudioConfig {
  AudioConfig();
  ~AudioConfig();

  // Stream id.
  StreamId id;
  // Audio codec.
  AudioCodec codec;
  // The format of each audio sample.
  SampleFormat sample_format;
  // Number of bytes in each channel.
  int bytes_per_channel;
  // Number of channels in this audio stream.
  int channel_number;
  // Number of audio samples per second.
  int samples_per_second;
  // Extra data buffer for certain codec initialization.
  std::vector<uint8_t> extra_data;
  // content is encrypted or not.
  bool is_encrypted;
};

inline AudioConfig::AudioConfig()
    : id(kPrimary),
      codec(kAudioCodecUnknown),
      sample_format(kUnknownSampleFormat),
      bytes_per_channel(0),
      channel_number(0),
      samples_per_second(0),
      is_encrypted(false) {
}

inline AudioConfig::~AudioConfig() {
}

// TODO(erickung): Remove constructor once CMA backend implementation does't
// create a new object to reset the configuration and use IsValidConfig() to
// determine if the configuration is still valid or not.
struct VideoConfig {
  VideoConfig();
  ~VideoConfig();

  // Stream Id.
  StreamId id;
  // Video codec.
  VideoCodec codec;
  // Video codec profile.
  VideoProfile profile;
  // Additional video config for the video stream if available. Consumers of
  // this structure should make an explicit copy of |additional_config| if it
  // will be used after SetConfig() finishes.
  VideoConfig* additional_config;
  // Extra data buffer for certain codec initialization.
  std::vector<uint8_t> extra_data;
  // content is encrypted or not.
  bool is_encrypted;
};

inline VideoConfig::VideoConfig()
    : id(kPrimary),
      codec(kVideoCodecUnknown),
      profile(kVideoProfileUnknown),
      additional_config(nullptr),
      is_encrypted(false) {
}

inline VideoConfig::~VideoConfig() {
}

// TODO(erickung): Remove following two inline IsValidConfig() functions. These
// are to keep existing CMA backend implementation consistent until the clean up
// is done. These SHOULD NOT be used in New CMA backend implementation.
inline bool IsValidConfig(const AudioConfig& config) {
  return config.codec >= kAudioCodecMin &&
      config.codec <= kAudioCodecMax &&
      config.codec != kAudioCodecUnknown &&
      config.sample_format >= kSampleFormatMin &&
      config.sample_format <= kSampleFormatMax &&
      config.sample_format != kUnknownSampleFormat &&
      config.channel_number > 0 &&
      config.bytes_per_channel > 0 &&
      config.bytes_per_channel <= kMaxBytesPerSample &&
      config.samples_per_second > 0 &&
      config.samples_per_second <= kMaxSampleRate;
}

inline bool IsValidConfig(const VideoConfig& config) {
  return config.codec >= kVideoCodecMin &&
      config.codec <= kVideoCodecMax &&
      config.codec != kVideoCodecUnknown;
}

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_DECODER_CONFIG_H_
