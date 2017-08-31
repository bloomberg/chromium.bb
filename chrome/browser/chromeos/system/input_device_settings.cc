// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/input_device_settings.h"

#include "ash/public/cpp/touchscreen_enabled_source.h"
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

// Sets |to_set| to |other| if |other| has a value and the value is not equal to
// |to_set|. This differs from *to_set = other; in so far as nothing is changed
// if |other| has no value. Returns true if |to_set| was updated.
template <typename T>
bool UpdateIfHasValue(const base::Optional<T>& other,
                      base::Optional<T>* to_set) {
  if (!other.has_value() || other == *to_set)
    return false;
  *to_set = other;
  return true;
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
  sensitivity_ = value;
}

int TouchpadSettings::GetSensitivity() const {
  return *sensitivity_;
}

bool TouchpadSettings::IsSensitivitySet() const {
  return sensitivity_.has_value();
}

void TouchpadSettings::SetTapToClick(bool enabled) {
  tap_to_click_ = enabled;
}

bool TouchpadSettings::GetTapToClick() const {
  return *tap_to_click_;
}

bool TouchpadSettings::IsTapToClickSet() const {
  return tap_to_click_.has_value();
}

void TouchpadSettings::SetNaturalScroll(bool enabled) {
  natural_scroll_ = enabled;
}

bool TouchpadSettings::GetNaturalScroll() const {
  return *natural_scroll_;
}

bool TouchpadSettings::IsNaturalScrollSet() const {
  return natural_scroll_.has_value();
}

void TouchpadSettings::SetThreeFingerClick(bool enabled) {
  three_finger_click_ = enabled;
}

bool TouchpadSettings::GetThreeFingerClick() const {
  return *three_finger_click_;
}

bool TouchpadSettings::IsThreeFingerClickSet() const {
  return three_finger_click_.has_value();
}

void TouchpadSettings::SetTapDragging(bool enabled) {
  tap_dragging_ = enabled;
}

bool TouchpadSettings::GetTapDragging() const {
  return *tap_dragging_;
}

bool TouchpadSettings::IsTapDraggingSet() const {
  return tap_dragging_.has_value();
}

bool TouchpadSettings::Update(const TouchpadSettings& settings) {
  bool updated = false;
  if (UpdateIfHasValue(settings.sensitivity_, &sensitivity_))
    updated = true;
  if (UpdateIfHasValue(settings.tap_to_click_, &tap_to_click_))
    updated = true;
  if (UpdateIfHasValue(settings.three_finger_click_, &three_finger_click_))
    updated = true;
  if (UpdateIfHasValue(settings.tap_dragging_, &tap_dragging_))
    updated = true;
  UpdateIfHasValue(settings.natural_scroll_, &natural_scroll_);
  // Always send natural scrolling to the shell command, as a workaround.
  // See crbug.com/406480
  if (natural_scroll_.has_value())
    updated = true;
  return updated;
}

// static
void TouchpadSettings::Apply(const TouchpadSettings& touchpad_settings,
                             InputDeviceSettings* input_device_settings) {
  if (!input_device_settings)
    return;
  if (touchpad_settings.sensitivity_.has_value()) {
    input_device_settings->SetTouchpadSensitivity(
        touchpad_settings.sensitivity_.value());
  }
  if (touchpad_settings.tap_to_click_.has_value()) {
    input_device_settings->SetTapToClick(
        touchpad_settings.tap_to_click_.value());
  }
  if (touchpad_settings.three_finger_click_.has_value()) {
    input_device_settings->SetThreeFingerClick(
        touchpad_settings.three_finger_click_.value());
  }
  if (touchpad_settings.tap_dragging_.has_value()) {
    input_device_settings->SetTapDragging(
        touchpad_settings.tap_dragging_.value());
  }
  if (touchpad_settings.natural_scroll_.has_value()) {
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
    reverse_scroll_ = other.reverse_scroll_;
  }
  return *this;
}

void MouseSettings::SetSensitivity(int value) {
  sensitivity_ = value;
}

int MouseSettings::GetSensitivity() const {
  return *sensitivity_;
}

bool MouseSettings::IsSensitivitySet() const {
  return sensitivity_.has_value();
}

void MouseSettings::SetPrimaryButtonRight(bool right) {
  primary_button_right_ = right;
}

