// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_manager_android.h"

#include "base/logging.h"
#include "media/audio/android/opensles_input.h"
#include "media/audio/android/opensles_output.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_util.h"
#include "media/audio/fake_audio_input_stream.h"

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

AudioParameters
AudioManagerAndroid::GetPreferredLowLatencyOutputStreamParameters(
    const AudioParameters& input_params) {
  // TODO(leozwang): Android defines the minimal buffer size requirment
  // we should follow it. From Android 4.1, a new audio low latency api
  // set was introduced and is under development, we want to take advantage
  // of it.
  int buffer_size = GetAudioHardwareBufferSize();
  if (input_params.frames_per_buffer() < buffer_size)
    buffer_size = input_params.frames_per_buffer();

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, input_params.channel_layout(),
      input_params.sample_rate(), 16, buffer_size);
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

}  // namespace media
