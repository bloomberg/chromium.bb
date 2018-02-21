// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_touch_exploration_manager_chromeos.h"

#include <memory>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_focus_ring_controller.h"
#include "ash/accessibility/touch_exploration_controller.h"
#include "ash/keyboard/keyboard_observer_register.h"
#include "ash/public/cpp/app_types.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_switches.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

AccessibilityController* GetA11yController() {
  return Shell::Get()->accessibility_controller();
}

}  // namespace

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
  if (GetA11yController()->IsSpokenFeedbackEnabled())
    GetA11yController()->SilenceSpokenFeedback();
}

void AshTouchExplorationManager::PlayVolumeAdjustEarcon() {
  if (!VolumeAdjustSoundEnabled())
    return;
  if (!audio_handler_->IsOutputMuted() &&
      audio_handler_->GetOutputVolumePercent() != 100) {
    GetA11yController()->PlayEarcon(chromeos::SOUND_VOLUME_ADJUST);
  }
}

void AshTouchExplorationManager::PlayPassthroughEarcon() {
  GetA11yController()->PlayEarcon(chromeos::SOUND_PASSTHROUGH);
}

void AshTouchExplorationManager::PlayExitScreenEarcon() {
  GetA11yController()->PlayEarcon(chromeos::SOUND_EXIT_SCREEN);
}

void AshTouchExplorationManager::PlayEnterScreenEarcon() {
  GetA11yController()->PlayEarcon(chromeos::SOUND_ENTER_SCREEN);
}

void AshTouchExplorationManager::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture) {
  GetA11yController()->HandleAccessibilityGesture(gesture);
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
  GetA11yController()->OnTwoFingerTouchStart();
}

void AshTouchExplorationManager::OnTwoFingerTouchStop() {
  GetA11yController()->OnTwoFingerTouchStop();
}

void AshTouchExplorationManager::PlaySpokenFeedbackToggleCountdown(
    int tick_count) {
  GetA11yController()->ShouldToggleSpokenFeedbackViaTouch(base::BindOnce(
      [](int tick_count, bool should_toggle) {
        if (!should_toggle)
          return;
        GetA11yController()->PlaySpokenFeedbackToggleCountdown(tick_count);
      },
      tick_count));
}

void AshTouchExplorationManager::PlayTouchTypeEarcon() {
  GetA11yController()->PlayEarcon(chromeos::SOUND_TOUCH_TYPE);
}

void AshTouchExplorationManager::ToggleSpokenFeedback() {
  GetA11yController()->ShouldToggleSpokenFeedbackViaTouch(
      base::BindOnce([](bool should_toggle) {
        if (!should_toggle)
          return;
        GetA11yController()->SetSpokenFeedbackEnabled(
            !GetA11yController()->IsSpokenFeedbackEnabled(),
            ash::A11Y_NOTIFICATION_SHOW);
      }));
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

void AshTouchExplorationManager::OnKeyboardVisibleBoundsChanged(
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
      GetA11yController()->IsSpokenFeedbackEnabled();

  if (!touch_accessibility_enabler_) {
    // Always enable gesture to toggle spoken feedback.
    touch_accessibility_enabler_.reset(new TouchAccessibilityEnabler(
        root_window_controller_->GetRootWindow(), this));
  }

  if (spoken_feedback_enabled) {
    if (!touch_exploration_controller_.get()) {
      touch_exploration_controller_ =
          std::make_unique<TouchExplorationController>(
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
