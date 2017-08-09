// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fuchsia/audio_manager_fuchsia.h"

#include "base/memory/ptr_util.h"

namespace media {

// TODO(fuchsia): Implement this class.

AudioManagerFuchsia::AudioManagerFuchsia(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {}
AudioManagerFuchsia::~AudioManagerFuchsia() {}

bool AudioManagerFuchsia::HasAudioOutputDevices() {
  NOTIMPLEMENTED();
  return false;
}

bool AudioManagerFuchsia::HasAudioInputDevices() {
  NOTIMPLEMENTED();
  return false;
}

void AudioManagerFuchsia::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  device_names->clear();
  NOTIMPLEMENTED();
}

void AudioManagerFuchsia::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  device_names->clear();
  NOTIMPLEMENTED();
}

AudioParameters AudioManagerFuchsia::GetInputStreamParameters(
    const std::string& device_id) {
  NOTREACHED();
  return AudioParameters();
}

AudioParameters AudioManagerFuchsia::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  NOTREACHED();
  return AudioParameters();
}

const char* AudioManagerFuchsia::GetName() {
  return "Fuchsia";
}

AudioOutputStream* AudioManagerFuchsia::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioOutputStream* AudioManagerFuchsia::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioInputStream* AudioManagerFuchsia::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioInputStream* AudioManagerFuchsia::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return base::MakeUnique<AudioManagerFuchsia>(std::move(audio_thread),
                                               audio_log_factory);
}

}  // namespace media
