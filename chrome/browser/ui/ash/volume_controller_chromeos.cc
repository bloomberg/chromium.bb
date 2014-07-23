// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/volume_controller_chromeos.h"

#include "ash/ash_switches.h"
#include "ash/audio/sounds.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/system_private/system_private_api.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/user_metrics.h"
#include "grit/browser_resources.h"
#include "media/audio/sounds/sounds_manager.h"
#include "ui/base/resource/resource_bundle.h"

using chromeos::CrasAudioHandler;

namespace {

// Percent by which the volume should be changed when a volume key is pressed.
const double kStepPercentage = 4.0;

bool VolumeAdjustSoundEnabled() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableVolumeAdjustSound);
}

void PlayVolumeAdjustSound() {
  if (VolumeAdjustSoundEnabled())
    ash::PlaySystemSoundIfSpokenFeedback(chromeos::SOUND_VOLUME_ADJUST);
}

}  // namespace

VolumeController::VolumeController() {
  CrasAudioHandler::Get()->AddAudioObserver(this);
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  if (VolumeAdjustSoundEnabled()) {
    media::SoundsManager::Get()->Initialize(
        chromeos::SOUND_VOLUME_ADJUST,
        bundle.GetRawDataResource(IDR_SOUND_VOLUME_ADJUST_WAV));
  }
}

VolumeController::~VolumeController() {
  if (CrasAudioHandler::IsInitialized())
    CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

bool VolumeController::HandleVolumeMute(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_MUTE)
    content::RecordAction(base::UserMetricsAction("Accel_VolumeMute_F8"));

  CrasAudioHandler::Get()->SetOutputMute(true);
  return true;
}

bool VolumeController::HandleVolumeDown(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_DOWN)
    content::RecordAction(base::UserMetricsAction("Accel_VolumeDown_F9"));

  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  if (audio_handler->IsOutputMuted()) {
    audio_handler->SetOutputVolumePercent(0);
  } else {
    audio_handler->AdjustOutputVolumeByPercent(-kStepPercentage);
    if (audio_handler->IsOutputVolumeBelowDefaultMuteLevel())
      audio_handler->SetOutputMute(true);
    else
      PlayVolumeAdjustSound();
  }
  return true;
}

bool VolumeController::HandleVolumeUp(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_UP)
    content::RecordAction(base::UserMetricsAction("Accel_VolumeUp_F10"));

  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  bool play_sound = false;
  if (audio_handler->IsOutputMuted()) {
    audio_handler->SetOutputMute(false);
    audio_handler->AdjustOutputVolumeToAudibleLevel();
    play_sound = true;
  } else {
    play_sound = audio_handler->GetOutputVolumePercent() != 100;
    audio_handler->AdjustOutputVolumeByPercent(kStepPercentage);
  }

  if (play_sound)
    PlayVolumeAdjustSound();
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
