// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/preferences.h"

#include "ash/magnifier/magnifier_constants.h"
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/ime/xkeyboard.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "googleurl/src/gurl.h"
#include "third_party/icu/public/i18n/unicode/timezone.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/events/event_utils.h"

namespace chromeos {

static const char kFallbackInputMethodLocale[] = "en-US";

Preferences::Preferences()
    : prefs_(NULL),
      input_method_manager_(input_method::GetInputMethodManager()) {
}

Preferences::Preferences(input_method::InputMethodManager* input_method_manager)
    : input_method_manager_(input_method_manager) {
}

Preferences::~Preferences() {
  prefs_->RemoveObserver(this);
}

// static
void Preferences::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kHighContrastEnabled, false);
  registry->RegisterBooleanPref(prefs::kOwnerPrimaryMouseButtonRight, false);
  registry->RegisterBooleanPref(prefs::kOwnerTapToClickEnabled, true);
  registry->RegisterBooleanPref(prefs::kSpokenFeedbackEnabled, false);
  registry->RegisterBooleanPref(prefs::kVirtualKeyboardEnabled, false);
}

// static
void Preferences::RegisterUserPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  std::string hardware_keyboard_id;
  // TODO(yusukes): Remove the runtime hack.
  if (base::chromeos::IsRunningOnChromeOS()) {
    input_method::InputMethodManager* manager =
        input_method::GetInputMethodManager();
    if (manager) {
      hardware_keyboard_id =
          manager->GetInputMethodUtil()->GetHardwareInputMethodId();
    }
  } else {
    hardware_keyboard_id = "xkb:us::eng";  // only for testing.
  }

  registry->RegisterBooleanPref(
      prefs::kTapToClickEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kTapDraggingEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kEnableTouchpadThreeFingerClick,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kEnableTouchpadThreeFingerSwipe,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kNaturalScroll,
      CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kNaturalScrollDefault),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPrimaryMouseButtonRight,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kLabsMediaplayerEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kLabsAdvancedFilesystemEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kScreenMagnifierEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kScreenMagnifierType,
      ash::kDefaultMagnifierType,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kScreenMagnifierScale,
      std::numeric_limits<double>::min(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kShouldAlwaysShowAccessibilityMenu,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kMouseSensitivity,
      3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kTouchpadSensitivity,
      3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
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
      prefs::kLanguageFilteredExtensionImes,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  for (size_t i = 0; i < language_prefs::kNumChewingBooleanPrefs; ++i) {
    registry->RegisterBooleanPref(
        language_prefs::kChewingBooleanPrefs[i].pref_name,
        language_prefs::kChewingBooleanPrefs[i].default_pref_value,
        language_prefs::kChewingBooleanPrefs[i].sync_status);
  }
  for (size_t i = 0; i < language_prefs::kNumChewingMultipleChoicePrefs; ++i) {
    registry->RegisterStringPref(
        language_prefs::kChewingMultipleChoicePrefs[i].pref_name,
        language_prefs::kChewingMultipleChoicePrefs[i].default_pref_value,
        language_prefs::kChewingMultipleChoicePrefs[i].sync_status);
  }
  registry->RegisterIntegerPref(
      language_prefs::kChewingHsuSelKeyType.pref_name,
      language_prefs::kChewingHsuSelKeyType.default_pref_value,
      language_prefs::kChewingHsuSelKeyType.sync_status);

  for (size_t i = 0; i < language_prefs::kNumChewingIntegerPrefs; ++i) {
    registry->RegisterIntegerPref(
        language_prefs::kChewingIntegerPrefs[i].pref_name,
        language_prefs::kChewingIntegerPrefs[i].default_pref_value,
        language_prefs::kChewingIntegerPrefs[i].sync_status);
  }
  registry->RegisterStringPref(
      prefs::kLanguageHangulKeyboard,
      language_prefs::kHangulKeyboardNameIDPairs[0].keyboard_id,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kLanguageHangulHanjaBindingKeys,
      language_prefs::kHangulHanjaBindingKeys,
      // Don't sync the pref as it's not user-configurable
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  for (size_t i = 0; i < language_prefs::kNumPinyinBooleanPrefs; ++i) {
    registry->RegisterBooleanPref(
        language_prefs::kPinyinBooleanPrefs[i].pref_name,
        language_prefs::kPinyinBooleanPrefs[i].default_pref_value,
        language_prefs::kPinyinBooleanPrefs[i].sync_status);
  }
  for (size_t i = 0; i < language_prefs::kNumPinyinIntegerPrefs; ++i) {
    registry->RegisterIntegerPref(
        language_prefs::kPinyinIntegerPrefs[i].pref_name,
        language_prefs::kPinyinIntegerPrefs[i].default_pref_value,
        language_prefs::kPinyinIntegerPrefs[i].sync_status);
  }
  registry->RegisterIntegerPref(
      language_prefs::kPinyinDoublePinyinSchema.pref_name,
      language_prefs::kPinyinDoublePinyinSchema.default_pref_value,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  for (size_t i = 0; i < language_prefs::kNumMozcBooleanPrefs; ++i) {
    registry->RegisterBooleanPref(
        language_prefs::kMozcBooleanPrefs[i].pref_name,
        language_prefs::kMozcBooleanPrefs[i].default_pref_value,
        language_prefs::kMozcBooleanPrefs[i].sync_status);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    registry->RegisterStringPref(
        language_prefs::kMozcMultipleChoicePrefs[i].pref_name,
        language_prefs::kMozcMultipleChoicePrefs[i].default_pref_value,
        language_prefs::kMozcMultipleChoicePrefs[i].sync_status);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcIntegerPrefs; ++i) {
    registry->RegisterIntegerPref(
        language_prefs::kMozcIntegerPrefs[i].pref_name,
        language_prefs::kMozcIntegerPrefs[i].default_pref_value,
        language_prefs::kMozcIntegerPrefs[i].sync_status);
  }
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapSearchKeyTo,
      input_method::kSearchKey,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapControlKeyTo,
      input_method::kControlKey,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapAltKeyTo,
      input_method::kAltKey,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapCapsLockKeyTo,
      input_method::kCapsLockKey,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapDiamondKeyTo,
      input_method::kControlKey,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
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

  // Screen lock default to off.
  registry->RegisterBooleanPref(
      prefs::kEnableScreenLock,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

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

  registry->RegisterIntegerPref(
      prefs::kPowerAcScreenDimDelayMs,
      420000,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kPowerAcScreenOffDelayMs,
      480000,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kPowerAcScreenLockDelayMs,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kPowerAcIdleWarningDelayMs,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kPowerAcIdleDelayMs,
      1800000,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kPowerBatteryScreenDimDelayMs,
      300000,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kPowerBatteryScreenOffDelayMs,
      360000,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kPowerBatteryScreenLockDelayMs,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kPowerBatteryIdleWarningDelayMs,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kPowerBatteryIdleDelayMs,
      600000,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kPowerIdleAction,
      PowerPolicyController::ACTION_SUSPEND,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kPowerLidClosedAction,
      PowerPolicyController::ACTION_SUSPEND,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPowerUseAudioActivity,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPowerUseVideoActivity,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPowerAllowScreenWakeLocks,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kPowerPresentationIdleDelayFactor,
      2.0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  // TODO(derat): Change the default to 2.0 once a policy is added such
  // that this can be set to 1.0 for public accounts.
  registry->RegisterDoublePref(
      prefs::kPowerUserActivityScreenDimDelayFactor,
      1.0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterStringPref(
      prefs::kTermsOfServiceURL,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void Preferences::InitUserPrefs(PrefServiceSyncable* prefs) {
  prefs_ = prefs;

  BooleanPrefMember::NamedChangeCallback callback =
      base::Bind(&Preferences::OnPreferenceChanged, base::Unretained(this));

  tap_to_click_enabled_.Init(prefs::kTapToClickEnabled, prefs, callback);
  tap_dragging_enabled_.Init(prefs::kTapDraggingEnabled, prefs, callback);
  three_finger_click_enabled_.Init(prefs::kEnableTouchpadThreeFingerClick,
      prefs, callback);
  three_finger_swipe_enabled_.Init(prefs::kEnableTouchpadThreeFingerSwipe,
      prefs, callback);
  natural_scroll_.Init(prefs::kNaturalScroll, prefs, callback);
  screen_magnifier_enabled_.Init(prefs::kScreenMagnifierEnabled,
                                 prefs, callback);
  screen_magnifier_type_.Init(prefs::kScreenMagnifierType, prefs, callback);
  screen_magnifier_scale_.Init(prefs::kScreenMagnifierScale, prefs, callback);
  mouse_sensitivity_.Init(prefs::kMouseSensitivity, prefs, callback);
  touchpad_sensitivity_.Init(prefs::kTouchpadSensitivity, prefs, callback);
  use_24hour_clock_.Init(prefs::kUse24HourClock, prefs, callback);
  disable_drive_.Init(prefs::kDisableDrive, prefs, callback);
  disable_drive_over_cellular_.Init(prefs::kDisableDriveOverCellular,
                                    prefs, callback);
  disable_drive_hosted_files_.Init(prefs::kDisableDriveHostedFiles,
                                   prefs, callback);
  download_default_directory_.Init(prefs::kDownloadDefaultDirectory,
                                   prefs, callback);
  select_file_last_directory_.Init(prefs::kSelectFileLastDirectory,
                                   prefs, callback);
  save_file_default_directory_.Init(prefs::kSaveFileDefaultDirectory,
                                    prefs, callback);
  primary_mouse_button_right_.Init(prefs::kPrimaryMouseButtonRight,
                                   prefs, callback);
  preferred_languages_.Init(prefs::kLanguagePreferredLanguages,
                            prefs, callback);
  preload_engines_.Init(prefs::kLanguagePreloadEngines, prefs, callback);
  filtered_extension_imes_.Init(prefs::kLanguageFilteredExtensionImes,
                                prefs, callback);
  current_input_method_.Init(prefs::kLanguageCurrentInputMethod,
                             prefs, callback);
  previous_input_method_.Init(prefs::kLanguagePreviousInputMethod,
                              prefs, callback);

  for (size_t i = 0; i < language_prefs::kNumChewingBooleanPrefs; ++i) {
    chewing_boolean_prefs_[i].Init(
        language_prefs::kChewingBooleanPrefs[i].pref_name, prefs, callback);
  }
  for (size_t i = 0; i < language_prefs::kNumChewingMultipleChoicePrefs; ++i) {
    chewing_multiple_choice_prefs_[i].Init(
        language_prefs::kChewingMultipleChoicePrefs[i].pref_name,
        prefs, callback);
  }
  chewing_hsu_sel_key_type_.Init(
      language_prefs::kChewingHsuSelKeyType.pref_name, prefs, callback);
  for (size_t i = 0; i < language_prefs::kNumChewingIntegerPrefs; ++i) {
    chewing_integer_prefs_[i].Init(
        language_prefs::kChewingIntegerPrefs[i].pref_name, prefs, callback);
  }
  hangul_keyboard_.Init(prefs::kLanguageHangulKeyboard, prefs, callback);
  hangul_hanja_binding_keys_.Init(
      prefs::kLanguageHangulHanjaBindingKeys, prefs, callback);
  for (size_t i = 0; i < language_prefs::kNumPinyinBooleanPrefs; ++i) {
    pinyin_boolean_prefs_[i].Init(
        language_prefs::kPinyinBooleanPrefs[i].pref_name, prefs, callback);
  }
  for (size_t i = 0; i < language_prefs::kNumPinyinIntegerPrefs; ++i) {
    pinyin_int_prefs_[i].Init(
        language_prefs::kPinyinIntegerPrefs[i].pref_name, prefs, callback);
  }
  pinyin_double_pinyin_schema_.Init(
      language_prefs::kPinyinDoublePinyinSchema.pref_name, prefs, callback);
  for (size_t i = 0; i < language_prefs::kNumMozcBooleanPrefs; ++i) {
    mozc_boolean_prefs_[i].Init(
        language_prefs::kMozcBooleanPrefs[i].pref_name, prefs, callback);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    mozc_multiple_choice_prefs_[i].Init(
        language_prefs::kMozcMultipleChoicePrefs[i].pref_name, prefs, callback);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcIntegerPrefs; ++i) {
    mozc_integer_prefs_[i].Init(
        language_prefs::kMozcIntegerPrefs[i].pref_name, prefs, callback);
  }
  xkb_auto_repeat_enabled_.Init(
      prefs::kLanguageXkbAutoRepeatEnabled, prefs, callback);
  xkb_auto_repeat_delay_pref_.Init(
      prefs::kLanguageXkbAutoRepeatDelay, prefs, callback);
  xkb_auto_repeat_interval_pref_.Init(
      prefs::kLanguageXkbAutoRepeatInterval, prefs, callback);

  enable_screen_lock_.Init(prefs::kEnableScreenLock, prefs, callback);

  power_ac_screen_dim_delay_ms_.Init(
      prefs::kPowerAcScreenDimDelayMs, prefs, callback);
  power_ac_screen_off_delay_ms_.Init(
      prefs::kPowerAcScreenOffDelayMs, prefs, callback);
  power_ac_screen_lock_delay_ms_.Init(
      prefs::kPowerAcScreenLockDelayMs, prefs, callback);
  power_ac_idle_warning_delay_ms_.Init(
      prefs::kPowerAcIdleWarningDelayMs, prefs, callback);
  power_ac_idle_delay_ms_.Init(prefs::kPowerAcIdleDelayMs, prefs, callback);
  power_battery_screen_dim_delay_ms_.Init(
      prefs::kPowerBatteryScreenDimDelayMs, prefs, callback);
  power_battery_screen_off_delay_ms_.Init(
      prefs::kPowerBatteryScreenOffDelayMs, prefs, callback);
  power_battery_screen_lock_delay_ms_.Init(
      prefs::kPowerBatteryScreenLockDelayMs, prefs, callback);
  power_battery_idle_warning_delay_ms_.Init(
      prefs::kPowerBatteryIdleWarningDelayMs, prefs, callback);
  power_battery_idle_delay_ms_.Init(
      prefs::kPowerBatteryIdleDelayMs, prefs, callback);
  power_idle_action_.Init(prefs::kPowerIdleAction, prefs, callback);
  power_lid_closed_action_.Init(prefs::kPowerLidClosedAction, prefs, callback);
  power_use_audio_activity_.Init(
      prefs::kPowerUseAudioActivity, prefs, callback);
  power_use_video_activity_.Init(
      prefs::kPowerUseVideoActivity, prefs, callback);
  power_allow_screen_wake_locks_.Init(
      prefs::kPowerAllowScreenWakeLocks, prefs, callback);
  power_presentation_idle_delay_factor_.Init(
      prefs::kPowerPresentationIdleDelayFactor, prefs, callback);
  power_user_activity_screen_dim_delay_factor_.Init(
      prefs::kPowerUserActivityScreenDimDelayFactor, prefs, callback);
}

void Preferences::Init(PrefServiceSyncable* prefs) {
  InitUserPrefs(prefs);

  // This causes OnIsSyncingChanged to be called when the value of
  // PrefService::IsSyncing() changes.
  prefs->AddObserver(this);

  // Initialize preferences to currently saved state.
  NotifyPrefChanged(NULL);

  // If a guest is logged in, initialize the prefs as if this is the first
  // login.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession)) {
    LoginUtils::Get()->SetFirstLoginPrefs(prefs);
  }
}

void Preferences::InitUserPrefsForTesting(PrefServiceSyncable* prefs) {
  InitUserPrefs(prefs);
}

void Preferences::SetInputMethodListForTesting() {
  SetInputMethodList();
}

void Preferences::OnPreferenceChanged(const std::string& pref_name) {
  NotifyPrefChanged(&pref_name);
}

void Preferences::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || *pref_name == prefs::kTapToClickEnabled) {
    const bool enabled = tap_to_click_enabled_.GetValue();
    system::touchpad_settings::SetTapToClick(enabled);
    if (pref_name)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapToClick.Changed", enabled);
    else
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapToClick.Started", enabled);

    // Save owner preference in local state to use on login screen.
    if (chromeos::UserManager::Get()->IsCurrentUserOwner()) {
      PrefService* prefs = g_browser_process->local_state();
      if (prefs->GetBoolean(prefs::kOwnerTapToClickEnabled) != enabled)
        prefs->SetBoolean(prefs::kOwnerTapToClickEnabled, enabled);
    }
  }
  if (!pref_name || *pref_name == prefs::kTapDraggingEnabled) {
    const bool enabled = tap_dragging_enabled_.GetValue();
    system::touchpad_settings::SetTapDragging(enabled);
    if (pref_name)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapDragging.Changed", enabled);
    else
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapDragging.Started", enabled);
  }
  if (!pref_name || *pref_name == prefs::kEnableTouchpadThreeFingerClick) {
    const bool enabled = three_finger_click_enabled_.GetValue();
    system::touchpad_settings::SetThreeFingerClick(enabled);
    if (pref_name)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.ThreeFingerClick.Changed", enabled);
    else
      UMA_HISTOGRAM_BOOLEAN("Touchpad.ThreeFingerClick.Started", enabled);
  }
  if (!pref_name || *pref_name == prefs::kEnableTouchpadThreeFingerSwipe) {
    const bool enabled = three_finger_swipe_enabled_.GetValue();
    system::touchpad_settings::SetThreeFingerSwipe(enabled);
    if (pref_name)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.ThreeFingerSwipe.Changed", enabled);
    else
      UMA_HISTOGRAM_BOOLEAN("Touchpad.ThreeFingerSwipe.Started", enabled);
  }
  if (!pref_name || *pref_name == prefs::kNaturalScroll) {
    // Force natural scroll default if we've sync'd and if the cmd line arg is
    // set.
    ForceNaturalScrollDefault();

    const bool enabled = natural_scroll_.GetValue();
    DVLOG(1) << "Natural scroll set to " << enabled;
    ui::SetNaturalScroll(enabled);
    if (pref_name)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.NaturalScroll.Changed", enabled);
    else
      UMA_HISTOGRAM_BOOLEAN("Touchpad.NaturalScroll.Started", enabled);
  }
  if (!pref_name || *pref_name == prefs::kMouseSensitivity) {
    const int sensitivity = mouse_sensitivity_.GetValue();
    system::mouse_settings::SetSensitivity(sensitivity);
    if (pref_name) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Mouse.Sensitivity.Changed", sensitivity, 1, 5, 5);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Mouse.Sensitivity.Started", sensitivity, 1, 5, 5);
    }
  }
  if (!pref_name || *pref_name == prefs::kTouchpadSensitivity) {
    const int sensitivity = touchpad_sensitivity_.GetValue();
    system::touchpad_settings::SetSensitivity(sensitivity);
    if (pref_name) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Touchpad.Sensitivity.Changed", sensitivity, 1, 5, 5);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Touchpad.Sensitivity.Started", sensitivity, 1, 5, 5);
    }
  }
  if (!pref_name || *pref_name == prefs::kPrimaryMouseButtonRight) {
    const bool right = primary_mouse_button_right_.GetValue();
    system::mouse_settings::SetPrimaryButtonRight(right);
    if (pref_name)
      UMA_HISTOGRAM_BOOLEAN("Mouse.PrimaryButtonRight.Changed", right);
    else
      UMA_HISTOGRAM_BOOLEAN("Mouse.PrimaryButtonRight.Started", right);

    // Save owner preference in local state to use on login screen.
    if (chromeos::UserManager::Get()->IsCurrentUserOwner()) {
      PrefService* prefs = g_browser_process->local_state();
      if (prefs->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight) != right)
        prefs->SetBoolean(prefs::kOwnerPrimaryMouseButtonRight, right);
    }
  }
  if (!pref_name || *pref_name == prefs::kDownloadDefaultDirectory) {
    const base::FilePath pref_path = download_default_directory_.GetValue();
    // TODO(haruki): Remove this when migration completes. crbug.com/229304.
    if (drive::util::NeedsNamespaceMigration(pref_path)) {
      prefs_->SetFilePath(prefs::kDownloadDefaultDirectory,
                          drive::util::ConvertToMyDriveNamespace(pref_path));
    }

    const bool default_download_to_drive = drive::util::IsUnderDriveMountPoint(
        download_default_directory_.GetValue());
    if (pref_name)
      UMA_HISTOGRAM_BOOLEAN(
          "FileBrowser.DownloadDestination.IsGoogleDrive.Changed",
          default_download_to_drive);
    else
      UMA_HISTOGRAM_BOOLEAN(
          "FileBrowser.DownloadDestination.IsGoogleDrive.Started",
          default_download_to_drive);
  }
  if (!pref_name || *pref_name == prefs::kSelectFileLastDirectory) {
    const base::FilePath pref_path = select_file_last_directory_.GetValue();
    // This pref can contain a Drive path, which needs to be updated due to
    // namespaces introduced by crbug.com/174233.
    // TODO(haruki): Remove this when migration completes. crbug.com/229304.
    if (drive::util::NeedsNamespaceMigration(pref_path)) {
      prefs_->SetFilePath(prefs::kSelectFileLastDirectory,
                          drive::util::ConvertToMyDriveNamespace(pref_path));
    }
  }
  if (!pref_name || *pref_name == prefs::kSaveFileDefaultDirectory) {
    const base::FilePath pref_path = save_file_default_directory_.GetValue();
    // This pref can contain a Drive path, which needs to be updated due to
    // namespaces introduced by crbug.com/174233.
    // TODO(haruki): Remove this when migration completes. crbug.com/229304.
    if (drive::util::NeedsNamespaceMigration(pref_path)) {
      prefs_->SetFilePath(prefs::kSaveFileDefaultDirectory,
                          drive::util::ConvertToMyDriveNamespace(pref_path));
    }
  }

  if (!pref_name || *pref_name == prefs::kLanguagePreferredLanguages) {
    // Unlike kLanguagePreloadEngines and some other input method
    // preferencs, we don't need to send this to ibus-daemon.
  }

  if (!pref_name || *pref_name == prefs::kLanguageXkbAutoRepeatEnabled) {
    const bool enabled = xkb_auto_repeat_enabled_.GetValue();
    input_method::XKeyboard::SetAutoRepeatEnabled(enabled);
  }
  if (!pref_name || ((*pref_name == prefs::kLanguageXkbAutoRepeatDelay) ||
                     (*pref_name == prefs::kLanguageXkbAutoRepeatInterval))) {
    UpdateAutoRepeatRate();
  }

  if (!pref_name) {
    SetInputMethodList();
  } else if (*pref_name == prefs::kLanguagePreloadEngines) {
    SetLanguageConfigStringListAsCSV(language_prefs::kGeneralSectionName,
                                     language_prefs::kPreloadEnginesConfigName,
                                     preload_engines_.GetValue());
  }

  if (!pref_name || *pref_name == prefs::kLanguageFilteredExtensionImes) {
    std::string value(filtered_extension_imes_.GetValue());

    std::vector<std::string> split_values;
    if (!value.empty())
      base::SplitString(value, ',', &split_values);

    input_method_manager_->SetFilteredExtensionImes(&split_values);
  }

  // Do not check |*pref_name| of the prefs for remembering current/previous
  // input methods here. We're only interested in initial values of the prefs.

  for (size_t i = 0; i < language_prefs::kNumChewingBooleanPrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kChewingBooleanPrefs[i].pref_name) {
      SetLanguageConfigBoolean(
          language_prefs::kChewingSectionName,
          language_prefs::kChewingBooleanPrefs[i].ibus_config_name,
          chewing_boolean_prefs_[i].GetValue());
    }
  }
  for (size_t i = 0; i < language_prefs::kNumChewingMultipleChoicePrefs; ++i) {
    if (!pref_name ||
        *pref_name ==
        language_prefs::kChewingMultipleChoicePrefs[i].pref_name) {
      SetLanguageConfigString(
          language_prefs::kChewingSectionName,
          language_prefs::kChewingMultipleChoicePrefs[i].ibus_config_name,
          chewing_multiple_choice_prefs_[i].GetValue());
    }
  }
  if (!pref_name ||
      *pref_name == language_prefs::kChewingHsuSelKeyType.pref_name) {
    SetLanguageConfigInteger(
        language_prefs::kChewingSectionName,
        language_prefs::kChewingHsuSelKeyType.ibus_config_name,
        chewing_hsu_sel_key_type_.GetValue());
  }
  for (size_t i = 0; i < language_prefs::kNumChewingIntegerPrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kChewingIntegerPrefs[i].pref_name) {
      SetLanguageConfigInteger(
          language_prefs::kChewingSectionName,
          language_prefs::kChewingIntegerPrefs[i].ibus_config_name,
          chewing_integer_prefs_[i].GetValue());
    }
  }
  if (!pref_name ||
      *pref_name == prefs::kLanguageHangulKeyboard) {
    SetLanguageConfigString(language_prefs::kHangulSectionName,
                            language_prefs::kHangulKeyboardConfigName,
                            hangul_keyboard_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kLanguageHangulHanjaBindingKeys) {
    SetLanguageConfigString(language_prefs::kHangulSectionName,
                            language_prefs::kHangulHanjaBindingKeysConfigName,
                            hangul_hanja_binding_keys_.GetValue());
  }
  for (size_t i = 0; i < language_prefs::kNumPinyinBooleanPrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kPinyinBooleanPrefs[i].pref_name) {
      SetLanguageConfigBoolean(
          language_prefs::kPinyinSectionName,
          language_prefs::kPinyinBooleanPrefs[i].ibus_config_name,
          pinyin_boolean_prefs_[i].GetValue());
    }
  }
  for (size_t i = 0; i < language_prefs::kNumPinyinIntegerPrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kPinyinIntegerPrefs[i].pref_name) {
      SetLanguageConfigInteger(
          language_prefs::kPinyinSectionName,
          language_prefs::kPinyinIntegerPrefs[i].ibus_config_name,
          pinyin_int_prefs_[i].GetValue());
    }
  }
  if (!pref_name ||
      *pref_name == language_prefs::kPinyinDoublePinyinSchema.pref_name) {
    SetLanguageConfigInteger(
        language_prefs::kPinyinSectionName,
        language_prefs::kPinyinDoublePinyinSchema.ibus_config_name,
        pinyin_double_pinyin_schema_.GetValue());
  }
  for (size_t i = 0; i < language_prefs::kNumMozcBooleanPrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kMozcBooleanPrefs[i].pref_name) {
      SetLanguageConfigBoolean(
          language_prefs::kMozcSectionName,
          language_prefs::kMozcBooleanPrefs[i].ibus_config_name,
          mozc_boolean_prefs_[i].GetValue());
    }
  }
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kMozcMultipleChoicePrefs[i].pref_name) {
      SetLanguageConfigString(
          language_prefs::kMozcSectionName,
          language_prefs::kMozcMultipleChoicePrefs[i].ibus_config_name,
          mozc_multiple_choice_prefs_[i].GetValue());
    }
  }
  for (size_t i = 0; i < language_prefs::kNumMozcIntegerPrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kMozcIntegerPrefs[i].pref_name) {
      SetLanguageConfigInteger(
          language_prefs::kMozcSectionName,
          language_prefs::kMozcIntegerPrefs[i].ibus_config_name,
          mozc_integer_prefs_[i].GetValue());
    }
  }

  // Change the download directory to the default value if a Drive directory is
  // selected and Drive is disabled.
  if (!pref_name || *pref_name == prefs::kDisableDrive) {
    if (disable_drive_.GetValue()) {
      if (drive::util::IsUnderDriveMountPoint(
          download_default_directory_.GetValue())) {
        prefs_->SetFilePath(prefs::kDownloadDefaultDirectory,
                            download_util::GetDefaultDownloadDirectory());
      }
    }
  }

  if (!pref_name ||
      *pref_name == prefs::kPowerAcScreenDimDelayMs ||
      *pref_name == prefs::kPowerAcScreenOffDelayMs ||
      *pref_name == prefs::kPowerAcScreenLockDelayMs ||
      *pref_name == prefs::kPowerAcIdleWarningDelayMs ||
      *pref_name == prefs::kPowerAcIdleDelayMs ||
      *pref_name == prefs::kPowerBatteryScreenDimDelayMs ||
      *pref_name == prefs::kPowerBatteryScreenOffDelayMs ||
      *pref_name == prefs::kPowerBatteryScreenLockDelayMs ||
      *pref_name == prefs::kPowerBatteryIdleWarningDelayMs ||
      *pref_name == prefs::kPowerBatteryIdleDelayMs ||
      *pref_name == prefs::kPowerIdleAction ||
      *pref_name == prefs::kPowerLidClosedAction ||
      *pref_name == prefs::kPowerUseAudioActivity ||
      *pref_name == prefs::kPowerUseVideoActivity ||
      *pref_name == prefs::kPowerAllowScreenWakeLocks ||
      *pref_name == prefs::kPowerPresentationIdleDelayFactor ||
      *pref_name == prefs::kPowerUserActivityScreenDimDelayFactor ||
      *pref_name == prefs::kEnableScreenLock) {
    PowerPolicyController::PrefValues values;
    values.ac_screen_dim_delay_ms = power_ac_screen_dim_delay_ms_.GetValue();
    values.ac_screen_off_delay_ms = power_ac_screen_off_delay_ms_.GetValue();
    values.ac_screen_lock_delay_ms = power_ac_screen_lock_delay_ms_.GetValue();
    values.ac_idle_warning_delay_ms =
        power_ac_idle_warning_delay_ms_.GetValue();
    values.ac_idle_delay_ms = power_ac_idle_delay_ms_.GetValue();
    values.battery_screen_dim_delay_ms =
        power_battery_screen_dim_delay_ms_.GetValue();
    values.battery_screen_off_delay_ms =
        power_battery_screen_off_delay_ms_.GetValue();
    values.battery_screen_lock_delay_ms =
        power_battery_screen_lock_delay_ms_.GetValue();
    values.battery_idle_warning_delay_ms =
        power_battery_idle_warning_delay_ms_.GetValue();
    values.battery_idle_delay_ms = power_battery_idle_delay_ms_.GetValue();
    values.idle_action = static_cast<PowerPolicyController::Action>(
        power_idle_action_.GetValue());
    values.lid_closed_action = static_cast<PowerPolicyController::Action>(
        power_lid_closed_action_.GetValue());
    values.use_audio_activity = power_use_audio_activity_.GetValue();
    values.use_video_activity = power_use_video_activity_.GetValue();
    values.allow_screen_wake_locks = power_allow_screen_wake_locks_.GetValue();
    values.enable_screen_lock = enable_screen_lock_.GetValue();
    values.presentation_idle_delay_factor =
        power_presentation_idle_delay_factor_.GetValue();
    values.user_activity_screen_dim_delay_factor =
        power_user_activity_screen_dim_delay_factor_.GetValue();

    DBusThreadManager::Get()->GetPowerPolicyController()->ApplyPrefs(values);
  }
}

