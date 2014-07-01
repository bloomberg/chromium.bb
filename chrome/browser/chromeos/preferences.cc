// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/preferences.h"

#include <vector>

#include "ash/autoclick/autoclick_controller.h"
#include "ash/magnifier/magnifier_constants.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/users/user.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/ime_keyboard.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/system/statistics_provider.h"
#include "components/feedback/tracing_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "url/gurl.h"

namespace chromeos {

static const char kFallbackInputMethodLocale[] = "en-US";

Preferences::Preferences()
    : prefs_(NULL),
      input_method_manager_(input_method::InputMethodManager::Get()),
      user_(NULL),
      user_is_primary_(false) {
  // Do not observe shell, if there is no shell instance; e.g., in some unit
  // tests.
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->AddShellObserver(this);
}

Preferences::Preferences(input_method::InputMethodManager* input_method_manager)
    : prefs_(NULL),
      input_method_manager_(input_method_manager),
      user_(NULL),
      user_is_primary_(false) {
  // Do not observe shell, if there is no shell instance; e.g., in some unit
  // tests.
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->AddShellObserver(this);
}

Preferences::~Preferences() {
  prefs_->RemoveObserver(this);
  UserManager::Get()->RemoveSessionStateObserver(this);
  // If shell instance is destoryed before this preferences instance, there is
  // no need to remove this shell observer.
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->RemoveShellObserver(this);
}

// static
void Preferences::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kOwnerPrimaryMouseButtonRight, false);
  registry->RegisterBooleanPref(prefs::kOwnerTapToClickEnabled, true);
  registry->RegisterBooleanPref(prefs::kAccessibilityVirtualKeyboardEnabled,
                                false);
  registry->RegisterStringPref(prefs::kLogoutStartedLast, std::string());
}

