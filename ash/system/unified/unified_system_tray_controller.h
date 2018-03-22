// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

class FeaturePodControllerBase;
class SystemTray;
class SystemTrayItem;
class UnifiedBrightnessSliderController;
class UnifiedVolumeSliderController;
class UnifiedSystemTrayView;

// Controller class of UnifiedSystemTrayView. Handles events of the view.
class ASH_EXPORT UnifiedSystemTrayController {
 public:
  // |system_tray| is used to show detailed views which are still not
  // implemented on UnifiedSystemTray.
  UnifiedSystemTrayController(SystemTray* system_tray);
  ~UnifiedSystemTrayController();

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

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTrayController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_CONTROLLER_H_
