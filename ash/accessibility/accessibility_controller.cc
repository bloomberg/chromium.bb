// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/session/session_observer.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

void NotifyAccessibilityStatusChanged() {
  Shell::Get()->system_tray_notifier()->NotifyAccessibilityStatusChanged(
      A11Y_NOTIFICATION_NONE);
}

}  // namespace

AccessibilityController::AccessibilityController() {
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
    return;
  }

  // In production the prefs are owned by chrome.
  registry->RegisterForeignPref(prefs::kAccessibilityLargeCursorEnabled);
}

void AccessibilityController::SetLargeCursorEnabled(bool enabled) {
  PrefService* prefs = GetActivePrefService();
  // Null early in startup.
  if (!prefs)
    return;

  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, enabled);
  prefs->CommitPendingWrite();

  // Code in chrome responds to the pref change and sets the cursor size.
  // TODO(jamescook): Move the cursor size code from chrome into this class.
}

bool AccessibilityController::IsLargeCursorEnabled() const {
  PrefService* prefs = GetActivePrefService();
  // Null early in startup.
  if (!prefs)
    return false;

  return prefs->GetBoolean(prefs::kAccessibilityLargeCursorEnabled);
}

void AccessibilityController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  // Watch for pref updates from webui settings.
  pref_change_registrar_ = base::MakeUnique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(prefs::kAccessibilityLargeCursorEnabled,
                              base::Bind(&NotifyAccessibilityStatusChanged));

  // Update the system tray based on this user's prefs.
  NotifyAccessibilityStatusChanged();
}

void AccessibilityController::SetPrefServiceForTest(PrefService* prefs) {
  pref_service_for_test_ = prefs;
  OnActiveUserPrefServiceChanged(prefs);
}

PrefService* AccessibilityController::GetActivePrefService() const {
  if (pref_service_for_test_)
    return pref_service_for_test_;

  return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
}

}  // namespace ash
