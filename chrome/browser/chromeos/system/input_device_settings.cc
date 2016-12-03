// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/input_device_settings.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/statistics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace system {

namespace {

PrefService* GetActiveProfilePrefs() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return profile ? profile->GetPrefs() : nullptr;
}

}  // namespace

TouchpadSettings::TouchpadSettings() {
}

TouchpadSettings::TouchpadSettings(const TouchpadSettings& other) = default;

TouchpadSettings& TouchpadSettings::operator=(const TouchpadSettings& other) {
  if (&other != this) {
    sensitivity_ = other.sensitivity_;
    tap_to_click_ = other.tap_to_click_;
    three_finger_click_ = other.three_finger_click_;
    tap_dragging_ = other.tap_dragging_;
    natural_scroll_ = other.natural_scroll_;
  }
  return *this;
}

void TouchpadSettings::SetSensitivity(int value) {
  sensitivity_.Set(value);
}

int TouchpadSettings::GetSensitivity() const {
  return sensitivity_.value();
}

bool TouchpadSettings::IsSensitivitySet() const {
  return sensitivity_.is_set();
}

void TouchpadSettings::SetTapToClick(bool enabled) {
  tap_to_click_.Set(enabled);
}

bool TouchpadSettings::GetTapToClick() const {
  return tap_to_click_.value();
}

bool TouchpadSettings::IsTapToClickSet() const {
  return tap_to_click_.is_set();
}

void TouchpadSettings::SetNaturalScroll(bool enabled) {
  natural_scroll_.Set(enabled);
}

bool TouchpadSettings::GetNaturalScroll() const {
  return natural_scroll_.value();
}

bool TouchpadSettings::IsNaturalScrollSet() const {
  return natural_scroll_.is_set();
}

void TouchpadSettings::SetThreeFingerClick(bool enabled) {
  three_finger_click_.Set(enabled);
}

bool TouchpadSettings::GetThreeFingerClick() const {
  return three_finger_click_.value();
}

bool TouchpadSettings::IsThreeFingerClickSet() const {
  return three_finger_click_.is_set();
}

void TouchpadSettings::SetTapDragging(bool enabled) {
  tap_dragging_.Set(enabled);
}

bool TouchpadSettings::GetTapDragging() const {
  return tap_dragging_.value();
}

bool TouchpadSettings::IsTapDraggingSet() const {
  return tap_dragging_.is_set();
}

bool TouchpadSettings::Update(const TouchpadSettings& settings) {
  bool updated = false;
  if (sensitivity_.Update(settings.sensitivity_))
    updated = true;
  if (tap_to_click_.Update(settings.tap_to_click_))
    updated = true;
  if (three_finger_click_.Update(settings.three_finger_click_))
    updated = true;
  if (tap_dragging_.Update(settings.tap_dragging_))
    updated = true;
  natural_scroll_.Update(settings.natural_scroll_);
  // Always send natural scrolling to the shell command, as a workaround.
  // See crbug.com/406480
  if (natural_scroll_.is_set())
    updated = true;
  return updated;
}

// static
void TouchpadSettings::Apply(const TouchpadSettings& touchpad_settings,
                             InputDeviceSettings* input_device_settings) {
  if (!input_device_settings)
    return;
  if (touchpad_settings.sensitivity_.is_set()) {
    input_device_settings->SetTouchpadSensitivity(
        touchpad_settings.sensitivity_.value());
  }
  if (touchpad_settings.tap_to_click_.is_set()) {
    input_device_settings->SetTapToClick(
        touchpad_settings.tap_to_click_.value());
  }
  if (touchpad_settings.three_finger_click_.is_set()) {
    input_device_settings->SetThreeFingerClick(
        touchpad_settings.three_finger_click_.value());
  }
  if (touchpad_settings.tap_dragging_.is_set()) {
    input_device_settings->SetTapDragging(
        touchpad_settings.tap_dragging_.value());
  }
  if (touchpad_settings.natural_scroll_.is_set()) {
    input_device_settings->SetNaturalScroll(
        touchpad_settings.natural_scroll_.value());
  }
}

MouseSettings::MouseSettings() {
}

MouseSettings::MouseSettings(const MouseSettings& other) = default;

MouseSettings& MouseSettings::operator=(const MouseSettings& other) {
  if (&other != this) {
    sensitivity_ = other.sensitivity_;
    primary_button_right_ = other.primary_button_right_;
  }
  return *this;
}

void MouseSettings::SetSensitivity(int value) {
  sensitivity_.Set(value);
}

int MouseSettings::GetSensitivity() const {
  return sensitivity_.value();
}

