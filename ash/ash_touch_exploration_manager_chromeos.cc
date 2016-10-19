// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_touch_exploration_manager_chromeos.h"

#include "ash/aura/wm_root_window_controller_aura.h"
#include "ash/common/accessibility_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
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
  WmShell::Get()->system_tray_notifier()->AddAccessibilityObserver(this);
  Shell::GetInstance()->activation_client()->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
  UpdateTouchExplorationState();
}

AshTouchExplorationManager::~AshTouchExplorationManager() {
  SystemTrayNotifier* system_tray_notifier =
      WmShell::Get()->system_tray_notifier();
  if (system_tray_notifier)
    system_tray_notifier->RemoveAccessibilityObserver(this);
  Shell::GetInstance()->activation_client()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
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
  if (WmShell::Get()->accessibility_delegate()->IsSpokenFeedbackEnabled())
    WmShell::Get()->accessibility_delegate()->SilenceSpokenFeedback();
}

void AshTouchExplorationManager::PlayVolumeAdjustEarcon() {
  if (!VolumeAdjustSoundEnabled())
    return;
  if (!audio_handler_->IsOutputMuted() &&
      audio_handler_->GetOutputVolumePercent() != 100) {
    WmShell::Get()->accessibility_delegate()->PlayEarcon(
        chromeos::SOUND_VOLUME_ADJUST);
  }
}

void AshTouchExplorationManager::PlayPassthroughEarcon() {
  WmShell::Get()->accessibility_delegate()->PlayEarcon(
      chromeos::SOUND_PASSTHROUGH);
}

void AshTouchExplorationManager::PlayExitScreenEarcon() {
  WmShell::Get()->accessibility_delegate()->PlayEarcon(
      chromeos::SOUND_EXIT_SCREEN);
}

void AshTouchExplorationManager::PlayEnterScreenEarcon() {
  WmShell::Get()->accessibility_delegate()->PlayEarcon(
      chromeos::SOUND_ENTER_SCREEN);
}

void AshTouchExplorationManager::HandleAccessibilityGesture(
    ui::AXGesture gesture) {
  WmShell::Get()->accessibility_delegate()->HandleAccessibilityGesture(gesture);
}

void AshTouchExplorationManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (root_window_controller_->wm_root_window_controller()
          ->GetWindow()
          ->GetDisplayNearestWindow()
          .id() == display.id())
    UpdateTouchExplorationState();
}

void AshTouchExplorationManager::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  UpdateTouchExplorationState();
}

void AshTouchExplorationManager::SetTouchAccessibilityAnchorPoint(
    const gfx::Point& anchor_point) {
  if (touch_exploration_controller_) {
    touch_exploration_controller_->SetTouchAccessibilityAnchorPoint(
        anchor_point);
  }
}

void AshTouchExplorationManager::UpdateTouchExplorationState() {
  // Comes from components/exo/shell_surface.cc.
  const char kExoShellSurfaceWindowName[] = "ExoShellSurface";

  // See crbug.com/603745 for more details.
  const bool pass_through_surface =
      wm::GetActiveWindow() &&
      wm::GetActiveWindow()->name() == kExoShellSurfaceWindowName;

  const bool spoken_feedback_enabled =
      WmShell::Get()->accessibility_delegate()->IsSpokenFeedbackEnabled();

  if (spoken_feedback_enabled) {
    if (!touch_exploration_controller_.get()) {
      touch_exploration_controller_ =
          base::MakeUnique<ui::TouchExplorationController>(
              root_window_controller_->GetRootWindow(), this);
    }
    if (pass_through_surface) {
      const gfx::Rect& work_area =
          root_window_controller_->wm_root_window_controller()
              ->GetWindow()
              ->GetDisplayNearestWindow()
              .work_area();
      touch_exploration_controller_->SetExcludeBounds(work_area);
      SilenceSpokenFeedback();
      WmShell::Get()->accessibility_delegate()->ClearFocusHighlight();
    } else {
      touch_exploration_controller_->SetExcludeBounds(gfx::Rect());
    }
  } else {
    touch_exploration_controller_.reset();
  }
}

bool AshTouchExplorationManager::VolumeAdjustSoundEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableVolumeAdjustSound);
}

}  // namespace ash