void Preferences::OnIsSyncingChanged() {
  DVLOG(1) << "OnIsSyncingChanged";
  ForceNaturalScrollDefault();
}

void Preferences::ForceNaturalScrollDefault() {
  DVLOG(1) << "ForceNaturalScrollDefault";
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kNaturalScrollDefault) &&
      prefs_->IsSyncing() &&
      !prefs_->GetUserPrefValue(prefs::kNaturalScroll)) {
    DVLOG(1) << "Natural scroll forced to true";
    natural_scroll_.SetValue(true);
    UMA_HISTOGRAM_BOOLEAN("Touchpad.NaturalScroll.Forced", true);
  }
}

void Preferences::SetLanguageConfigBoolean(const char* section,
                                           const char* name,
                                           bool value) {
  input_method::InputMethodConfigValue config;
  config.type = input_method::InputMethodConfigValue::kValueTypeBool;
  config.bool_value = value;
  input_method_manager_->SetInputMethodConfig(section, name, config);
}

void Preferences::SetLanguageConfigInteger(const char* section,
                                           const char* name,
                                           int value) {
  input_method::InputMethodConfigValue config;
  config.type = input_method::InputMethodConfigValue::kValueTypeInt;
  config.int_value = value;
  input_method_manager_->SetInputMethodConfig(section, name, config);
}

