// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/point.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace ash {

class FeaturePodControllerBase;
class SystemTray;
class SystemTrayItem;
class UnifiedBrightnessSliderController;
class UnifiedVolumeSliderController;
class UnifiedSystemTrayView;

// Controller class of UnifiedSystemTrayView. Handles events of the view.
class ASH_EXPORT UnifiedSystemTrayController : public gfx::AnimationDelegate {
 public:
  // |system_tray| is used to show detailed views which are still not
  // implemented on UnifiedSystemTray.
  UnifiedSystemTrayController(SystemTray* system_tray);
  ~UnifiedSystemTrayController() override;

  // Create the view. The created view is unowned.
  UnifiedSystemTrayView* CreateView();

  // Sign out from the current user. Called from the view.
  void HandleSignOutAction();
  // Show lock screen which asks the user password. Called from the view.
  void HandleLockAction();
  // Show WebUI settings. Called from the view.
  void HandleSettingsAction();
  // Shutdown the computer. Called from the view.
  void HandlePowerAction();
  // Toggle expanded state of UnifiedSystemTrayView. Called from the view.
  void ToggleExpanded();

  // Handle finger dragging and expand/collapse the view. Called from view.
  void BeginDrag(const gfx::Point& location);
  void UpdateDrag(const gfx::Point& location);
  void EndDrag(const gfx::Point& location);

  // Show the detailed view of network. Called from the view.
  void ShowNetworkDetailedView();
  // Show the detailed view of bluetooth. Called from the view.
  void ShowBluetoothDetailedView();
  // Show the detailed view of accessibility. Called from the view.
  void ShowAccessibilityDetailedView();
  // Show the detailed view of VPN. Called from the view.
  void ShowVPNDetailedView();
  // Show the detailed view of IME. Called from the view.
  void ShowIMEDetailedView();

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

 private:
  // Initialize feature pod controllers and their views.
  // If you want to add a new feature pod item, you have to add here.
  void InitFeaturePods();

  // Add the feature pod controller and its view.
  void AddFeaturePodItem(std::unique_ptr<FeaturePodControllerBase> controller);

  // Show detailed view of SystemTray.
  // TODO(tetsui): Remove when detailed views are implemented on
  // UnifiedSystemTray.
  void ShowSystemTrayDetailedView(SystemTrayItem* system_tray_item);

  // Update how much the view is expanded based on |animation_|.
  void UpdateExpandedAmount();

  // Return touch drag amount between 0.0 and 1.0. If expanding, it increases
  // towards 1.0. If collapsing, it decreases towards 0.0. If the view is
  // dragged to the same direction as the current state, it does not change the
  // value. For example, if the view is expanded and it's dragged to the top, it
  // keeps returning 1.0.
  double GetDragExpandedAmount(const gfx::Point& location) const;

  // Only used to show detailed views which are still not implemented on
  // UnifiedSystemTray. Unowned.
  // TODO(tetsui): Remove reference to |system_tray|.
  SystemTray* const system_tray_;

  // Unowned. Owned by Views hierarchy.
  UnifiedSystemTrayView* unified_view_ = nullptr;

  // Controllers of feature pod buttons. Owned by this.
  std::vector<std::unique_ptr<FeaturePodControllerBase>>
      feature_pod_controllers_;

  // Controller of volume slider. Owned.
  std::unique_ptr<UnifiedVolumeSliderController> volume_slider_controller_;

  // Controller of brightness slider. Owned.
  std::unique_ptr<UnifiedBrightnessSliderController>
      brightness_slider_controller_;

  // If the previous state is expanded or not. Only valid during dragging (from
  // BeginDrag to EndDrag).
  bool was_expanded_ = true;

  // The last |location| passed to BeginDrag(). Only valid during dragging.
  gfx::Point drag_init_point_;

  // Animation between expanded and collapsed states.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTrayController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_
