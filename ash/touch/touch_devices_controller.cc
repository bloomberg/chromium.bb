// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_devices_controller.h"

#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/metrics/histogram_macros.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/ui/public/cpp/input_devices/input_device_controller_client.h"

namespace ash {

namespace {

void OnSetTouchpadEnabledDone(bool succeeded) {
  // Don't log here, |succeeded| is only true if there is a touchpad *and* the
  // value changed. In other words |succeeded| is false when not on device or
  // the value was already at the value specified. Neither of these are
  // interesting failures.
}

ui::InputDeviceControllerClient* GetInputDeviceControllerClient() {
  return Shell::Get()->shell_delegate()->GetInputDeviceControllerClient();
}

PrefService* GetActivePrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

}  // namespace

// static
void TouchDevicesController::RegisterProfilePrefs(PrefRegistrySimple* registry,
                                                  bool for_test) {
  if (for_test) {
    // In tests there is no remote pref service. Make ash own the prefs.
    registry->RegisterBooleanPref(
        prefs::kTapDraggingEnabled, false,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF |
            PrefRegistry::PUBLIC);
  } else {
    // In production the prefs are owned by chrome.
    // TODO: Move ownership to ash.
    registry->RegisterForeignPref(prefs::kTapDraggingEnabled);
  }

  registry->RegisterBooleanPref(prefs::kTouchpadEnabled, PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(prefs::kTouchscreenEnabled,
                                PrefRegistry::PUBLIC);
}

TouchDevicesController::TouchDevicesController() {
  Shell::Get()->session_controller()->AddObserver(this);
}

TouchDevicesController::~TouchDevicesController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void TouchDevicesController::ToggleTouchpad() {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  const bool touchpad_enabled = prefs->GetBoolean(prefs::kTouchpadEnabled);
  prefs->SetBoolean(prefs::kTouchpadEnabled, !touchpad_enabled);
}

bool TouchDevicesController::GetTouchscreenEnabled(
    TouchscreenEnabledSource source) const {
  if (source == TouchscreenEnabledSource::GLOBAL)
    return global_touchscreen_enabled_;

  PrefService* prefs = GetActivePrefService();
  return prefs && prefs->GetBoolean(prefs::kTouchscreenEnabled);
}

void TouchDevicesController::SetTouchscreenEnabled(
    bool enabled,
    TouchscreenEnabledSource source) {
  if (source == TouchscreenEnabledSource::GLOBAL) {
    global_touchscreen_enabled_ = enabled;
    // Explicitly call |UpdateTouchscreenEnabled()| to update the actual
    // touchscreen state from multiple sources.
    UpdateTouchscreenEnabled();
    return;
  }

  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kTouchscreenEnabled, enabled);
}

void TouchDevicesController::OnUserSessionAdded(const AccountId& account_id) {
  uma_record_callback_ = base::BindOnce([](PrefService* prefs) {
    UMA_HISTOGRAM_BOOLEAN("Touchpad.TapDragging.Started",
                          prefs->GetBoolean(prefs::kTapDraggingEnabled));
  });
}

void TouchDevicesController::OnSigninScreenPrefServiceInitialized(
    PrefService* prefs) {
  ObservePrefs(prefs);
}

void TouchDevicesController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  if (uma_record_callback_)
    std::move(uma_record_callback_).Run(prefs);
  ObservePrefs(prefs);
}

void TouchDevicesController::ObservePrefs(PrefService* prefs) {
  // Watch for pref updates.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      prefs::kTapDraggingEnabled,
      base::BindRepeating(&TouchDevicesController::UpdateTapDraggingEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kTouchpadEnabled,
      base::BindRepeating(&TouchDevicesController::UpdateTouchpadEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kTouchscreenEnabled,
      base::BindRepeating(&TouchDevicesController::UpdateTouchscreenEnabled,
                          base::Unretained(this)));
  // Load current state.
  UpdateTapDraggingEnabled();
  UpdateTouchpadEnabled();
  UpdateTouchscreenEnabled();
}

void TouchDevicesController::UpdateTapDraggingEnabled() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled = prefs->GetBoolean(prefs::kTapDraggingEnabled);

  if (tap_dragging_enabled_ == enabled)
    return;

  tap_dragging_enabled_ = enabled;

  UMA_HISTOGRAM_BOOLEAN("Touchpad.TapDragging.Changed", enabled);

  if (!GetInputDeviceControllerClient())
    return;  // Happens in tests.

  GetInputDeviceControllerClient()->SetTapDragging(enabled);
}

void TouchDevicesController::UpdateTouchpadEnabled() {
  if (!GetInputDeviceControllerClient())
    return;  // Happens in tests.

  PrefService* prefs = GetActivePrefService();

  GetInputDeviceControllerClient()->SetInternalTouchpadEnabled(
      prefs->GetBoolean(prefs::kTouchpadEnabled),
      base::BindRepeating(&OnSetTouchpadEnabledDone));
}

void TouchDevicesController::UpdateTouchscreenEnabled() {
  if (!GetInputDeviceControllerClient())
    return;  // Happens in tests.

  GetInputDeviceControllerClient()->SetTouchscreensEnabled(
      GetTouchscreenEnabled(TouchscreenEnabledSource::GLOBAL) &&
      GetTouchscreenEnabled(TouchscreenEnabledSource::USER_PREF));
}

}  // namespace ash
