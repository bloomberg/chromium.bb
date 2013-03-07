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

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

static const char kCrasAutomaticDeviceName[] = "Automatic";
static const char kCrasAutomaticDeviceId[] = "automatic";

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

void AudioManagerCras::GetCrasAudioInputDevices(
    media::AudioDeviceNames* device_names) {
  // Cras will route audio from a proper physical device automatically.
  device_names->push_back(media::AudioDeviceName(
      kCrasAutomaticDeviceName, kCrasAutomaticDeviceId));
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

AudioOutputStream* AudioManagerCras::MakeOutputStream(
    const AudioParameters& params) {
  return new CrasOutputStream(params, this);
}

AudioInputStream* AudioManagerCras::MakeInputStream(
    const AudioParameters& params, const std::string& device_id) {
  return new CrasInputStream(params, this);
}

AudioParameters AudioManagerCras::GetPreferredLowLatencyOutputStreamParameters(
    const AudioParameters& input_params) {
  // TODO(dalecurtis): This should include bits per channel and channel layout
  // eventually.
  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, input_params.channel_layout(),
      input_params.sample_rate(), 16, input_params.frames_per_buffer());
}

}  // namespace media
