// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller.h"

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_highlight_controller.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/accessibility/accessibility_panel_layout_manager.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/components/autoclick/public/mojom/autoclick.mojom.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller.h"
#include "ash/session/session_observer.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/scoped_backlights_forced_off.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "mash/public/mojom/launchable.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/accessibility_manager.mojom.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor_type.h"
#include "ui/keyboard/keyboard_util.h"

using session_manager::SessionState;

namespace ash {
namespace {

PrefService* GetActivePrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

}  // namespace

AccessibilityController::AccessibilityController(
    service_manager::Connector* connector)
    : connector_(connector),
      autoclick_delay_(AutoclickController::GetDefaultAutoclickDelay()) {
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
    registry->RegisterIntegerPref(
        prefs::kAccessibilityAutoclickDelayMs,
        static_cast<int>(
            AutoclickController::GetDefaultAutoclickDelay().InMilliseconds()));
    registry->RegisterBooleanPref(prefs::kAccessibilityCaretHighlightEnabled,
                                  false);
    registry->RegisterBooleanPref(prefs::kAccessibilityCursorHighlightEnabled,
                                  false);
    registry->RegisterBooleanPref(prefs::kAccessibilityFocusHighlightEnabled,
                                  false);
    registry->RegisterBooleanPref(prefs::kAccessibilityHighContrastEnabled,
                                  false);
    registry->RegisterBooleanPref(prefs::kAccessibilityLargeCursorEnabled,
                                  false);
    registry->RegisterIntegerPref(prefs::kAccessibilityLargeCursorDipSize,
                                  kDefaultLargeCursorSize);
    registry->RegisterBooleanPref(prefs::kAccessibilityMonoAudioEnabled, false);
    registry->RegisterBooleanPref(prefs::kAccessibilityScreenMagnifierEnabled,
                                  false);
    registry->RegisterDoublePref(prefs::kAccessibilityScreenMagnifierScale,
                                 1.0);
    registry->RegisterBooleanPref(prefs::kAccessibilitySpokenFeedbackEnabled,
                                  false);
    registry->RegisterBooleanPref(prefs::kAccessibilitySelectToSpeakEnabled,
                                  false);
    registry->RegisterBooleanPref(prefs::kAccessibilityStickyKeysEnabled,
                                  false);
    registry->RegisterBooleanPref(prefs::kAccessibilityVirtualKeyboardEnabled,
                                  false);
    return;
  }

  // In production the prefs are owned by chrome.
  // TODO(jamescook): Move ownership to ash.
  registry->RegisterForeignPref(prefs::kAccessibilityAutoclickEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityAutoclickDelayMs);
  registry->RegisterForeignPref(prefs::kAccessibilityCaretHighlightEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityCursorHighlightEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityFocusHighlightEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityHighContrastEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityLargeCursorEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityLargeCursorDipSize);
  registry->RegisterForeignPref(prefs::kAccessibilityMonoAudioEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityScreenMagnifierEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityScreenMagnifierScale);
  registry->RegisterForeignPref(prefs::kAccessibilitySpokenFeedbackEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilitySelectToSpeakEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityStickyKeysEnabled);
  registry->RegisterForeignPref(prefs::kAccessibilityVirtualKeyboardEnabled);
}

void AccessibilityController::AddObserver(AccessibilityObserver* observer) {
  observers_.AddObserver(observer);
}

void AccessibilityController::RemoveObserver(AccessibilityObserver* observer) {
  observers_.RemoveObserver(observer);
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

void AccessibilityController::SetCaretHighlightEnabled(bool enabled) {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kAccessibilityCaretHighlightEnabled, enabled);
  prefs->CommitPendingWrite();
}

bool AccessibilityController::IsCaretHighlightEnabled() const {
  return caret_highlight_enabled_;
}

void AccessibilityController::SetCursorHighlightEnabled(bool enabled) {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kAccessibilityCursorHighlightEnabled, enabled);
  prefs->CommitPendingWrite();
}

