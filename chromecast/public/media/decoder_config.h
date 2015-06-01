// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_DECODER_CONFIG_H_
#define CHROMECAST_PUBLIC_MEDIA_DECODER_CONFIG_H_

#include <stdint.h>
#include <vector>

#include "chromecast/public/media/stream_id.h"

namespace chromecast {
namespace media {

namespace {

// Maximum audio bytes per sample.
static const int kMaxBytesPerSample = 4;

// Maximum audio sampling rate.
static const int kMaxSampleRate = 192000;

}  // namespace

enum AudioCodec {
  kAudioCodecUnknown = -1,

  kAudioCodecMin = 0,
  kCodecAAC = kAudioCodecMin,
  kCodecMP3,
  kCodecPCM,
  kCodecPCM_S16BE,
  kCodecVorbis,
  kCodecOpus,
  kCodecEAC3,
  kCodecAC3,
  kCodecDTS,
  kAudioCodecMax = kCodecDTS,
};

enum VideoCodec {
  kVideoCodecUnknown = -1,

  kVideoCodecMin = 0,
  kCodecH264 = kVideoCodecMin,
  kCodecVC1,
  kCodecMPEG2,
  kCodecMPEG4,
  kCodecTheora,
  kCodecVP8,
  kCodecVP9,
  kCodecHEVC,
  kVideoCodecMax = kCodecHEVC,
};

// Profile for Video codec.
enum VideoProfile {
  kVideoProfileUnknown = -1,

  kVideoProfileMin = 0,
  kH264Baseline = kVideoProfileMin,
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
  kVideoProfileMax = kVP9ProfileAny,
};

// TODO(erickung): Remove constructor once CMA backend implementation does't
// create a new object to reset the configuration and use IsValidConfig() to
// determine if the configuration is still valid or not.
struct AudioConfig {
  AudioConfig()
    : id(kPrimary),
      codec(kAudioCodecUnknown),
      bytes_per_channel(0),
      channel_number(0),
      samples_per_second(0),
      extra_data(nullptr),
      extra_data_size(0),
      is_encrypted(false) {}

  // Stream id.
  StreamId id;
  // Audio codec.
  AudioCodec codec;
  // Number of bytes in each channel.
  int bytes_per_channel;
  // Number of channels in this audio stream.
  int channel_number;
  // Number of audio samples per second.
  int samples_per_second;
  // Pointer to extra data buffer for certain codec initialization. The memory
  // is allocated outside this structure. Consumers of the structure should make
  // a copy if it is expected to be used beyond the function ends.
  const uint8_t* extra_data;
  // Size of extra data in bytes.
  int extra_data_size;
  // content is encrypted or not.
  bool is_encrypted;
};

// TODO(erickung): Remove constructor once CMA backend implementation does't
// create a new object to reset the configuration and use IsValidConfig() to
// determine if the configuration is still valid or not.
struct VideoConfig {
  VideoConfig()
    : id(kPrimary),
      codec(kVideoCodecUnknown),
      profile(kVideoProfileUnknown),
      additional_config(nullptr),
      extra_data(nullptr),
      extra_data_size(0),
      is_encrypted(false) {}

  // Stream Id.
  StreamId id;
  // Video codec.
  VideoCodec codec;
  // Video codec profile.
  VideoProfile profile;
  // Both |additional_config| and |extra_data| are the pointers to the object
  // memory that are allocated outside this structure. Consumers of the
  // structure should make a copy if it is expected to be used beyond the
  // function ends.
  // Additional video config for the video stream if available.
  VideoConfig* additional_config;
  // Pointer to extra data buffer for certain codec initialization.
  const uint8_t* extra_data;
  // Size of extra data in bytes.
  int extra_data_size;
  // content is encrypted or not.
  bool is_encrypted;
};

// TODO(erickung): Remove following two inline IsValidConfig() functions. These
// are to keep existing CMA backend implementation consistent until the clean up
// is done. These SHOULD NOT be used in New CMA backend implementation.
inline bool IsValidConfig(const AudioConfig& config) {
  return config.codec >= kAudioCodecMin &&
      config.codec <= kAudioCodecMax &&
      config.channel_number > 0 &&
      config.bytes_per_channel > 0 &&
      config.bytes_per_channel <= kMaxBytesPerSample &&
      config.samples_per_second > 0 &&
      config.samples_per_second <= kMaxSampleRate;
}

inline bool IsValidConfig(const VideoConfig& config) {
  return config.codec >= kVideoCodecMin && config.codec <= kVideoCodecMax;
}

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_DECODER_CONFIG_H_
