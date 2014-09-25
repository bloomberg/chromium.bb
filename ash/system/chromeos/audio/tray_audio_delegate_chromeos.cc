// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/audio/tray_audio_delegate_chromeos.h"

#include "chromeos/audio/cras_audio_handler.h"
#include "grit/ash_resources.h"

using chromeos::CrasAudioHandler;

namespace ash  {
namespace system {

void TrayAudioDelegateChromeOs::AdjustOutputVolumeToAudibleLevel() {
  CrasAudioHandler::Get()->AdjustOutputVolumeToAudibleLevel();
}

int TrayAudioDelegateChromeOs::GetOutputDefaultVolumeMuteLevel() {
  return CrasAudioHandler::Get()->GetOutputDefaultVolumeMuteThreshold();
}

int TrayAudioDelegateChromeOs::GetOutputVolumeLevel() {
  return CrasAudioHandler::Get()->GetOutputVolumePercent();
}

int TrayAudioDelegateChromeOs::GetActiveOutputDeviceIconId() {
  chromeos::AudioDevice device;
  if (!CrasAudioHandler::Get()->GetPrimaryActiveOutputDevice(&device))
    return kNoAudioDeviceIcon;

  if (device.type == chromeos::AUDIO_TYPE_HEADPHONE)
    return IDR_AURA_UBER_TRAY_AUDIO_HEADPHONE;
  else if (device.type == chromeos::AUDIO_TYPE_USB)
    return IDR_AURA_UBER_TRAY_AUDIO_USB;
  else if (device.type == chromeos::AUDIO_TYPE_BLUETOOTH)
    return IDR_AURA_UBER_TRAY_AUDIO_BLUETOOTH;
  else if (device.type == chromeos::AUDIO_TYPE_HDMI)
    return IDR_AURA_UBER_TRAY_AUDIO_HDMI;
  else
    return kNoAudioDeviceIcon;
}


bool TrayAudioDelegateChromeOs::HasAlternativeSources() {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  return (audio_handler->has_alternative_output() ||
          audio_handler->has_alternative_input());
}

bool TrayAudioDelegateChromeOs::IsOutputAudioMuted() {
  return CrasAudioHandler::Get()->IsOutputMuted();
}

void TrayAudioDelegateChromeOs::SetOutputAudioIsMuted(bool is_muted) {
  CrasAudioHandler::Get()->SetOutputMute(is_muted);
}

void TrayAudioDelegateChromeOs::SetOutputVolumeLevel(int level) {
  CrasAudioHandler::Get()->SetOutputVolumePercent(level);
}

}  // namespace system
}  // namespace ash