bool AccessibilityController::IsCursorHighlightEnabled() const {
  return cursor_highlight_enabled_;
}

void AccessibilityController::SetFocusHighlightEnabled(bool enabled) {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kAccessibilityFocusHighlightEnabled, enabled);
  prefs->CommitPendingWrite();
}

bool AccessibilityController::IsFocusHighlightEnabled() const {
  return focus_highlight_enabled_;
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

void AccessibilityController::SetSelectToSpeakEnabled(bool enabled) {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kAccessibilitySelectToSpeakEnabled, enabled);
  prefs->CommitPendingWrite();
}

bool AccessibilityController::IsSelectToSpeakEnabled() const {
  return select_to_speak_enabled_;
}

void AccessibilityController::SetStickyKeysEnabled(bool enabled) {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kAccessibilityStickyKeysEnabled, enabled);
  prefs->CommitPendingWrite();
}

bool AccessibilityController::IsStickyKeysEnabled() const {
  return sticky_keys_enabled_;
}

void AccessibilityController::SetVirtualKeyboardEnabled(bool enabled) {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled, enabled);
  prefs->CommitPendingWrite();
}

bool AccessibilityController::IsVirtualKeyboardEnabled() const {
  return virtual_keyboard_enabled_;
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
    ax::mojom::Gesture gesture) {
  if (client_)
    client_->HandleAccessibilityGesture(gesture);
}

void AccessibilityController::ToggleDictation() {
  if (client_)
    client_->ToggleDictation();
}

void AccessibilityController::SilenceSpokenFeedback() {
  if (client_)
    client_->SilenceSpokenFeedback();
}

void AccessibilityController::OnTwoFingerTouchStart() {
  if (client_)
    client_->OnTwoFingerTouchStart();
}

void AccessibilityController::OnTwoFingerTouchStop() {
  if (client_)
    client_->OnTwoFingerTouchStop();
}

void AccessibilityController::ShouldToggleSpokenFeedbackViaTouch(
    base::OnceCallback<void(bool)> callback) {
  if (client_)
    client_->ShouldToggleSpokenFeedbackViaTouch(std::move(callback));
}

void AccessibilityController::PlaySpokenFeedbackToggleCountdown(
    int tick_count) {
  if (client_)
    client_->PlaySpokenFeedbackToggleCountdown(tick_count);
}

void AccessibilityController::NotifyAccessibilityStatusChanged() {
  for (auto& observer : observers_)
    observer.OnAccessibilityStatusChanged();
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

void AccessibilityController::BrailleDisplayStateChanged(bool connected) {
  braille_display_connected_ = connected;
  if (braille_display_connected_)
    SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_SHOW);
  NotifyAccessibilityStatusChanged();
  NotifyShowAccessibilityNotification();
}

void AccessibilityController::SetFocusHighlightRect(
    const gfx::Rect& bounds_in_screen) {
  if (!accessibility_highlight_controller_)
    return;
  accessibility_highlight_controller_->SetFocusHighlightRect(bounds_in_screen);
}

void AccessibilityController::SetAccessibilityPanelFullscreen(bool fullscreen) {
  // The accessibility panel is only shown on the primary display.
  aura::Window* root = Shell::GetPrimaryRootWindow();
  aura::Window* container =
      Shell::GetContainer(root, kShellWindowId_AccessibilityPanelContainer);
  // TODO(jamescook): Avoid this cast by moving ash::AccessibilityObserver
  // ownership to this class and notifying it on ChromeVox fullscreen updates.
  AccessibilityPanelLayoutManager* layout =
      static_cast<AccessibilityPanelLayoutManager*>(
          container->layout_manager());
  layout->SetPanelFullscreen(fullscreen);
}

void AccessibilityController::OnSigninScreenPrefServiceInitialized(
    PrefService* prefs) {
  ObservePrefs(prefs);
}

