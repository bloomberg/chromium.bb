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
#include "ash/system/network/tray_vpn.h"
#include "ash/system/network/vpn_feature_pod_controller.h"
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
#include "base/numerics/ranges.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "ui/gfx/animation/slide_animation.h"

namespace ash {

namespace {

// Animation duration to collapse / expand the view in milliseconds.
const int kExpandAnimationDurationMs = 500;
// Threshold in pixel that fully collapses / expands the view through gesture.
const int kDragThreshold = 200;

}  // namespace

UnifiedSystemTrayController::UnifiedSystemTrayController(
    SystemTray* system_tray)
    : system_tray_(system_tray),
      animation_(std::make_unique<gfx::SlideAnimation>(this)) {
  animation_->Reset(1.0);
  animation_->SetSlideDuration(kExpandAnimationDurationMs);
  animation_->SetTweenType(gfx::Tween::EASE_IN_OUT);
}

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
  if (animation_->IsShowing())
    animation_->Hide();
  else
    animation_->Show();
}

void UnifiedSystemTrayController::BeginDrag(const gfx::Point& location) {
  drag_init_point_ = location;
  was_expanded_ = animation_->IsShowing();
}

void UnifiedSystemTrayController::UpdateDrag(const gfx::Point& location) {
  animation_->Reset(GetDragExpandedAmount(location));
  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::EndDrag(const gfx::Point& location) {
  // If dragging is finished, animate to closer state.
  if (GetDragExpandedAmount(location) > 0.5) {
    animation_->Show();
  } else {
    // To animate to hidden state, first set SlideAnimation::IsShowing() to
    // true.
    animation_->Show();
    animation_->Hide();
  }
}

void UnifiedSystemTrayController::ShowNetworkDetailedView() {
  // TODO(tetsui): Implement UnifiedSystemTray's Network detailed view.
  ShowSystemTrayDetailedView(system_tray_->GetTrayNetwork());
}

void UnifiedSystemTrayController::ShowBluetoothDetailedView() {
  // TODO(tetsui): Implement UnifiedSystemTray's Bluetooth detailed view.
  ShowSystemTrayDetailedView(system_tray_->GetTrayBluetooth());
}

void UnifiedSystemTrayController::ShowAccessibilityDetailedView() {
  // TODO(tetsui): Implement UnifiedSystemTray's Accessibility detailed view.
  ShowSystemTrayDetailedView(system_tray_->GetTrayAccessibility());
}

void UnifiedSystemTrayController::ShowVPNDetailedView() {
  // TODO(tetsui): Implement UnifiedSystemTray's VPN detailed view.
  ShowSystemTrayDetailedView(system_tray_->GetTrayVPN());
}

void UnifiedSystemTrayController::ShowIMEDetailedView() {
  // TODO(tetsui): Implement UnifiedSystemTray's IME detailed view.
  ShowSystemTrayDetailedView(system_tray_->GetTrayIME());
}

void UnifiedSystemTrayController::AnimationEnded(
    const gfx::Animation* animation) {
  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::AnimationProgressed(
    const gfx::Animation* animation) {
  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::AnimationCanceled(
    const gfx::Animation* animation) {
  animation_->Reset(std::round(animation_->GetCurrentValue()));
  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::InitFeaturePods() {
  AddFeaturePodItem(std::make_unique<NetworkFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<BluetoothFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<QuietModeFeaturePodController>());
  AddFeaturePodItem(std::make_unique<RotationLockFeaturePodController>());
  AddFeaturePodItem(std::make_unique<NightLightFeaturePodController>());
  AddFeaturePodItem(std::make_unique<AccessibilityFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<VPNFeaturePodController>(this));
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

void UnifiedSystemTrayController::ShowSystemTrayDetailedView(
    SystemTrayItem* system_tray_item) {
  // Initially create default view to set |default_bubble_height_|.
  system_tray_->ShowDefaultView(BubbleCreationType::BUBBLE_CREATE_NEW,
                                true /* show_by_click */);
  system_tray_->ShowDetailedView(system_tray_item,
                                 0 /* close_delay_in_seconds */,
                                 BubbleCreationType::BUBBLE_USE_EXISTING);
}

void UnifiedSystemTrayController::UpdateExpandedAmount() {
  unified_view_->SetExpandedAmount(animation_->GetCurrentValue());
}

double UnifiedSystemTrayController::GetDragExpandedAmount(
    const gfx::Point& location) const {
  double y_diff = (location - drag_init_point_).y();

  // If already expanded, only consider swiping down. Otherwise, only consider
  // swiping up.
  if (was_expanded_) {
    return base::ClampToRange(1.0 - std::max(0.0, y_diff) / kDragThreshold, 0.0,
                              1.0);
  } else {
    return base::ClampToRange(std::max(0.0, -y_diff) / kDragThreshold, 0.0,
                              1.0);
  }
}

}  // namespace ash