// static
void Preferences::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  std::string hardware_keyboard_id;
  // TODO(yusukes): Remove the runtime hack.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    DCHECK(g_browser_process);
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    hardware_keyboard_id =
        local_state->GetString(prefs::kHardwareKeyboardLayout);
  } else {
    hardware_keyboard_id = "xkb:us::eng";  // only for testing.
  }

  registry->RegisterBooleanPref(
      prefs::kPerformanceTracingEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kTapToClickEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kTapDraggingEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kEnableTouchpadThreeFingerClick,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kNaturalScroll,
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNaturalScrollDefault),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kPrimaryMouseButtonRight,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kLabsMediaplayerEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kLabsAdvancedFilesystemEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityStickyKeysEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityLargeCursorEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilitySpokenFeedbackEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityHighContrastEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityScreenMagnifierEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityScreenMagnifierType,
      ash::kDefaultMagnifierType,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kAccessibilityScreenMagnifierScale,
      std::numeric_limits<double>::min(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityAutoclickEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityAutoclickDelayMs,
      ash::AutoclickController::kDefaultAutoclickDelayMs,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityVirtualKeyboardEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kShouldAlwaysShowAccessibilityMenu,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kMouseSensitivity,
      3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kTouchpadSensitivity,
      3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kUse24HourClock,
      base::GetHourClockType() == base::k24HourClock,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDisableDrive,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDisableDriveOverCellular,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDisableDriveHostedFiles,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  // We don't sync prefs::kLanguageCurrentInputMethod and PreviousInputMethod
  // because they're just used to track the logout state of the device.
  registry->RegisterStringPref(
      prefs::kLanguageCurrentInputMethod,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kLanguagePreviousInputMethod,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  // We don't sync the list of input methods and preferred languages since a
  // user might use two or more devices with different hardware keyboards.
  // crosbug.com/15181
  registry->RegisterStringPref(
      prefs::kLanguagePreferredLanguages,
      kFallbackInputMethodLocale,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kLanguagePreloadEngines,
      hardware_keyboard_id,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kLanguageEnabledExtensionImes,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterIntegerPref(
      prefs::kLanguageRemapSearchKeyTo,
      input_method::kSearchKey,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapControlKeyTo,
      input_method::kControlKey,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapAltKeyTo,
      input_method::kAltKey,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  // We don't sync the CapsLock remapping pref, since the UI hides this pref
  // on certain devices, so syncing a non-default value to a device that
  // doesn't allow changing the pref would be odd. http://crbug.com/167237
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapCapsLockKeyTo,
      input_method::kCapsLockKey,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapDiamondKeyTo,
      input_method::kControlKey,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  // The following pref isn't synced since the user may desire a different value
  // depending on whether an external keyboard is attached to a particular
  // device.
  registry->RegisterBooleanPref(
      prefs::kLanguageSendFunctionKeys,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  // We don't sync the following keyboard prefs since they are not user-
  // configurable.
  registry->RegisterBooleanPref(
      prefs::kLanguageXkbAutoRepeatEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageXkbAutoRepeatDelay,
      language_prefs::kXkbAutoRepeatDelayInMs,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageXkbAutoRepeatInterval,
      language_prefs::kXkbAutoRepeatIntervalInMs,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  // Mobile plan notifications default to on.
  registry->RegisterBooleanPref(
      prefs::kShowPlanNotifications,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // 3G first-time usage promo will be shown at least once.
  registry->RegisterBooleanPref(
      prefs::kShow3gPromoNotification,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  // Initially all existing users would see "What's new" for current version
  // after update.
  registry->RegisterStringPref(prefs::kChromeOSReleaseNotesVersion,
                               "0.0.0.0",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kExternalStorageDisabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterStringPref(
      prefs::kTermsOfServiceURL,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kTouchHudProjectionEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kTouchVirtualKeyboardEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void Preferences::InitUserPrefs(PrefServiceSyncable* prefs) {
  prefs_ = prefs;

  BooleanPrefMember::NamedChangeCallback callback =
      base::Bind(&Preferences::OnPreferenceChanged, base::Unretained(this));

  performance_tracing_enabled_.Init(prefs::kPerformanceTracingEnabled,
                                    prefs, callback);
  tap_to_click_enabled_.Init(prefs::kTapToClickEnabled, prefs, callback);
  tap_dragging_enabled_.Init(prefs::kTapDraggingEnabled, prefs, callback);
  three_finger_click_enabled_.Init(prefs::kEnableTouchpadThreeFingerClick,
      prefs, callback);
  natural_scroll_.Init(prefs::kNaturalScroll, prefs, callback);
  mouse_sensitivity_.Init(prefs::kMouseSensitivity, prefs, callback);
  touchpad_sensitivity_.Init(prefs::kTouchpadSensitivity, prefs, callback);
  primary_mouse_button_right_.Init(prefs::kPrimaryMouseButtonRight,
                                   prefs, callback);
  download_default_directory_.Init(prefs::kDownloadDefaultDirectory,
                                   prefs, callback);
  touch_hud_projection_enabled_.Init(prefs::kTouchHudProjectionEnabled,
                                     prefs, callback);
  preload_engines_.Init(prefs::kLanguagePreloadEngines, prefs, callback);
  enabled_extension_imes_.Init(prefs::kLanguageEnabledExtensionImes,
                               prefs, callback);
  current_input_method_.Init(prefs::kLanguageCurrentInputMethod,
                             prefs, callback);
  previous_input_method_.Init(prefs::kLanguagePreviousInputMethod,
                              prefs, callback);

  xkb_auto_repeat_enabled_.Init(
      prefs::kLanguageXkbAutoRepeatEnabled, prefs, callback);
  xkb_auto_repeat_delay_pref_.Init(
      prefs::kLanguageXkbAutoRepeatDelay, prefs, callback);
  xkb_auto_repeat_interval_pref_.Init(
      prefs::kLanguageXkbAutoRepeatInterval, prefs, callback);
}

void Preferences::Init(PrefServiceSyncable* prefs, const User* user) {
  DCHECK(user);
  user_ = user;
  user_is_primary_ = UserManager::Get()->GetPrimaryUser() == user_;
  InitUserPrefs(prefs);

  UserManager::Get()->AddSessionStateObserver(this);

  // This causes OnIsSyncingChanged to be called when the value of
  // PrefService::IsSyncing() changes.
  prefs->AddObserver(this);

  // Initialize preferences to currently saved state.
  ApplyPreferences(REASON_INITIALIZATION, "");

  // If a guest is logged in, initialize the prefs as if this is the first
  // login. For a regular user this is done in
  // UserSessionManager::InitProfilePreferences().
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession))
    UserSessionManager::SetFirstLoginPrefs(prefs);
}

void Preferences::InitUserPrefsForTesting(PrefServiceSyncable* prefs,
                                          const User* user) {
  user_ = user;
  InitUserPrefs(prefs);
}

void Preferences::SetInputMethodListForTesting() {
  SetInputMethodList();
}

void Preferences::OnPreferenceChanged(const std::string& pref_name) {
  ApplyPreferences(REASON_PREF_CHANGED, pref_name);
}

void Preferences::ApplyPreferences(ApplyReason reason,
                                   const std::string& pref_name) {
  DCHECK(reason != REASON_PREF_CHANGED || !pref_name.empty());
  const bool user_is_owner =
      UserManager::Get()->GetOwnerEmail() == user_->email();
  const bool user_is_active = user_->is_active();

  system::TouchpadSettings touchpad_settings;
  system::MouseSettings mouse_settings;

  if (user_is_primary_ && (reason == REASON_INITIALIZATION ||
                           pref_name == prefs::kPerformanceTracingEnabled)) {
    const bool enabled = performance_tracing_enabled_.GetValue();
    if (enabled)
      tracing_manager_ = TracingManager::Create();
    else
      tracing_manager_.reset();
  }
  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kTapToClickEnabled) {
    const bool enabled = tap_to_click_enabled_.GetValue();
    if (user_is_active)
      touchpad_settings.SetTapToClick(enabled);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapToClick.Changed", enabled);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapToClick.Started", enabled);

    // Save owner preference in local state to use on login screen.
    if (user_is_owner) {
      PrefService* prefs = g_browser_process->local_state();
      if (prefs->GetBoolean(prefs::kOwnerTapToClickEnabled) != enabled)
        prefs->SetBoolean(prefs::kOwnerTapToClickEnabled, enabled);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kTapDraggingEnabled) {
    const bool enabled = tap_dragging_enabled_.GetValue();
    if (user_is_active)
      touchpad_settings.SetTapDragging(enabled);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapDragging.Changed", enabled);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapDragging.Started", enabled);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kEnableTouchpadThreeFingerClick) {
    const bool enabled = three_finger_click_enabled_.GetValue();
    if (user_is_active)
      touchpad_settings.SetThreeFingerClick(enabled);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.ThreeFingerClick.Changed", enabled);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.ThreeFingerClick.Started", enabled);
  }
  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kNaturalScroll) {
    // Force natural scroll default if we've sync'd and if the cmd line arg is
    // set.
    ForceNaturalScrollDefault();

    const bool enabled = natural_scroll_.GetValue();
    DVLOG(1) << "Natural scroll set to " << enabled;
    if (user_is_active)
      touchpad_settings.SetNaturalScroll(enabled);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.NaturalScroll.Changed", enabled);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.NaturalScroll.Started", enabled);
  }
  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kMouseSensitivity) {
    const int sensitivity = mouse_sensitivity_.GetValue();
    if (user_is_active)
      mouse_settings.SetSensitivity(sensitivity);
    if (reason == REASON_PREF_CHANGED) {
      UMA_HISTOGRAM_ENUMERATION("Mouse.PointerSensitivity.Changed",
                                sensitivity,
                                system::kMaxPointerSensitivity + 1);
    } else if (reason == REASON_INITIALIZATION) {
      UMA_HISTOGRAM_ENUMERATION("Mouse.PointerSensitivity.Started",
                                sensitivity,
                                system::kMaxPointerSensitivity + 1);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kTouchpadSensitivity) {
    const int sensitivity = touchpad_sensitivity_.GetValue();
    if (user_is_active)
      touchpad_settings.SetSensitivity(sensitivity);
    if (reason == REASON_PREF_CHANGED) {
      UMA_HISTOGRAM_ENUMERATION("Touchpad.PointerSensitivity.Changed",
                                sensitivity,
                                system::kMaxPointerSensitivity + 1);
    } else if (reason == REASON_INITIALIZATION) {
      UMA_HISTOGRAM_ENUMERATION("Touchpad.PointerSensitivity.Started",
                                sensitivity,
                                system::kMaxPointerSensitivity + 1);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kPrimaryMouseButtonRight) {
    const bool right = primary_mouse_button_right_.GetValue();
    if (user_is_active)
      mouse_settings.SetPrimaryButtonRight(right);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Mouse.PrimaryButtonRight.Changed", right);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Mouse.PrimaryButtonRight.Started", right);
    // Save owner preference in local state to use on login screen.
    if (user_is_owner) {
      PrefService* prefs = g_browser_process->local_state();
      if (prefs->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight) != right)
        prefs->SetBoolean(prefs::kOwnerPrimaryMouseButtonRight, right);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kDownloadDefaultDirectory) {
    const bool default_download_to_drive = drive::util::IsUnderDriveMountPoint(
        download_default_directory_.GetValue());
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN(
          "FileBrowser.DownloadDestination.IsGoogleDrive.Changed",
          default_download_to_drive);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN(
          "FileBrowser.DownloadDestination.IsGoogleDrive.Started",
          default_download_to_drive);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kTouchHudProjectionEnabled) {
    if (user_is_active) {
      const bool enabled = touch_hud_projection_enabled_.GetValue();
      ash::Shell::GetInstance()->SetTouchHudProjectionEnabled(enabled);
    }
  }

  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kLanguageXkbAutoRepeatEnabled) {
    if (user_is_active) {
      const bool enabled = xkb_auto_repeat_enabled_.GetValue();
      input_method::InputMethodManager::Get()
          ->GetImeKeyboard()
          ->SetAutoRepeatEnabled(enabled);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kLanguageXkbAutoRepeatDelay ||
      pref_name == prefs::kLanguageXkbAutoRepeatInterval) {
    if (user_is_active)
      UpdateAutoRepeatRate();
  }

  if (reason != REASON_PREF_CHANGED && user_is_active) {
    SetInputMethodList();
  } else if (pref_name == prefs::kLanguagePreloadEngines && user_is_active) {
    SetLanguageConfigStringListAsCSV(language_prefs::kGeneralSectionName,
                                     language_prefs::kPreloadEnginesConfigName,
                                     preload_engines_.GetValue());
  }

  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kLanguageEnabledExtensionImes) {
    if (user_is_active) {
      std::string value(enabled_extension_imes_.GetValue());

      std::vector<std::string> split_values;
      if (!value.empty())
        base::SplitString(value, ',', &split_values);

      input_method_manager_->SetEnabledExtensionImes(&split_values);
    }
  }

  if (user_is_active) {
    system::InputDeviceSettings::Get()->UpdateTouchpadSettings(
        touchpad_settings);
    system::InputDeviceSettings::Get()->UpdateMouseSettings(mouse_settings);
  }
}

void Preferences::OnIsSyncingChanged() {
  DVLOG(1) << "OnIsSyncingChanged";
  ForceNaturalScrollDefault();
}

void Preferences::ForceNaturalScrollDefault() {
  DVLOG(1) << "ForceNaturalScrollDefault";
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNaturalScrollDefault) &&
      prefs_->IsSyncing() &&
      !prefs_->GetUserPrefValue(prefs::kNaturalScroll)) {
    DVLOG(1) << "Natural scroll forced to true";
    natural_scroll_.SetValue(true);
    UMA_HISTOGRAM_BOOLEAN("Touchpad.NaturalScroll.Forced", true);
  }
}