void Preferences::SetLanguageConfigString(const char* section,
                                          const char* name,
                                          const std::string& value) {
  input_method::InputMethodConfigValue config;
  config.type = input_method::InputMethodConfigValue::kValueTypeString;
  config.string_value = value;
  input_method_manager_->SetInputMethodConfig(section, name, config);
}

void Preferences::SetLanguageConfigStringList(
    const char* section,
    const char* name,
    const std::vector<std::string>& values) {
  input_method::InputMethodConfigValue config;
  config.type = input_method::InputMethodConfigValue::kValueTypeStringList;
  for (size_t i = 0; i < values.size(); ++i)
    config.string_list_value.push_back(values[i]);

  input_method_manager_->SetInputMethodConfig(section, name, config);
}

void Preferences::SetLanguageConfigStringListAsCSV(const char* section,
                                                   const char* name,
                                                   const std::string& value) {
  VLOG(1) << "Setting " << name << " to '" << value << "'";

  std::vector<std::string> split_values;
  if (!value.empty())
    base::SplitString(value, ',', &split_values);

  if (section == std::string(language_prefs::kGeneralSectionName) &&
      name == std::string(language_prefs::kPreloadEnginesConfigName)) {
    // TODO(nona): Remove this function after few milestones are passed.
    //             (http://crbug.com/236747)
    if (input_method_manager_->MigrateOldInputMethods(&split_values))
      preload_engines_.SetValue(JoinString(split_values, ','));
    input_method_manager_->EnableInputMethods(split_values);
    return;
  }

  // We should call the cros API even when |value| is empty, to disable default
  // config.
  SetLanguageConfigStringList(section, name, split_values);
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
  // Avoid setting repeat rate on desktop dev environment.
  if (!base::chromeos::IsRunningOnChromeOS())
    return;

  input_method::AutoRepeatRate rate;
  rate.initial_delay_in_ms = xkb_auto_repeat_delay_pref_.GetValue();
  rate.repeat_interval_in_ms = xkb_auto_repeat_interval_pref_.GetValue();
  DCHECK(rate.initial_delay_in_ms > 0);
  DCHECK(rate.repeat_interval_in_ms > 0);
  input_method::XKeyboard::SetAutoRepeatRate(rate);
}

}  // namespace chromeos
