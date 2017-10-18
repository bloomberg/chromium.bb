// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller.h"

#include <memory>

#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/config.h"
#include "ash/session/session_controller.h"
#include "ash/session/session_observer.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/accessibility_manager.mojom.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/base/cursor/cursor_type.h"

using session_manager::SessionState;

namespace ash {
namespace {

void NotifyAccessibilityStatusChanged() {
  Shell::Get()->system_tray_notifier()->NotifyAccessibilityStatusChanged(
      A11Y_NOTIFICATION_NONE);
}

}  // namespace

AccessibilityController::AccessibilityController(
    service_manager::Connector* connector)
    : connector_(connector) {
  Shell::Get()->session_controller()->AddObserver(this);
}

AccessibilityController::~AccessibilityController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void AccessibilityController::RegisterProfilePrefs(PrefRegistrySimple* registry,
                                                   bool for_test) {
  if (for_test) {
    // In tests there is no remote pref service. Make ash own the prefs.
    registry->RegisterBooleanPref(prefs::kAccessibilityLargeCursorEnabled,
                                  false);
    registry->RegisterIntegerPref(prefs::kAccessibilityLargeCursorDipSize,
                                  kDefaultLargeCursorSize);
    registry->RegisterBooleanPref(prefs::kAccessibilityHighContrastEnabled,
                                  false);
    registry->RegisterBooleanPref(prefs::kAccessibilityScreenMagnifierEnabled,
                                  false);
    return;
  }

  // In production the prefs are owned by chrome.
  // TODO(jamescook): Move ownership to ash.
  registry->RegisterForeignPref(prefs::kAccessibilityLargeCursorEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityLargeCursorDipSize);
  registry->RegisterForeignPref(prefs::kAccessibilityHighContrastEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityScreenMagnifierEnabled);
}

void AccessibilityController::SetLargeCursorEnabled(bool enabled) {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, enabled);
  prefs->CommitPendingWrite();
}

bool AccessibilityController::IsLargeCursorEnabled() const {
  return large_cursor_enabled_;
}

void AccessibilityController::SetHighContrastEnabled(bool enabled) {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kAccessibilityHighContrastEnabled, enabled);
  prefs->CommitPendingWrite();
}

bool AccessibilityController::IsHighContrastEnabled() const {
  return high_contrast_enabled_;
}

void AccessibilityController::OnSigninScreenPrefServiceInitialized(
    PrefService* prefs) {
  ObservePrefs(prefs);
}

void AccessibilityController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  ObservePrefs(prefs);
}

void AccessibilityController::SetPrefServiceForTest(PrefService* prefs) {
  pref_service_for_test_ = prefs;
  ObservePrefs(prefs);
}

void AccessibilityController::ObservePrefs(PrefService* prefs) {
  // Watch for pref updates from webui settings and policy.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      prefs::kAccessibilityLargeCursorEnabled,
      base::Bind(&AccessibilityController::UpdateLargeCursorFromPref,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityLargeCursorDipSize,
      base::Bind(&AccessibilityController::UpdateLargeCursorFromPref,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityHighContrastEnabled,
      base::Bind(&AccessibilityController::UpdateHighContrastFromPref,
                 base::Unretained(this)));

  // Load current state.
  UpdateLargeCursorFromPref();
  UpdateHighContrastFromPref();
}

PrefService* AccessibilityController::GetActivePrefService() const {
  if (pref_service_for_test_)
    return pref_service_for_test_;
  return Shell::Get()->session_controller()->GetActivePrefService();
}

void AccessibilityController::UpdateLargeCursorFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled =
      prefs->GetBoolean(prefs::kAccessibilityLargeCursorEnabled);
  // Reset large cursor size to the default size when large cursor is disabled.
  if (!enabled)
    prefs->ClearPref(prefs::kAccessibilityLargeCursorDipSize);
  const int size = prefs->GetInteger(prefs::kAccessibilityLargeCursorDipSize);

  if (large_cursor_enabled_ == enabled && large_cursor_size_in_dip_ == size)
    return;

  large_cursor_enabled_ = enabled;
  large_cursor_size_in_dip_ = size;

  NotifyAccessibilityStatusChanged();

  ShellPort::Get()->SetCursorSize(
      large_cursor_enabled_ ? ui::CursorSize::kLarge : ui::CursorSize::kNormal);
  Shell::Get()->SetLargeCursorSizeInDip(large_cursor_size_in_dip_);
  Shell::Get()->UpdateCursorCompositingEnabled();
}

void AccessibilityController::UpdateHighContrastFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled =
      prefs->GetBoolean(prefs::kAccessibilityHighContrastEnabled);

  if (high_contrast_enabled_ == enabled)
    return;

  high_contrast_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();

  // Under mash the UI service (window server) handles high contrast mode.
  if (Shell::GetAshConfig() == Config::MASH) {
    if (!connector_)  // Null in tests.
      return;
    ui::mojom::AccessibilityManagerPtr accessibility_ptr;
    connector_->BindInterface(ui::mojom::kServiceName, &accessibility_ptr);
    accessibility_ptr->SetHighContrastMode(enabled);
    return;
  }

  // Under classic ash high contrast mode is handled internally.
  Shell::Get()->high_contrast_controller()->SetEnabled(enabled);
  Shell::Get()->UpdateCursorCompositingEnabled();
}

}  // namespace ash
