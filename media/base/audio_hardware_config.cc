// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_hardware_config.h"

#include <algorithm>

#include "base/logging.h"
#include "build/build_config.h"

using base::AutoLock;
using media::AudioParameters;

namespace media {

#if defined(OS_LINUX)
#define HIGH_LATENCY_AUDIO_SUPPORT 1
#endif

#if defined(HIGH_LATENCY_AUDIO_SUPPORT)
// Taken from "Bit Twiddling Hacks"
// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static uint32_t RoundUpToPowerOfTwo(uint32_t v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}
#endif

AudioHardwareConfig::AudioHardwareConfig(
    const AudioParameters& input_params,
    const AudioParameters& output_params)
    : input_params_(input_params),
      output_params_(output_params) {}

AudioHardwareConfig::~AudioHardwareConfig() {}

int AudioHardwareConfig::GetOutputBufferSize() const {
  AutoLock auto_lock(config_lock_);
  return output_params_.frames_per_buffer();
}

int AudioHardwareConfig::GetOutputSampleRate() const {
  AutoLock auto_lock(config_lock_);
  return output_params_.sample_rate();
}

ChannelLayout AudioHardwareConfig::GetOutputChannelLayout() const {
  AutoLock auto_lock(config_lock_);
  return output_params_.channel_layout();
}

int AudioHardwareConfig::GetOutputChannels() const {
  AutoLock auto_lock(config_lock_);
  return output_params_.channels();
}

int AudioHardwareConfig::GetInputSampleRate() const {
  AutoLock auto_lock(config_lock_);
  return input_params_.sample_rate();
}

ChannelLayout AudioHardwareConfig::GetInputChannelLayout() const {
  AutoLock auto_lock(config_lock_);
  return input_params_.channel_layout();
}

int AudioHardwareConfig::GetInputChannels() const {
  AutoLock auto_lock(config_lock_);
  return input_params_.channels();
}

media::AudioParameters
AudioHardwareConfig::GetInputConfig() const {
  AutoLock auto_lock(config_lock_);
  return input_params_;
}

media::AudioParameters
AudioHardwareConfig::GetOutputConfig() const {
  AutoLock auto_lock(config_lock_);
  return output_params_;
}

void AudioHardwareConfig::UpdateInputConfig(
    const AudioParameters& input_params) {
  AutoLock auto_lock(config_lock_);
  input_params_ = input_params;
}

void AudioHardwareConfig::UpdateOutputConfig(
    const AudioParameters& output_params) {
  AutoLock auto_lock(config_lock_);
  output_params_ = output_params;
}

int AudioHardwareConfig::GetHighLatencyBufferSize() const {
  AutoLock auto_lock(config_lock_);
#if defined(HIGH_LATENCY_AUDIO_SUPPORT)
  // Empirically, use the nearest higher power of two buffer size corresponding
  // to 20 ms worth of samples.  For a given sample rate, this works out to:
  //
  //     <= 3200   : 64
  //     <= 6400   : 128
  //     <= 12800  : 256
  //     <= 25600  : 512
  //     <= 51200  : 1024
  //     <= 102400 : 2048
  //     <= 204800 : 4096
  //
  // On Linux, the minimum hardware buffer size is 512, so the lower calculated
  // values are unused.  OSX may have a value as low as 128.  Windows is device
  // dependent but will generally be sample_rate() / 100.
  const int high_latency_buffer_size =
      RoundUpToPowerOfTwo(2 * output_params_.sample_rate() / 100);
  return std::max(output_params_.frames_per_buffer(), high_latency_buffer_size);
#else
  return output_params_.frames_per_buffer();
#endif
}

}  // namespace media
