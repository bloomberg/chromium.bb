// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_EXPLORATION_MANAGER_CHROMEOS_H_
#define ASH_TOUCH_EXPLORATION_MANAGER_CHROMEOS_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/system/accessibility_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/chromeos/touch_accessibility_enabler.h"
#include "ui/chromeos/touch_exploration_controller.h"
#include "ui/display/display_observer.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace chromeos {
class CrasAudioHandler;
}

namespace keyboard {
class KeyboardController;
}

namespace ash {
class RootWindowController;

// Responsible for initializing TouchExplorationController when spoken
// feedback is on for ChromeOS only. This class implements
// TouchExplorationControllerDelegate which allows touch gestures to manipulate
// the system.
class ASH_EXPORT AshTouchExplorationManager
    : public AccessibilityObserver,
      public ui::TouchExplorationControllerDelegate,
      public ui::TouchAccessibilityEnablerDelegate,
      public display::DisplayObserver,
      public ::wm::ActivationChangeObserver,
      public keyboard::KeyboardControllerObserver,
      public ShellObserver {
 public:
  explicit AshTouchExplorationManager(
      RootWindowController* root_window_controller);
  ~AshTouchExplorationManager() override;

  // AccessibilityObserver overrides:
  void OnAccessibilityStatusChanged(
      AccessibilityNotificationVisibility notify) override;

  // TouchExplorationControllerDelegate overrides:
  void SetOutputLevel(int volume) override;
  void SilenceSpokenFeedback() override;
  void PlayVolumeAdjustEarcon() override;
  void PlayPassthroughEarcon() override;
  void PlayExitScreenEarcon() override;
  void PlayEnterScreenEarcon() override;
  void HandleAccessibilityGesture(ui::AXGesture gesture) override;

  // display::DisplayObserver overrides:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // TouchAccessibilityEnablerDelegate overrides:
  void OnTwoFingerTouchStart() override;
  void OnTwoFingerTouchStop() override;
  void PlaySpokenFeedbackToggleCountdown(int tick_count) override;
  void ToggleSpokenFeedback() override;

  // wm::ActivationChangeObserver overrides:
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // Update the touch exploration controller so that synthesized touch
  // events are anchored at this point.
  void SetTouchAccessibilityAnchorPoint(const gfx::Point& anchor_point);

 private:
  // keyboard::KeyboardControllerObserver overrides:
  void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override;
  void OnKeyboardClosed() override;

  // ShellObserver overrides:
  void OnVirtualKeyboardStateChanged(bool activated,
                                     aura::Window* root_window) override;

  void UpdateTouchExplorationState();
  bool VolumeAdjustSoundEnabled();

  std::unique_ptr<ui::TouchExplorationController> touch_exploration_controller_;
  std::unique_ptr<ui::TouchAccessibilityEnabler> touch_accessibility_enabler_;
  RootWindowController* root_window_controller_;
  chromeos::CrasAudioHandler* audio_handler_;
  const bool enable_chromevox_arc_support_;
  ScopedObserver<keyboard::KeyboardController,
                 keyboard::KeyboardControllerObserver>
      keyboard_observer_;

  DISALLOW_COPY_AND_ASSIGN(AshTouchExplorationManager);
};

}  // namespace ash

#endif  // ASH_TOUCH_EXPLORATION_MANAGER_CHROMEOS_H_
