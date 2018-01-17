// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller.h"

#include <memory>
#include <utility>

#include "ash/autoclick/autoclick_controller.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/config.h"
#include "ash/session/session_controller.h"
#include "ash/session/session_observer.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/scoped_backlights_forced_off.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/accessibility_manager.mojom.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/base/cursor/cursor_type.h"

using session_manager::SessionState;

namespace ash {
namespace {

void NotifyAccessibilityStatusChanged(
    AccessibilityNotificationVisibility notification_visibility) {
  Shell::Get()->system_tray_notifier()->NotifyAccessibilityStatusChanged(
      notification_visibility);
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
    registry->RegisterBooleanPref(prefs::kAccessibilityAutoclickEnabled, false);
    registry->RegisterBooleanPref(prefs::kAccessibilityHighContrastEnabled,
                                  false);
    registry->RegisterBooleanPref(prefs::kAccessibilityLargeCursorEnabled,
                                  false);
    registry->RegisterIntegerPref(prefs::kAccessibilityLargeCursorDipSize,
                                  kDefaultLargeCursorSize);
    registry->RegisterBooleanPref(prefs::kAccessibilityMonoAudioEnabled, false);
    registry->RegisterBooleanPref(prefs::kAccessibilityScreenMagnifierEnabled,
                                  false);
    registry->RegisterBooleanPref(prefs::kAccessibilitySpokenFeedbackEnabled,
                                  false);
    return;
  }

  // In production the prefs are owned by chrome.
  // TODO(jamescook): Move ownership to ash.
  registry->RegisterForeignPref(prefs::kAccessibilityAutoclickEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityHighContrastEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityLargeCursorEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityLargeCursorDipSize);
  registry->RegisterForeignPref(prefs::kAccessibilityMonoAudioEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityScreenMagnifierEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilitySpokenFeedbackEnabled);
}

void AccessibilityController::BindRequest(
    mojom::AccessibilityControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AccessibilityController::SetAutoclickEnabled(bool enabled) {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kAccessibilityAutoclickEnabled, enabled);
  prefs->CommitPendingWrite();
}

bool AccessibilityController::IsAutoclickEnabled() const {
  return autoclick_enabled_;
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

void AccessibilityController::SetMonoAudioEnabled(bool enabled) {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kAccessibilityMonoAudioEnabled, enabled);
  prefs->CommitPendingWrite();
}

bool AccessibilityController::IsMonoAudioEnabled() const {
  return mono_audio_enabled_;
}

void AccessibilityController::SetSpokenFeedbackEnabled(
    bool enabled,
    AccessibilityNotificationVisibility notify) {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  spoken_feedback_notification_ = notify;
  prefs->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled, enabled);
  prefs->CommitPendingWrite();
}

bool AccessibilityController::IsSpokenFeedbackEnabled() const {
  return spoken_feedback_enabled_;
}

void AccessibilityController::TriggerAccessibilityAlert(
    mojom::AccessibilityAlert alert) {
  if (client_)
    client_->TriggerAccessibilityAlert(alert);
}

void AccessibilityController::PlayEarcon(int32_t sound_key) {
  if (client_)
    client_->PlayEarcon(sound_key);
}

void AccessibilityController::PlayShutdownSound(
    base::OnceCallback<void(base::TimeDelta)> callback) {
  if (client_)
    client_->PlayShutdownSound(std::move(callback));
}

void AccessibilityController::HandleAccessibilityGesture(
    ui::AXGesture gesture) {
  if (client_) {
    const std::string gesture_str(ui::ToString(gesture));
    DCHECK(!gesture_str.empty() || gesture == ui::AX_GESTURE_NONE);
    client_->HandleAccessibilityGesture(gesture_str);
  }
}

void AccessibilityController::ToggleDictation() {
  if (client_)
    client_->ToggleDictation();
}

void AccessibilityController::SetClient(
    mojom::AccessibilityControllerClientPtr client) {
  client_ = std::move(client);
}