void AccessibilityController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
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
      base::BindRepeating(&AccessibilityController::UpdateAutoclickFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickDelayMs,
      base::BindRepeating(
          &AccessibilityController::UpdateAutoclickDelayFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityCaretHighlightEnabled,
      base::BindRepeating(
          &AccessibilityController::UpdateCaretHighlightFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityCursorHighlightEnabled,
      base::BindRepeating(
          &AccessibilityController::UpdateCursorHighlightFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityFocusHighlightEnabled,
      base::BindRepeating(
          &AccessibilityController::UpdateFocusHighlightFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityHighContrastEnabled,
      base::BindRepeating(&AccessibilityController::UpdateHighContrastFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityLargeCursorEnabled,
      base::BindRepeating(&AccessibilityController::UpdateLargeCursorFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityLargeCursorDipSize,
      base::BindRepeating(&AccessibilityController::UpdateLargeCursorFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityMonoAudioEnabled,
      base::BindRepeating(&AccessibilityController::UpdateMonoAudioFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySpokenFeedbackEnabled,
      base::BindRepeating(
          &AccessibilityController::UpdateSpokenFeedbackFromPref,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilitySelectToSpeakEnabled,
      base::BindRepeating(&AccessibilityController::UpdateSelectToSpeakFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityStickyKeysEnabled,
      base::BindRepeating(&AccessibilityController::UpdateStickyKeysFromPref,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityVirtualKeyboardEnabled,
      base::BindRepeating(
          &AccessibilityController::UpdateVirtualKeyboardFromPref,
          base::Unretained(this)));

  // Load current state.
  UpdateAutoclickFromPref();
  UpdateAutoclickDelayFromPref();
  UpdateCaretHighlightFromPref();
  UpdateCursorHighlightFromPref();
  UpdateFocusHighlightFromPref();
  UpdateHighContrastFromPref();
  UpdateLargeCursorFromPref();
  UpdateMonoAudioFromPref();
  UpdateSpokenFeedbackFromPref();
  UpdateSelectToSpeakFromPref();
  UpdateStickyKeysFromPref();
  UpdateVirtualKeyboardFromPref();
}

void AccessibilityController::UpdateAutoclickFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled = prefs->GetBoolean(prefs::kAccessibilityAutoclickEnabled);

  if (autoclick_enabled_ == enabled)
    return;

  autoclick_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();

  if (Shell::GetAshConfig() == Config::MASH) {
    if (!connector_)  // Null in tests.
      return;
    mash::mojom::LaunchablePtr launchable;
    connector_->BindInterface("autoclick_app", &launchable);
    launchable->Launch(mash::mojom::kWindow, mash::mojom::LaunchMode::DEFAULT);
    return;
  }

  Shell::Get()->autoclick_controller()->SetEnabled(enabled);
}

void AccessibilityController::UpdateAutoclickDelayFromPref() {
  PrefService* prefs = GetActivePrefService();
  base::TimeDelta autoclick_delay = base::TimeDelta::FromMilliseconds(
      int64_t{prefs->GetInteger(prefs::kAccessibilityAutoclickDelayMs)});

  if (autoclick_delay_ == autoclick_delay)
    return;
  autoclick_delay_ = autoclick_delay;

  if (Shell::GetAshConfig() == Config::MASH) {
    if (!connector_)  // Null in tests.
      return;
    autoclick::mojom::AutoclickControllerPtr autoclick_controller;
    connector_->BindInterface("autoclick_app", &autoclick_controller);
    autoclick_controller->SetAutoclickDelay(autoclick_delay_.InMilliseconds());
    return;
  }

  Shell::Get()->autoclick_controller()->SetAutoclickDelay(autoclick_delay_);
}

void AccessibilityController::UpdateCaretHighlightFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled =
      prefs->GetBoolean(prefs::kAccessibilityCaretHighlightEnabled);

  if (caret_highlight_enabled_ == enabled)
    return;

  caret_highlight_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
  UpdateAccessibilityHighlightingFromPrefs();
}

void AccessibilityController::UpdateCursorHighlightFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled =
      prefs->GetBoolean(prefs::kAccessibilityCursorHighlightEnabled);

  if (cursor_highlight_enabled_ == enabled)
    return;

  cursor_highlight_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
  UpdateAccessibilityHighlightingFromPrefs();
}

void AccessibilityController::UpdateFocusHighlightFromPref() {
  PrefService* prefs = GetActivePrefService();
  bool enabled = prefs->GetBoolean(prefs::kAccessibilityFocusHighlightEnabled);

  // Focus highlighting can't be on when spoken feedback is on, because
  // ChromeVox does its own focus highlighting.
  if (spoken_feedback_enabled_)
    enabled = false;

  if (focus_highlight_enabled_ == enabled)
    return;

  focus_highlight_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
  UpdateAccessibilityHighlightingFromPrefs();
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

void AccessibilityController::UpdateMonoAudioFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled = prefs->GetBoolean(prefs::kAccessibilityMonoAudioEnabled);

  if (mono_audio_enabled_ == enabled)
    return;

  mono_audio_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
  chromeos::CrasAudioHandler::Get()->SetOutputMonoEnabled(enabled);
}

void AccessibilityController::UpdateSpokenFeedbackFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled =
      prefs->GetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled);

  if (spoken_feedback_enabled_ == enabled)
    return;

  spoken_feedback_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
  if (spoken_feedback_notification_ != A11Y_NOTIFICATION_NONE)
    NotifyShowAccessibilityNotification();

  // TODO(warx): ChromeVox loading/unloading requires browser process started,
  // thus it is still handled on Chrome side.

  // ChromeVox focus highlighting overrides the other focus highlighting.
  UpdateFocusHighlightFromPref();
}

void AccessibilityController::UpdateAccessibilityHighlightingFromPrefs() {
  if (!caret_highlight_enabled_ && !cursor_highlight_enabled_ &&
      !focus_highlight_enabled_) {
    accessibility_highlight_controller_.reset();
    return;
  }

  if (!accessibility_highlight_controller_) {
    accessibility_highlight_controller_ =
        std::make_unique<AccessibilityHighlightController>();
  }

  accessibility_highlight_controller_->HighlightCaret(caret_highlight_enabled_);
  accessibility_highlight_controller_->HighlightCursor(
      cursor_highlight_enabled_);
  accessibility_highlight_controller_->HighlightFocus(focus_highlight_enabled_);
}

void AccessibilityController::UpdateSelectToSpeakFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled =
      prefs->GetBoolean(prefs::kAccessibilitySelectToSpeakEnabled);

  if (select_to_speak_enabled_ == enabled)
    return;

  select_to_speak_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();
}

void AccessibilityController::UpdateStickyKeysFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled =
      prefs->GetBoolean(prefs::kAccessibilityStickyKeysEnabled);

  if (sticky_keys_enabled_ == enabled)
    return;

  sticky_keys_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();

  Shell::Get()->sticky_keys_controller()->Enable(enabled);
}

void AccessibilityController::UpdateVirtualKeyboardFromPref() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled =
      prefs->GetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled);

  if (virtual_keyboard_enabled_ == enabled)
    return;

  virtual_keyboard_enabled_ = enabled;

  NotifyAccessibilityStatusChanged();

  keyboard::SetAccessibilityKeyboardEnabled(enabled);

  if (Shell::GetAshConfig() == Config::MASH) {
    // TODO(mash): Support on-screen keyboard. See https://crbug.com/646565.
    NOTIMPLEMENTED();
    return;
  }

  // Note that there are two versions of the on-screen keyboard. A full layout
  // is provided for accessibility, which includes sticky modifier keys to
  // enable typing of hotkeys. A compact version is used in tablet mode to
  // provide a layout with larger keys to facilitate touch typing. In the event
  // that the a11y keyboard is being disabled, an on-screen keyboard might still
  // be enabled and a forced reset is required to pick up the layout change.
  if (keyboard::IsKeyboardEnabled())
    Shell::Get()->CreateKeyboard();
  else
    Shell::Get()->DestroyKeyboard();
}

void AccessibilityController::NotifyShowAccessibilityNotification() {
  for (auto& observer : observers_)
    observer.ShowAccessibilityNotification();
}

}  // namespace ash
