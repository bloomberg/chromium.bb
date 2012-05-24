// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/volume_controller_chromeos.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/audio/audio_handler.h"
#include "chrome/browser/extensions/system/system_api.h"
#include "content/public/browser/user_metrics.h"

namespace {

// Percent by which the volume should be changed when a volume key is pressed.
const double kStepPercentage = 4.0;

}  // namespace

bool VolumeController::HandleVolumeMute(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_F8)
    content::RecordAction(content::UserMetricsAction("Accel_VolumeMute_F8"));

  chromeos::AudioHandler* audio_handler = chromeos::AudioHandler::GetInstance();

  // Always muting (and not toggling) as per final decision on
  // http://crosbug.com/3751
  audio_handler->SetMuted(true);

  extensions::DispatchVolumeChangedEvent(audio_handler->GetVolumePercent(),
                                         audio_handler->IsMuted());
  return true;
}

bool VolumeController::HandleVolumeDown(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_F9)
    content::RecordAction(content::UserMetricsAction("Accel_VolumeDown_F9"));

  chromeos::AudioHandler* audio_handler = chromeos::AudioHandler::GetInstance();
  if (audio_handler->IsMuted())
    audio_handler->SetVolumePercent(0.0);
  else
    audio_handler->AdjustVolumeByPercent(-kStepPercentage);

  extensions::DispatchVolumeChangedEvent(audio_handler->GetVolumePercent(),
                                         audio_handler->IsMuted());
  return true;
}

bool VolumeController::HandleVolumeUp(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_F10)
    content::RecordAction(content::UserMetricsAction("Accel_VolumeUp_F10"));

  chromeos::AudioHandler* audio_handler = chromeos::AudioHandler::GetInstance();
  if (audio_handler->IsMuted()) {
    audio_handler->SetMuted(false);
   // If volume percent is still 0.0 after reset the mute status, it means that
    // the mute status was done by VolumeDown, so we need to increase
    // the volume as usual.
    if (audio_handler->GetVolumePercent() == 0.0)
      audio_handler->AdjustVolumeByPercent(kStepPercentage);
  } else {
    audio_handler->AdjustVolumeByPercent(kStepPercentage);
  }

  extensions::DispatchVolumeChangedEvent(audio_handler->GetVolumePercent(),
                                         audio_handler->IsMuted());
  return true;
}

void VolumeController::SetVolumePercent(double percent) {
  chromeos::AudioHandler* audio_handler = chromeos::AudioHandler::GetInstance();
  audio_handler->SetVolumePercent(percent);
  extensions::DispatchVolumeChangedEvent(audio_handler->GetVolumePercent(),
                                         audio_handler->IsMuted());
}
