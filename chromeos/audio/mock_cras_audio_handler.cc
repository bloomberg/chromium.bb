// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/audio/mock_cras_audio_handler.h"

#include "chromeos/audio/audio_devices_pref_handler.h"

namespace chromeos {

void MockCrasAudioHandler::AddAudioObserver(AudioObserver* observer) {
}

void MockCrasAudioHandler::RemoveAudioObserver(AudioObserver* observer) {
}

bool MockCrasAudioHandler::IsOutputMuted() {
  return false;
}

bool MockCrasAudioHandler::IsInputMuted() {
  return false;
}

int MockCrasAudioHandler::GetOutputVolumePercent() {
  return 0;
}

uint64 MockCrasAudioHandler::GetActiveOutputNode() const {
  return 0;
}

uint64 MockCrasAudioHandler::GetActiveInputNode() const {
  return 0;
}

void MockCrasAudioHandler::GetAudioDevices(AudioDeviceList* device_list) const {
}

bool MockCrasAudioHandler::GetActiveOutputDevice(AudioDevice* device) const {
  return false;
}

bool MockCrasAudioHandler::has_alternative_input() const {
  return false;
}

bool MockCrasAudioHandler::has_alternative_output() const {
  return false;
}

void MockCrasAudioHandler::SetOutputVolumePercent(int volume_percent) {
}

void MockCrasAudioHandler::AdjustOutputVolumeByPercent(int adjust_by_percent) {
}

void MockCrasAudioHandler::SetOutputMute(bool mute_on) {
}

void MockCrasAudioHandler::SetInputMute(bool mute_on) {
}

void MockCrasAudioHandler::SetActiveOutputNode(uint64 node_id) {
}

void MockCrasAudioHandler::SetActiveInputNode(uint64 node_id) {
}

MockCrasAudioHandler::MockCrasAudioHandler() : CrasAudioHandler(NULL) {
}

MockCrasAudioHandler::~MockCrasAudioHandler() {
}

}  // namespace chromeos