void AccessibilityController::SetDarkenScreen(bool darken) {
  if (darken && !scoped_backlights_forced_off_) {
    scoped_backlights_forced_off_ =
        Shell::Get()->backlights_forced_off_setter()->ForceBacklightsOff();
  } else if (!darken && scoped_backlights_forced_off_) {
    scoped_backlights_forced_off_.reset();
  }
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

void AccessibilityController::FlushMojoForTest() {
  client_.FlushForTesting();
}

void AccessibilityController::ObservePrefs(PrefService* prefs) {
  // Watch for pref updates from webui settings and policy.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickEnabled,
      base::Bind(&AccessibilityController::UpdateAutoclickFromPref,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityHighContrastEnabled,
      base::Bind(&AccessibilityController::UpdateHighContrastFromPref,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityLargeCursorEnabled,
      base::Bind(&AccessibilityController::UpdateLargeCursorFromPref,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityLargeCursorDipSize,
      base::Bind(&AccessibilityController::UpdateLargeCursorFromPref,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityMonoAudioEnabled,
      base::Bind(&AccessibilityController::UpdateMonoAudioFromPref,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySpokenFeedbackEnabled,
      base::Bind(&AccessibilityController::UpdateSpokenFeedbackFromPref,
                 base::Unretained(this)));

  // Load current state.
  UpdateAutoclickFromPref();
  UpdateHighContrastFromPref();
  UpdateLargeCursorFromPref();
  UpdateMonoAudioFromPref();
  UpdateSpokenFeedbackFromPref();
}

PrefService* AccessibilityController::GetActivePrefService() const {
  if (pref_service_for_test_)
    return pref_service_for_test_;
  return Shell::Get()->session_controller()->GetActivePrefService();
}

void AccessibilityController::UpdateAutoclickFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled = prefs->GetBoolean(prefs::kAccessibilityAutoclickEnabled);

  if (autoclick_enabled_ == enabled)
    return;

  autoclick_enabled_ = enabled;

  NotifyAccessibilityStatusChanged(A11Y_NOTIFICATION_NONE);

  if (Shell::GetAshConfig() == Config::MASH) {
    if (!connector_)  // Null in tests.
      return;
    mash::mojom::LaunchablePtr launchable;
    connector_->BindInterface("accessibility_autoclick", &launchable);
    launchable->Launch(mash::mojom::kWindow, mash::mojom::LaunchMode::DEFAULT);
    return;
  }

  Shell::Get()->autoclick_controller()->SetEnabled(enabled);
}

void AccessibilityController::UpdateHighContrastFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled =
      prefs->GetBoolean(prefs::kAccessibilityHighContrastEnabled);

  if (high_contrast_enabled_ == enabled)
    return;

  high_contrast_enabled_ = enabled;

  NotifyAccessibilityStatusChanged(A11Y_NOTIFICATION_NONE);

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

  NotifyAccessibilityStatusChanged(A11Y_NOTIFICATION_NONE);

  ShellPort::Get()->SetCursorSize(
      large_cursor_enabled_ ? ui::CursorSize::kLarge : ui::CursorSize::kNormal);
  Shell::Get()->SetLargeCursorSizeInDip(large_cursor_size_in_dip_);
  Shell::Get()->UpdateCursorCompositingEnabled();
}

void AccessibilityController::UpdateMonoAudioFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled = prefs->GetBoolean(prefs::kAccessibilityMonoAudioEnabled);

  if (mono_audio_enabled_ == enabled)
    return;

  mono_audio_enabled_ = enabled;

  NotifyAccessibilityStatusChanged(A11Y_NOTIFICATION_NONE);
  chromeos::CrasAudioHandler::Get()->SetOutputMonoEnabled(enabled);
}

void AccessibilityController::UpdateSpokenFeedbackFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled =
      prefs->GetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled);

  if (spoken_feedback_enabled_ == enabled)
    return;

  spoken_feedback_enabled_ = enabled;

  NotifyAccessibilityStatusChanged(spoken_feedback_notification_);
  // TODO(warx): Chrome observes prefs change and turns on/off spoken feedback.
  // Define a mojo call to control toggling spoken feedback (ChromeVox) once
  // prefs ownership and registration is moved to ash.
}

}  // namespace ash
