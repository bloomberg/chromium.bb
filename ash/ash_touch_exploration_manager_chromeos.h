// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_EXPLORATION_MANAGER_CHROMEOS_H_
#define ASH_TOUCH_EXPLORATION_MANAGER_CHROMEOS_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/common/system/accessibility_observer.h"
#include "base/macros.h"
#include "ui/chromeos/touch_exploration_controller.h"
#include "ui/display/display_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace chromeos {
class CrasAudioHandler;
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
      public display::DisplayObserver,
      public aura::client::ActivationChangeObserver {
 public:
  explicit AshTouchExplorationManager(
      RootWindowController* root_window_controller);
  ~AshTouchExplorationManager() override;

  // AccessibilityObserver overrides:
  void OnAccessibilityModeChanged(
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

  // aura::client::ActivationChangeObserver overrides:
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // Update the touch exploration controller so that synthesized touch
  // events are anchored at this point.
  void SetTouchAccessibilityAnchorPoint(const gfx::Point& anchor_point);

 private:
  void UpdateTouchExplorationState();
  bool VolumeAdjustSoundEnabled();

  std::unique_ptr<ui::TouchExplorationController> touch_exploration_controller_;
  RootWindowController* root_window_controller_;
  chromeos::CrasAudioHandler* audio_handler_;

  DISALLOW_COPY_AND_ASSIGN(AshTouchExplorationManager);
};

}  // namespace ash

#endif  // ASH_TOUCH_EXPLORATION_MANAGER_CHROMEOS_H_
