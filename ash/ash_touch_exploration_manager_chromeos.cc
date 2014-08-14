// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_touch_exploration_manager_chromeos.h"

#include "ash/accessibility_delegate.h"
#include "ash/audio/sounds.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/command_line.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_switches.h"
#include "ui/chromeos/touch_exploration_controller.h"

namespace ash {

AshTouchExplorationManager::AshTouchExplorationManager(
    RootWindowController* root_window_controller)
    : root_window_controller_(root_window_controller),
      audio_handler_(chromeos::CrasAudioHandler::Get()) {
  Shell::GetInstance()->system_tray_notifier()->AddAccessibilityObserver(this);
  UpdateTouchExplorationState();
}

AshTouchExplorationManager::~AshTouchExplorationManager() {
  SystemTrayNotifier* system_tray_notifier =
      Shell::GetInstance()->system_tray_notifier();
  if (system_tray_notifier)
    system_tray_notifier->RemoveAccessibilityObserver(this);
}

void AshTouchExplorationManager::OnAccessibilityModeChanged(
    AccessibilityNotificationVisibility notify) {
  UpdateTouchExplorationState();
}

void AshTouchExplorationManager::SetOutputLevel(int volume) {
  if (volume > 0) {
    if (audio_handler_->IsOutputMuted()) {
      audio_handler_->SetOutputMute(false);
    }
  }
  audio_handler_->SetOutputVolumePercent(volume);
  // Avoid negative volume.
  if (audio_handler_->IsOutputVolumeBelowDefaultMuteLevel())
    audio_handler_->SetOutputMute(true);
}

void AshTouchExplorationManager::SilenceSpokenFeedback() {
  AccessibilityDelegate* delegate =
    Shell::GetInstance()->accessibility_delegate();
  if (!delegate->IsSpokenFeedbackEnabled())
    return;
  delegate->SilenceSpokenFeedback();
}

void AshTouchExplorationManager::PlayVolumeAdjustEarcon() {
  if (!VolumeAdjustSoundEnabled())
    return;
  if (!audio_handler_->IsOutputMuted() &&
      audio_handler_->GetOutputVolumePercent() != 100)
    PlaySystemSoundIfSpokenFeedback(chromeos::SOUND_VOLUME_ADJUST);
}

void AshTouchExplorationManager::PlayPassthroughEarcon() {
  Shell::GetInstance()->accessibility_delegate()->PlayEarcon(
      chromeos::SOUND_PASSTHROUGH);
}

void AshTouchExplorationManager::PlayExitScreenEarcon() {
  Shell::GetInstance()->accessibility_delegate()->PlayEarcon(
      chromeos::SOUND_EXIT_SCREEN);
}

void AshTouchExplorationManager::PlayEnterScreenEarcon() {
  Shell::GetInstance()->accessibility_delegate()->PlayEarcon(
      chromeos::SOUND_ENTER_SCREEN);
}

void AshTouchExplorationManager::UpdateTouchExplorationState() {
  AccessibilityDelegate* delegate =
      Shell::GetInstance()->accessibility_delegate();
  bool enabled = delegate->IsSpokenFeedbackEnabled();

  if (enabled && !touch_exploration_controller_.get()) {
    touch_exploration_controller_.reset(new ui::TouchExplorationController(
        root_window_controller_->GetRootWindow(), this));
  } else if (!enabled) {
    touch_exploration_controller_.reset();
  }
}

bool AshTouchExplorationManager::VolumeAdjustSoundEnabled() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableVolumeAdjustSound);
}

}  // namespace ash