bool MouseSettings::IsSensitivitySet() const {
  return sensitivity_.is_set();
}

void MouseSettings::SetPrimaryButtonRight(bool right) {
  primary_button_right_.Set(right);
}

bool MouseSettings::GetPrimaryButtonRight() const {
  return primary_button_right_.value();
}

bool MouseSettings::IsPrimaryButtonRightSet() const {
  return primary_button_right_.is_set();
}

bool MouseSettings::Update(const MouseSettings& settings) {
  bool updated = false;
  if (sensitivity_.Update(settings.sensitivity_))
    updated = true;
  if (primary_button_right_.Update(settings.primary_button_right_))
    updated = true;
  return updated;
}

// static
void MouseSettings::Apply(const MouseSettings& mouse_settings,
                          InputDeviceSettings* input_device_settings) {
  if (!input_device_settings)
    return;
  if (mouse_settings.sensitivity_.is_set()) {
    input_device_settings->SetMouseSensitivity(
        mouse_settings.sensitivity_.value());
  }
  if (mouse_settings.primary_button_right_.is_set()) {
    input_device_settings->SetPrimaryButtonRight(
        mouse_settings.primary_button_right_.value());
  }
}

// static
bool InputDeviceSettings::ForceKeyboardDrivenUINavigation() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector)
    return false;

  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  if (!policy_manager)
    return false;

  if (policy_manager->IsRemoraRequisition() ||
      policy_manager->IsSharkRequisition()) {
    return true;
  }

  bool keyboard_driven = false;
  if (chromeos::system::StatisticsProvider::GetInstance()->GetMachineFlag(
          kOemKeyboardDrivenOobeKey, &keyboard_driven)) {
    return keyboard_driven;
  }

  return false;
}

// static
void InputDeviceSettings::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(::prefs::kTouchscreenEnabledLocal, true);
}

// static
void InputDeviceSettings::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(::prefs::kTouchscreenEnabled, true);
  registry->RegisterBooleanPref(::prefs::kTouchpadEnabled, true);
}

void InputDeviceSettings::UpdateTouchDevicesStatusFromPrefs() {
  UpdateTouchscreenStatusFromPrefs();

  PrefService* user_prefs = GetActiveProfilePrefs();
  if (!user_prefs)
    return;

  const bool touchpad_status =
      user_prefs->HasPrefPath(::prefs::kTouchpadEnabled)
          ? user_prefs->GetBoolean(::prefs::kTouchpadEnabled)
          : true;
  SetInternalTouchpadEnabled(touchpad_status);
}

bool InputDeviceSettings::IsTouchscreenEnabledInPrefs(
    bool use_local_state) const {
  if (use_local_state) {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);

    return local_state->HasPrefPath(::prefs::kTouchscreenEnabledLocal)
               ? local_state->GetBoolean(::prefs::kTouchscreenEnabledLocal)
               : true;
  } else {
    PrefService* user_prefs = GetActiveProfilePrefs();
    if (!user_prefs)
      return true;

    return user_prefs->HasPrefPath(::prefs::kTouchscreenEnabled)
               ? user_prefs->GetBoolean(::prefs::kTouchscreenEnabled)
               : true;
  }
}

void InputDeviceSettings::SetTouchscreenEnabledInPrefs(bool enabled,
                                                       bool use_local_state) {
  if (use_local_state) {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    local_state->SetBoolean(::prefs::kTouchscreenEnabledLocal, enabled);
  } else {
    PrefService* user_prefs = GetActiveProfilePrefs();
    if (!user_prefs)
      return;

    user_prefs->SetBoolean(::prefs::kTouchscreenEnabled, enabled);
  }
}

void InputDeviceSettings::UpdateTouchscreenStatusFromPrefs() {
  bool enabled_in_local_state = IsTouchscreenEnabledInPrefs(true);
  bool enabled_in_user_prefs = IsTouchscreenEnabledInPrefs(false);
  SetTouchscreensEnabled(enabled_in_local_state && enabled_in_user_prefs);
}

void InputDeviceSettings::ToggleTouchpad() {
  PrefService* user_prefs = GetActiveProfilePrefs();
  if (!user_prefs)
    return;

  const bool touchpad_status =
      user_prefs->HasPrefPath(::prefs::kTouchpadEnabled)
          ? user_prefs->GetBoolean(::prefs::kTouchpadEnabled)
          : true;

  user_prefs->SetBoolean(::prefs::kTouchpadEnabled, !touchpad_status);
  SetInternalTouchpadEnabled(!touchpad_status);
}

}  // namespace system
}  // namespace chromeos