bool MouseSettings::GetPrimaryButtonRight() const {
  return primary_button_right_.value();
}

bool MouseSettings::IsPrimaryButtonRightSet() const {
  return primary_button_right_.has_value();
}

void MouseSettings::SetReverseScroll(bool enabled) {
  reverse_scroll_ = enabled;
}

bool MouseSettings::GetReverseScroll() const {
  return *reverse_scroll_;
}

bool MouseSettings::IsReverseScrollSet() const {
  return reverse_scroll_.has_value();
}

bool MouseSettings::Update(const MouseSettings& settings) {
  bool updated = false;
  if (UpdateIfHasValue(settings.sensitivity_, &sensitivity_))
    updated = true;
  if (UpdateIfHasValue(settings.primary_button_right_,
                       &primary_button_right_)) {
    updated = true;
  }
  if (UpdateIfHasValue(settings.reverse_scroll_, &reverse_scroll_)) {
    updated = true;
  }
  return updated;
}

// static
void MouseSettings::Apply(const MouseSettings& mouse_settings,
                          InputDeviceSettings* input_device_settings) {
  if (!input_device_settings)
    return;
  if (mouse_settings.sensitivity_.has_value()) {
    input_device_settings->SetMouseSensitivity(
        mouse_settings.sensitivity_.value());
  }
  if (mouse_settings.primary_button_right_.has_value()) {
    input_device_settings->SetPrimaryButtonRight(
        mouse_settings.primary_button_right_.value());
  }
  if (mouse_settings.reverse_scroll_.has_value()) {
    input_device_settings->SetMouseReverseScroll(
        mouse_settings.reverse_scroll_.value());
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
void InputDeviceSettings::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(::prefs::kTouchscreenEnabled, true);
  registry->RegisterBooleanPref(::prefs::kTouchpadEnabled, true);
}

void InputDeviceSettings::UpdateTouchDevicesStatusFromPrefs() {
  UpdateTouchscreenEnabled();

  PrefService* pref_service = GetActiveProfilePrefs();
  if (!pref_service)
    return;

  const bool touchpad_status =
      pref_service->HasPrefPath(::prefs::kTouchpadEnabled)
          ? pref_service->GetBoolean(::prefs::kTouchpadEnabled)
          : true;
  SetInternalTouchpadEnabled(touchpad_status);
}

bool InputDeviceSettings::GetTouchscreenEnabled(
    ash::TouchscreenEnabledSource source) const {
  if (source == ash::TouchscreenEnabledSource::GLOBAL)
    return global_touchscreen_enabled_;

  PrefService* pref_service = GetActiveProfilePrefs();
  if (!pref_service)
    return true;

  return pref_service->HasPrefPath(::prefs::kTouchscreenEnabled)
             ? pref_service->GetBoolean(::prefs::kTouchscreenEnabled)
             : true;
}

void InputDeviceSettings::SetTouchscreenEnabled(
    bool enabled,
    ash::TouchscreenEnabledSource source) {
  if (source == ash::TouchscreenEnabledSource::GLOBAL) {
    global_touchscreen_enabled_ = enabled;
  } else {
    PrefService* pref_service = GetActiveProfilePrefs();
    if (pref_service)
      pref_service->SetBoolean(::prefs::kTouchscreenEnabled, enabled);
  }

  UpdateTouchscreenEnabled();
}

void InputDeviceSettings::ToggleTouchpad() {
  PrefService* pref_service = GetActiveProfilePrefs();
  if (!pref_service)
    return;

  const bool touchpad_status =
      pref_service->HasPrefPath(::prefs::kTouchpadEnabled)
          ? pref_service->GetBoolean(::prefs::kTouchpadEnabled)
          : true;

  pref_service->SetBoolean(::prefs::kTouchpadEnabled, !touchpad_status);
  SetInternalTouchpadEnabled(!touchpad_status);
}

void InputDeviceSettings::UpdateTouchscreenEnabled() {
  SetTouchscreensEnabled(
      GetTouchscreenEnabled(ash::TouchscreenEnabledSource::GLOBAL) &&
      GetTouchscreenEnabled(ash::TouchscreenEnabledSource::USER_PREF));
}

}  // namespace system
}  // namespace chromeos
