// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_controller.h"

#include "ash/metrics/user_metrics_action.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/bluetooth/bluetooth_feature_pod_controller.h"
#include "ash/system/bluetooth/tray_bluetooth.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/ime/ime_feature_pod_controller.h"
#include "ash/system/ime/tray_ime_chromeos.h"
#include "ash/system/network/network_feature_pod_controller.h"
#include "ash/system/network/tray_network.h"
#include "ash/system/night_light/night_light_feature_pod_controller.h"
#include "ash/system/rotation/rotation_lock_feature_pod_controller.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/unified/accessibility_feature_pod_controller.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/quiet_mode_feature_pod_controller.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/wm/lock_state_controller.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"

namespace ash {

UnifiedSystemTrayController::UnifiedSystemTrayController(
    SystemTray* system_tray)
    : system_tray_(system_tray) {}

UnifiedSystemTrayController::~UnifiedSystemTrayController() = default;

UnifiedSystemTrayView* UnifiedSystemTrayController::CreateView() {
  DCHECK(!unified_view_);
  unified_view_ = new UnifiedSystemTrayView(this);
  InitFeaturePods();

  volume_slider_controller_ = std::make_unique<UnifiedVolumeSliderController>();
  unified_view_->AddSliderView(volume_slider_controller_->CreateView());

  brightness_slider_controller_ =
      std::make_unique<UnifiedBrightnessSliderController>();
  unified_view_->AddSliderView(brightness_slider_controller_->CreateView());

  return unified_view_;
}

void UnifiedSystemTrayController::HandleSignOutAction() {
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_STATUS_AREA_SIGN_OUT);
  Shell::Get()->session_controller()->RequestSignOut();
}

void UnifiedSystemTrayController::HandleLockAction() {
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_TRAY_LOCK_SCREEN);
  chromeos::DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->RequestLockScreen();
}

void UnifiedSystemTrayController::HandleSettingsAction() {
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_TRAY_SETTINGS);
  Shell::Get()->system_tray_controller()->ShowSettings();
}

void UnifiedSystemTrayController::HandlePowerAction() {
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_TRAY_SHUT_DOWN);
  Shell::Get()->lock_state_controller()->RequestShutdown(
      ShutdownReason::TRAY_SHUT_DOWN_BUTTON);
}

void UnifiedSystemTrayController::ToggleExpanded() {
  // TODO(tetsui): Implement.
}

void UnifiedSystemTrayController::ShowNetworkDetailedView() {
  // TODO(tetsui): Implement UnifiedSystemTray's Network detailed view.

  // Initially create default view to set |default_bubble_height_|.
  system_tray_->ShowDefaultView(BubbleCreationType::BUBBLE_CREATE_NEW,
                                true /* show_by_click */);
  system_tray_->ShowDetailedView(system_tray_->GetTrayNetwork(),
                                 0 /* close_delay_in_seconds */,
                                 BubbleCreationType::BUBBLE_USE_EXISTING);
}

void UnifiedSystemTrayController::ShowBluetoothDetailedView() {
  // TODO(tetsui): Implement UnifiedSystemTray's Bluetooth detailed view.

  // Initially create default view to set |default_bubble_height_|.
  system_tray_->ShowDefaultView(BubbleCreationType::BUBBLE_CREATE_NEW,
                                true /* show_by_click */);
  system_tray_->ShowDetailedView(system_tray_->GetTrayBluetooth(),
                                 0 /* close_delay_in_seconds */,
                                 BubbleCreationType::BUBBLE_USE_EXISTING);
}

void UnifiedSystemTrayController::ShowAccessibilityDetailedView() {
  // TODO(tetsui): Implement UnifiedSystemTray's Accessibility detailed view.

  // Initially create default view to set |default_bubble_height_|.
  system_tray_->ShowDefaultView(BubbleCreationType::BUBBLE_CREATE_NEW,
                                true /* show_by_click */);
  system_tray_->ShowDetailedView(system_tray_->GetTrayAccessibility(),
                                 0 /* close_delay_in_seconds */,
                                 BubbleCreationType::BUBBLE_USE_EXISTING);
}

void UnifiedSystemTrayController::ShowIMEDetailedView() {
  // TODO(tetsui): Implement UnifiedSystemTray's IME detailed view.
  // Initially create default view to set |default_bubble_height_|.
  system_tray_->ShowDefaultView(BubbleCreationType::BUBBLE_CREATE_NEW,
                                true /* show_by_click */);
  system_tray_->ShowDetailedView(system_tray_->GetTrayIME(),
                                 0 /* close_delay_in_seconds */,
                                 BubbleCreationType::BUBBLE_USE_EXISTING);
}

void UnifiedSystemTrayController::InitFeaturePods() {
  AddFeaturePodItem(std::make_unique<NetworkFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<BluetoothFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<QuietModeFeaturePodController>());
  AddFeaturePodItem(std::make_unique<RotationLockFeaturePodController>());
  AddFeaturePodItem(std::make_unique<NightLightFeaturePodController>());
  AddFeaturePodItem(std::make_unique<AccessibilityFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<IMEFeaturePodController>(this));

  // If you want to add a new feature pod item, add here.
  // TODO(tetsui): Add more feature pod items in spec.
}

void UnifiedSystemTrayController::AddFeaturePodItem(
    std::unique_ptr<FeaturePodControllerBase> controller) {
  DCHECK(unified_view_);
  unified_view_->AddFeaturePodButton(controller->CreateButton());
  feature_pod_controllers_.push_back(std::move(controller));
}

}  // namespace ash
