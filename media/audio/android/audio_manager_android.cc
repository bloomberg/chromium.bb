// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_manager_android.h"

#include "base/logging.h"
#include "media/audio/android/opensles_input.h"
#include "media/audio/android/opensles_output.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/channel_layout.h"

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 10;

AudioManager* CreateAudioManager() {
  return new AudioManagerAndroid();
}

AudioManagerAndroid::AudioManagerAndroid() {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerAndroid::~AudioManagerAndroid() {
  Shutdown();
}

bool AudioManagerAndroid::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerAndroid::HasAudioInputDevices() {
  return true;
}

void AudioManagerAndroid::GetAudioInputDeviceNames(
    media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  device_names->push_front(
      media::AudioDeviceName(kDefaultDeviceName, kDefaultDeviceId));
}

AudioParameters AudioManagerAndroid::GetInputStreamParameters(
    const std::string& device_id) {
  // TODO(xians): figure out the right input sample rate and buffer size to
  // achieve the best audio performance for Android devices.
  // TODO(xians): query the native channel layout for the specific device.
  static const int kDefaultSampleRate = 16000;
  static const int kDefaultBufferSize = 1024;
  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
      kDefaultSampleRate, 16, kDefaultBufferSize);
}

AudioOutputStream* AudioManagerAndroid::MakeLinearOutputStream(
      const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return new OpenSLESOutputStream(this, params);
}

AudioOutputStream* AudioManagerAndroid::MakeLowLatencyOutputStream(
      const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return new OpenSLESOutputStream(this, params);
}

AudioInputStream* AudioManagerAndroid::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return new OpenSLESInputStream(this, params);
}

AudioInputStream* AudioManagerAndroid::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return new OpenSLESInputStream(this, params);
}

AudioParameters AudioManagerAndroid::GetPreferredOutputStreamParameters(
    const AudioParameters& input_params) {
  // TODO(xians): figure out the right output sample rate and sample rate to
  // achieve the best audio performance for Android devices.
  static const int kDefaultSampleRate = 16000;
  static const int kDefaultBufferSize = 1024;

  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int sample_rate = kDefaultSampleRate;
  int buffer_size = kDefaultBufferSize;
  int bits_per_sample = 16;
  int input_channels = 0;
  if (input_params.IsValid()) {
    // Use the client's input parameters if they are valid.
    sample_rate = input_params.sample_rate();
    bits_per_sample = input_params.bits_per_sample();
    channel_layout = input_params.channel_layout();
    input_channels = input_params.input_channels();

    // TODO(leozwang): Android defines the minimal buffer size requirment
    // we should follow it. From Android 4.1, a new audio low latency api
    // set was introduced and is under development, we want to take advantage
    // of it.
    buffer_size = std::min(buffer_size, input_params.frames_per_buffer());
  }

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, input_channels,
      sample_rate, bits_per_sample, buffer_size);
}

}  // namespace media
