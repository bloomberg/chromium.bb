// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/audio_devices_pref_handler_stub.h"

#include "base/stl_util.h"
#include "chromeos/audio/audio_device.h"

namespace chromeos {

AudioDevicesPrefHandlerStub::AudioDevicesPrefHandlerStub() {
}

AudioDevicesPrefHandlerStub::~AudioDevicesPrefHandlerStub() {
}

double AudioDevicesPrefHandlerStub::GetOutputVolumeValue(
    const AudioDevice* device) {
  if (!device || !ContainsKey(audio_device_volume_gain_map_, device->id))
    return kDefaultOutputVolumePercent;
  return audio_device_volume_gain_map_[device->id];
}

double AudioDevicesPrefHandlerStub::GetInputGainValue(
    const AudioDevice* device) {
  // TODO(rkc): The default value for gain is wrong. http://crbug.com/442489
  if (!device || !ContainsKey(audio_device_volume_gain_map_, device->id))
    return 75.0;
  return audio_device_volume_gain_map_[device->id];
}

void AudioDevicesPrefHandlerStub::SetVolumeGainValue(const AudioDevice& device,
                                                     double value) {
  audio_device_volume_gain_map_[device.id] = value;
}

bool AudioDevicesPrefHandlerStub::GetMuteValue(
    const AudioDevice& device) {
  return audio_device_mute_map_[device.id];
}

void AudioDevicesPrefHandlerStub::SetMuteValue(const AudioDevice& device,
                                               bool mute_on) {
  audio_device_mute_map_[device.id] = mute_on;
}

bool AudioDevicesPrefHandlerStub::GetAudioOutputAllowedValue() {
  return true;
}

void AudioDevicesPrefHandlerStub::AddAudioPrefObserver(
    AudioPrefObserver* observer) {
}

void AudioDevicesPrefHandlerStub::RemoveAudioPrefObserver(
    AudioPrefObserver* observer) {
}

}  // namespace chromeos

