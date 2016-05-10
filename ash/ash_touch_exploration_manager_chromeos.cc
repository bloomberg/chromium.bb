// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_touch_exploration_manager_chromeos.h"

#include "ash/accessibility_delegate.h"
#include "ash/audio/sounds.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_switches.h"
#include "ui/chromeos/touch_exploration_controller.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

AshTouchExplorationManager::AshTouchExplorationManager(
    RootWindowController* root_window_controller)
    : root_window_controller_(root_window_controller),
      audio_handler_(chromeos::CrasAudioHandler::Get()) {
  Shell::GetInstance()->system_tray_notifier()->AddAccessibilityObserver(this);
  ash::Shell::GetInstance()->activation_client()->AddObserver(this);
  UpdateTouchExplorationState();
}

AshTouchExplorationManager::~AshTouchExplorationManager() {
  SystemTrayNotifier* system_tray_notifier =
      Shell::GetInstance()->system_tray_notifier();
  if (system_tray_notifier)
    system_tray_notifier->RemoveAccessibilityObserver(this);
  ash::Shell::GetInstance()->activation_client()->RemoveObserver(this);
}

void AshTouchExplorationManager::OnAccessibilityModeChanged(
    ui::AccessibilityNotificationVisibility notify) {
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

void AshTouchExplorationManager::HandleAccessibilityGesture(
    ui::AXGesture gesture) {
  Shell::GetInstance()->accessibility_delegate()->HandleAccessibilityGesture(
      gesture);
}

void AshTouchExplorationManager::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  UpdateTouchExplorationState();
}

void AshTouchExplorationManager::UpdateTouchExplorationState() {
  // Comes from components/exo/shell_surface.cc.
  const char kExoShellSurfaceWindowName[] = "ExoShellSurface";

  // See crbug.com/603745 for more details.
  bool pass_through_surface =
      wm::GetActiveWindow() &&
      wm::GetActiveWindow()->name() == kExoShellSurfaceWindowName;

  AccessibilityDelegate* delegate =
      Shell::GetInstance()->accessibility_delegate();
  bool enabled = delegate->IsSpokenFeedbackEnabled();

  if (!pass_through_surface && enabled &&
      !touch_exploration_controller_.get()) {
    touch_exploration_controller_.reset(new ui::TouchExplorationController(
        root_window_controller_->GetRootWindow(), this));
  } else if (!enabled || pass_through_surface) {
    touch_exploration_controller_.reset();
  }
}

bool AshTouchExplorationManager::VolumeAdjustSoundEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableVolumeAdjustSound);
}

}  // namespace ash
