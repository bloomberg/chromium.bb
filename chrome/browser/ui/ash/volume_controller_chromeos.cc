// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/volume_controller_chromeos.h"

#include "ash/ash_switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/system_private/system_private_api.h"
#include "content/public/browser/user_metrics.h"

using chromeos::CrasAudioHandler;

namespace {

// Percent by which the volume should be changed when a volume key is pressed.
const double kStepPercentage = 4.0;

}  // namespace

VolumeController::VolumeController() {
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

VolumeController::~VolumeController() {
  if (CrasAudioHandler::IsInitialized())
    CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

bool VolumeController::HandleVolumeMute(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_MUTE)
    content::RecordAction(content::UserMetricsAction("Accel_VolumeMute_F8"));

  CrasAudioHandler::Get()->SetOutputMute(true);
  return true;
}

bool VolumeController::HandleVolumeDown(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_DOWN)
    content::RecordAction(content::UserMetricsAction("Accel_VolumeDown_F9"));

  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  if (audio_handler->IsOutputMuted()) {
    audio_handler->SetOutputVolumePercent(0);
  } else {
    audio_handler->AdjustOutputVolumeByPercent(-kStepPercentage);
    if (audio_handler->IsOutputVolumeBelowDefaultMuteLvel())
      audio_handler->SetOutputMute(true);
  }
  return true;
}

bool VolumeController::HandleVolumeUp(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_UP)
    content::RecordAction(content::UserMetricsAction("Accel_VolumeUp_F10"));

  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  if (audio_handler->IsOutputMuted()) {
      audio_handler->SetOutputMute(false);
      audio_handler->AdjustOutputVolumeToAudibleLevel();
  } else {
    audio_handler->AdjustOutputVolumeByPercent(kStepPercentage);
  }

  return true;
}

void VolumeController::OnOutputVolumeChanged() {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  extensions::DispatchVolumeChangedEvent(
      audio_handler->GetOutputVolumePercent(),
      audio_handler->IsOutputMuted());
}

void VolumeController::OnOutputMuteChanged() {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  extensions::DispatchVolumeChangedEvent(
      audio_handler->GetOutputVolumePercent(),
      audio_handler->IsOutputMuted());
}
