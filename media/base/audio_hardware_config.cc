// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_hardware_config.h"

namespace media {

AudioHardwareConfig::AudioHardwareConfig(
    int output_buffer_size, int output_sample_rate,
    int input_sample_rate, ChannelLayout input_channel_layout)
    : output_buffer_size_(output_buffer_size),
      output_sample_rate_(output_sample_rate),
      input_sample_rate_(input_sample_rate),
      input_channel_layout_(input_channel_layout) {
}

AudioHardwareConfig::~AudioHardwareConfig() {}

int AudioHardwareConfig::GetOutputBufferSize() {
  base::AutoLock auto_lock(config_lock_);
  return output_buffer_size_;
}

int AudioHardwareConfig::GetOutputSampleRate() {
  base::AutoLock auto_lock(config_lock_);
  return output_sample_rate_;
}

int AudioHardwareConfig::GetInputSampleRate() {
  base::AutoLock auto_lock(config_lock_);
  return input_sample_rate_;
}

ChannelLayout AudioHardwareConfig::GetInputChannelLayout() {
  base::AutoLock auto_lock(config_lock_);
  return input_channel_layout_;
}

void AudioHardwareConfig::UpdateInputConfig(
    int sample_rate, media::ChannelLayout channel_layout) {
  base::AutoLock auto_lock(config_lock_);
  input_sample_rate_ = sample_rate;
  input_channel_layout_ = channel_layout;
}

void AudioHardwareConfig::UpdateOutputConfig(int buffer_size, int sample_rate) {
  base::AutoLock auto_lock(config_lock_);
  output_buffer_size_ = buffer_size;
  output_sample_rate_ = sample_rate;
}

}  // namespace media