void Preferences::SetLanguageConfigStringListAsCSV(const char* section,
                                                   const char* name,
                                                   const std::string& value) {
  VLOG(1) << "Setting " << name << " to '" << value << "'";

  std::vector<std::string> split_values;
  if (!value.empty())
    base::SplitString(value, ',', &split_values);

  // Transfers the xkb id to extension-xkb id.
  if (input_method_manager_->MigrateInputMethods(&split_values))
    preload_engines_.SetValue(JoinString(split_values, ','));

  if (section == std::string(language_prefs::kGeneralSectionName) &&
      name == std::string(language_prefs::kPreloadEnginesConfigName)) {
    input_method_manager_->ReplaceEnabledInputMethods(split_values);
    return;
  }
}

void Preferences::SetInputMethodList() {
  // When |preload_engines_| are set, InputMethodManager::ChangeInputMethod()
  // might be called to change the current input method to the first one in the
  // |preload_engines_| list. This also updates previous/current input method
  // prefs. That's why GetValue() calls are placed before the
  // SetLanguageConfigStringListAsCSV() call below.
  const std::string previous_input_method_id =
      previous_input_method_.GetValue();
  const std::string current_input_method_id = current_input_method_.GetValue();
  SetLanguageConfigStringListAsCSV(language_prefs::kGeneralSectionName,
                                   language_prefs::kPreloadEnginesConfigName,
                                   preload_engines_.GetValue());

  // ChangeInputMethod() has to be called AFTER the value of |preload_engines_|
  // is sent to the InputMethodManager. Otherwise, the ChangeInputMethod request
  // might be ignored as an invalid input method ID. The ChangeInputMethod()
  // calls are also necessary to restore the previous/current input method prefs
  // which could have been modified by the SetLanguageConfigStringListAsCSV call
  // above to the original state.
  if (!previous_input_method_id.empty())
    input_method_manager_->ChangeInputMethod(previous_input_method_id);
  if (!current_input_method_id.empty())
    input_method_manager_->ChangeInputMethod(current_input_method_id);
}

void Preferences::UpdateAutoRepeatRate() {
  input_method::AutoRepeatRate rate;
  rate.initial_delay_in_ms = xkb_auto_repeat_delay_pref_.GetValue();
  rate.repeat_interval_in_ms = xkb_auto_repeat_interval_pref_.GetValue();
  DCHECK(rate.initial_delay_in_ms > 0);
  DCHECK(rate.repeat_interval_in_ms > 0);
  input_method::InputMethodManager::Get()
      ->GetImeKeyboard()
      ->SetAutoRepeatRate(rate);
}

void Preferences::OnTouchHudProjectionToggled(bool enabled) {
  if (touch_hud_projection_enabled_.GetValue() == enabled)
    return;
  if (!user_->is_active())
    return;
  touch_hud_projection_enabled_.SetValue(enabled);
}

void Preferences::ActiveUserChanged(const User* active_user) {
  if (active_user != user_)
    return;
  ApplyPreferences(REASON_ACTIVE_USER_CHANGED, "");
}

}  // namespace chromeos
