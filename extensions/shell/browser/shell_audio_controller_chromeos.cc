// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_audio_controller_chromeos.h"

#include <algorithm>

#include "chromeos/audio/audio_device.h"

namespace extensions {

namespace {

// Default output and input volume.
const double kOutputVolumePercent = 100.0;
const double kInputGainPercent = 100.0;

// Returns a pointer to the device in |devices| with ID |node_id|, or NULL if it
// isn't present.
const chromeos::AudioDevice* GetDevice(const chromeos::AudioDeviceList& devices,
                                       uint64 node_id) {
  for (chromeos::AudioDeviceList::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    if (it->id == node_id)
      return &(*it);
  }
  return NULL;
}

}  // namespace

ShellAudioController::PrefHandler::PrefHandler() {}

double ShellAudioController::PrefHandler::GetOutputVolumeValue(
    const chromeos::AudioDevice* device) {
  return kOutputVolumePercent;
}

double ShellAudioController::PrefHandler::GetInputGainValue(
    const chromeos::AudioDevice* device) {
  return kInputGainPercent;
}

void ShellAudioController::PrefHandler::SetVolumeGainValue(
    const chromeos::AudioDevice& device,
    double value) {
  // TODO(derat): Shove volume and mute prefs into a map so we can at least
  // honor changes that are made at runtime.
}

bool ShellAudioController::PrefHandler::GetMuteValue(
    const chromeos::AudioDevice& device) {
  return false;
}

void ShellAudioController::PrefHandler::SetMuteValue(
    const chromeos::AudioDevice& device,
    bool mute_on) {}

bool ShellAudioController::PrefHandler::GetAudioCaptureAllowedValue() {
  return true;
}

bool ShellAudioController::PrefHandler::GetAudioOutputAllowedValue() {
  return true;
}

void ShellAudioController::PrefHandler::AddAudioPrefObserver(
    chromeos::AudioPrefObserver* observer) {}

void ShellAudioController::PrefHandler::RemoveAudioPrefObserver(
    chromeos::AudioPrefObserver* observer) {}

ShellAudioController::PrefHandler::~PrefHandler() {}

ShellAudioController::ShellAudioController() {
  chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
  ActivateDevices();
}

ShellAudioController::~ShellAudioController() {
  chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void ShellAudioController::OnOutputVolumeChanged() {}

void ShellAudioController::OnOutputMuteChanged() {}

void ShellAudioController::OnInputGainChanged() {}

void ShellAudioController::OnInputMuteChanged() {}

void ShellAudioController::OnAudioNodesChanged() {
  VLOG(1) << "Audio nodes changed";
  ActivateDevices();
}

void ShellAudioController::OnActiveOutputNodeChanged() {}

void ShellAudioController::OnActiveInputNodeChanged() {}

void ShellAudioController::ActivateDevices() {
  chromeos::CrasAudioHandler* handler = chromeos::CrasAudioHandler::Get();
  chromeos::AudioDeviceList devices;
  handler->GetAudioDevices(&devices);
  sort(devices.begin(), devices.end(), chromeos::AudioDeviceCompare());

  uint64 best_input = 0, best_output = 0;
  for (chromeos::AudioDeviceList::const_reverse_iterator it = devices.rbegin();
       it != devices.rend() && (!best_input || !best_output); ++it) {
    // TODO(derat): Need to check |plugged_time|?
    if (it->is_input && !best_input)
      best_input = it->id;
    else if (!it->is_input && !best_output)
      best_output = it->id;
  }

  if (best_input && best_input != handler->GetPrimaryActiveInputNode()) {
    const chromeos::AudioDevice* device = GetDevice(devices, best_input);
    DCHECK(device);
    VLOG(1) << "Activating input device: " << device->ToString();
    handler->SwitchToDevice(*device);
  }
  if (best_output && best_output != handler->GetPrimaryActiveOutputNode()) {
    const chromeos::AudioDevice* device = GetDevice(devices, best_output);
    DCHECK(device);
    VLOG(1) << "Activating output device: " << device->ToString();
    handler->SwitchToDevice(*device);
  }
}

}  // namespace extensions
