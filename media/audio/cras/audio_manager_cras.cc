// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/audio_manager_cras.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "media/audio/audio_util.h"
#include "media/audio/cras/cras_input.h"
#include "media/audio/cras/cras_output.h"
#include "media/base/channel_layout.h"

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

// Default sample rate for input and output streams.
static const int kDefaultSampleRate = 48000;

bool AudioManagerCras::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerCras::HasAudioInputDevices() {
  return true;
}

AudioManagerCras::AudioManagerCras() {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerCras::~AudioManagerCras() {
  Shutdown();
}

void AudioManagerCras::ShowAudioInputSettings() {
  NOTIMPLEMENTED();
}

void AudioManagerCras::GetAudioInputDeviceNames(
    media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  GetCrasAudioInputDevices(device_names);
  return;
}

AudioParameters AudioManagerCras::GetInputStreamParameters(
    const std::string& device_id) {
  static const int kDefaultInputBufferSize = 1024;

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
      kDefaultSampleRate, 16, kDefaultInputBufferSize);
}

void AudioManagerCras::GetCrasAudioInputDevices(
    media::AudioDeviceNames* device_names) {
  // Cras will route audio from a proper physical device automatically.
  device_names->push_back(
      AudioDeviceName(AudioManagerBase::kDefaultDeviceName,
                      AudioManagerBase::kDefaultDeviceId));
}

AudioOutputStream* AudioManagerCras::MakeLinearOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeOutputStream(params);
}

AudioOutputStream* AudioManagerCras::MakeLowLatencyOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeOutputStream(params);
}

AudioInputStream* AudioManagerCras::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerCras::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

AudioParameters AudioManagerCras::GetPreferredOutputStreamParameters(
    const AudioParameters& input_params) {
  static const int kDefaultOutputBufferSize = 512;

  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int sample_rate = kDefaultSampleRate;
  int buffer_size = kDefaultOutputBufferSize;
  int bits_per_sample = 16;
  int input_channels = 0;
  if (input_params.IsValid()) {
    sample_rate = input_params.sample_rate();
    bits_per_sample = input_params.bits_per_sample();
    channel_layout = input_params.channel_layout();
    input_channels = input_params.input_channels();
    buffer_size = input_params.frames_per_buffer();
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, input_channels,
      sample_rate, bits_per_sample, buffer_size);
}

AudioOutputStream* AudioManagerCras::MakeOutputStream(
    const AudioParameters& params) {
  return new CrasOutputStream(params, this);
}

AudioInputStream* AudioManagerCras::MakeInputStream(
    const AudioParameters& params, const std::string& device_id) {
  return new CrasInputStream(params, this);
}

}  // namespace media
