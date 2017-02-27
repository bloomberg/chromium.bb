// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/audio/tray_audio_delegate_chromeos.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "ui/gfx/paint_vector_icon.h"

using chromeos::CrasAudioHandler;

namespace ash {
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

const gfx::VectorIcon&
TrayAudioDelegateChromeOs::GetActiveOutputDeviceVectorIcon() {
  chromeos::AudioDevice device;
  if (CrasAudioHandler::Get()->GetPrimaryActiveOutputDevice(&device)) {
    if (device.type == chromeos::AUDIO_TYPE_HEADPHONE)
      return kSystemMenuHeadsetIcon;
    if (device.type == chromeos::AUDIO_TYPE_USB)
      return kSystemMenuUsbIcon;
    if (device.type == chromeos::AUDIO_TYPE_BLUETOOTH)
      return kSystemMenuBluetoothIcon;
    if (device.type == chromeos::AUDIO_TYPE_HDMI)
      return kSystemMenuHdmiIcon;
  }
  return gfx::kNoneIcon;
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

void TrayAudioDelegateChromeOs::SetInternalSpeakerChannelMode(
    AudioChannelMode mode) {
  CrasAudioHandler::Get()->SwapInternalSpeakerLeftRightChannel(
      mode == LEFT_RIGHT_SWAPPED);
}

void TrayAudioDelegateChromeOs::SetActiveHDMIOutoutRediscoveringIfNecessary(
    bool force_rediscovering) {
  CrasAudioHandler::Get()->SetActiveHDMIOutoutRediscoveringIfNecessary(
      force_rediscovering);
}

}  // namespace system
}  // namespace ash
