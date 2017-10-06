// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_touch_exploration_manager_chromeos.h"

#include <vector>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/accessibility/accessibility_focus_ring_controller.h"
#include "ash/keyboard/keyboard_observer_register.h"
#include "ash/public/cpp/app_types.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_switches.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/chromeos/touch_exploration_controller.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

AshTouchExplorationManager::AshTouchExplorationManager(
    RootWindowController* root_window_controller)
    : root_window_controller_(root_window_controller),
      audio_handler_(chromeos::CrasAudioHandler::Get()),
      keyboard_observer_(this) {
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->system_tray_notifier()->AddAccessibilityObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
  UpdateTouchExplorationState();
}

AshTouchExplorationManager::~AshTouchExplorationManager() {
  SystemTrayNotifier* system_tray_notifier =
      Shell::Get()->system_tray_notifier();
  if (system_tray_notifier)
    system_tray_notifier->RemoveAccessibilityObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
}

void AshTouchExplorationManager::OnAccessibilityStatusChanged(
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
  if (Shell::Get()->accessibility_delegate()->IsSpokenFeedbackEnabled())
    Shell::Get()->accessibility_delegate()->SilenceSpokenFeedback();
}

void AshTouchExplorationManager::PlayVolumeAdjustEarcon() {
  if (!VolumeAdjustSoundEnabled())
    return;
  if (!audio_handler_->IsOutputMuted() &&
      audio_handler_->GetOutputVolumePercent() != 100) {
    Shell::Get()->accessibility_delegate()->PlayEarcon(
        chromeos::SOUND_VOLUME_ADJUST);
  }
}

void AshTouchExplorationManager::PlayPassthroughEarcon() {
  Shell::Get()->accessibility_delegate()->PlayEarcon(
      chromeos::SOUND_PASSTHROUGH);
}

void AshTouchExplorationManager::PlayExitScreenEarcon() {
  Shell::Get()->accessibility_delegate()->PlayEarcon(
      chromeos::SOUND_EXIT_SCREEN);
}

void AshTouchExplorationManager::PlayEnterScreenEarcon() {
  Shell::Get()->accessibility_delegate()->PlayEarcon(
      chromeos::SOUND_ENTER_SCREEN);
}

void AshTouchExplorationManager::HandleAccessibilityGesture(
    ui::AXGesture gesture) {
  Shell::Get()->accessibility_delegate()->HandleAccessibilityGesture(gesture);
}

void AshTouchExplorationManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  const display::Display this_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          root_window_controller_->GetRootWindow());
  if (this_display.id() == display.id())
    UpdateTouchExplorationState();
}

void AshTouchExplorationManager::OnTwoFingerTouchStart() {
  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
  delegate->OnTwoFingerTouchStart();
}

void AshTouchExplorationManager::OnTwoFingerTouchStop() {
  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
  delegate->OnTwoFingerTouchStop();
}

void AshTouchExplorationManager::PlaySpokenFeedbackToggleCountdown(
    int tick_count) {
  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
  if (delegate->ShouldToggleSpokenFeedbackViaTouch())
    delegate->PlaySpokenFeedbackToggleCountdown(tick_count);
}

void AshTouchExplorationManager::PlayTouchTypeEarcon() {
  Shell::Get()->accessibility_delegate()->PlayEarcon(
      chromeos::SOUND_TOUCH_TYPE);
}

void AshTouchExplorationManager::ToggleSpokenFeedback() {
  AccessibilityDelegate* delegate = Shell::Get()->accessibility_delegate();
  if (delegate->ShouldToggleSpokenFeedbackViaTouch())
    delegate->ToggleSpokenFeedback(ash::A11Y_NOTIFICATION_SHOW);
}

void AshTouchExplorationManager::OnWindowActivated(
    ::wm::ActivationChangeObserver::ActivationReason reason,
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

void AshTouchExplorationManager::OnKeyboardBoundsChanging(
    const gfx::Rect& new_bounds) {
  UpdateTouchExplorationState();
}

void AshTouchExplorationManager::OnKeyboardClosed() {
  keyboard_observer_.RemoveAll();
  UpdateTouchExplorationState();
}

void AshTouchExplorationManager::OnVirtualKeyboardStateChanged(
    bool activated,
    aura::Window* root_window) {
  UpdateKeyboardObserverFromStateChanged(
      activated, root_window, root_window_controller_->GetRootWindow(),
      &keyboard_observer_);
}

void AshTouchExplorationManager::UpdateTouchExplorationState() {
  // See crbug.com/603745 for more details.
  const bool pass_through_surface =
      wm::GetActiveWindow() &&
      wm::GetActiveWindow()->GetProperty(
          aura::client::kAccessibilityTouchExplorationPassThrough);

  const bool spoken_feedback_enabled =
      Shell::Get()->accessibility_delegate()->IsSpokenFeedbackEnabled();

  if (!touch_accessibility_enabler_) {
    // Always enable gesture to toggle spoken feedback.
    touch_accessibility_enabler_.reset(new ui::TouchAccessibilityEnabler(
        root_window_controller_->GetRootWindow(), this));
  }

  if (spoken_feedback_enabled) {
    if (!touch_exploration_controller_.get()) {
      touch_exploration_controller_ =
          base::MakeUnique<ui::TouchExplorationController>(
              root_window_controller_->GetRootWindow(), this,
              touch_accessibility_enabler_->GetWeakPtr());
    }
    if (pass_through_surface) {
      const display::Display display =
          display::Screen::GetScreen()->GetDisplayNearestWindow(
              root_window_controller_->GetRootWindow());
      const gfx::Rect work_area = display.work_area();
      touch_exploration_controller_->SetExcludeBounds(work_area);
      SilenceSpokenFeedback();
      // Clear the focus highlight.
      AccessibilityFocusRingController::GetInstance()->SetFocusRing(
          std::vector<gfx::Rect>(),
          AccessibilityFocusRingController::PERSIST_FOCUS_RING);
    } else {
      touch_exploration_controller_->SetExcludeBounds(gfx::Rect());
    }

    // Virtual keyboard.
    keyboard::KeyboardController* keyboard_controller =
        keyboard::KeyboardController::GetInstance();
    if (keyboard_controller) {
      touch_exploration_controller_->SetLiftActivationBounds(
          keyboard_controller->current_keyboard_bounds());
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
