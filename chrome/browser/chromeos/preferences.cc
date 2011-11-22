// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/preferences.h"

#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/system/screen_locker_settings.h"
#include "chrome/browser/chromeos/system/touchpad_settings.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "googleurl/src/gurl.h"
#include "unicode/timezone.h"

namespace chromeos {

static const char kFallbackInputMethodLocale[] = "en-US";

Preferences::Preferences() {}

Preferences::~Preferences() {}

// static
void Preferences::RegisterUserPrefs(PrefService* prefs) {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::GetInstance();

  prefs->RegisterBooleanPref(prefs::kTapToClickEnabled,
                             false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kLabsMediaplayerEnabled,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kLabsAdvancedFilesystemEnabled,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  // Check if the accessibility pref is already registered, which can happen
  // in WizardController::RegisterPrefs. We still want to try to register
  // the pref here in case of Chrome/Linux with ChromeOS=1.
  if (prefs->FindPreference(prefs::kAccessibilityEnabled) == NULL) {
    prefs->RegisterBooleanPref(prefs::kAccessibilityEnabled,
                               false,
                               PrefService::UNSYNCABLE_PREF);
  }
  prefs->RegisterIntegerPref(prefs::kTouchpadSensitivity,
                             3,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kUse24HourClock,
                             base::GetHourClockType() == base::k24HourClock,
                             PrefService::SYNCABLE_PREF);
  // We don't sync prefs::kLanguageCurrentInputMethod and PreviousInputMethod
  // because they're just used to track the logout state of the device.
  prefs->RegisterStringPref(prefs::kLanguageCurrentInputMethod,
                            "",
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kLanguagePreviousInputMethod,
                            "",
                            PrefService::UNSYNCABLE_PREF);
  // We don't sync input method hotkeys since they're not configurable.
  prefs->RegisterStringPref(prefs::kLanguageHotkeyNextEngineInMenu,
                            language_prefs::kHotkeyNextEngineInMenu,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kLanguageHotkeyPreviousEngine,
                            language_prefs::kHotkeyPreviousEngine,
                            PrefService::UNSYNCABLE_PREF);
  // We don't sync the list of input methods and preferred languages since a
  // user might use two or more devices with different hardware keyboards.
  // crosbug.com/15181
  prefs->RegisterStringPref(prefs::kLanguagePreferredLanguages,
                            kFallbackInputMethodLocale,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(
      prefs::kLanguagePreloadEngines,
      manager->GetInputMethodUtil()->GetHardwareInputMethodId(),
      PrefService::UNSYNCABLE_PREF);
  for (size_t i = 0; i < language_prefs::kNumChewingBooleanPrefs; ++i) {
    prefs->RegisterBooleanPref(
        language_prefs::kChewingBooleanPrefs[i].pref_name,
        language_prefs::kChewingBooleanPrefs[i].default_pref_value,
        PrefService::UNSYNCABLE_PREF);
  }
  for (size_t i = 0; i < language_prefs::kNumChewingMultipleChoicePrefs; ++i) {
    prefs->RegisterStringPref(
        language_prefs::kChewingMultipleChoicePrefs[i].pref_name,
        language_prefs::kChewingMultipleChoicePrefs[i].default_pref_value,
        PrefService::UNSYNCABLE_PREF);
  }
  prefs->RegisterIntegerPref(
      language_prefs::kChewingHsuSelKeyType.pref_name,
      language_prefs::kChewingHsuSelKeyType.default_pref_value,
      PrefService::UNSYNCABLE_PREF);

  for (size_t i = 0; i < language_prefs::kNumChewingIntegerPrefs; ++i) {
    prefs->RegisterIntegerPref(
        language_prefs::kChewingIntegerPrefs[i].pref_name,
        language_prefs::kChewingIntegerPrefs[i].default_pref_value,
        PrefService::UNSYNCABLE_PREF);
  }
  prefs->RegisterStringPref(
      prefs::kLanguageHangulKeyboard,
      language_prefs::kHangulKeyboardNameIDPairs[0].keyboard_id,
      PrefService::SYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kLanguageHangulHanjaBindingKeys,
                            language_prefs::kHangulHanjaBindingKeys,
                            // Don't sync the pref as it's not user-configurable
                            PrefService::UNSYNCABLE_PREF);
  for (size_t i = 0; i < language_prefs::kNumPinyinBooleanPrefs; ++i) {
    prefs->RegisterBooleanPref(
        language_prefs::kPinyinBooleanPrefs[i].pref_name,
        language_prefs::kPinyinBooleanPrefs[i].default_pref_value,
        language_prefs::kPinyinBooleanPrefs[i].sync_status);
  }
  for (size_t i = 0; i < language_prefs::kNumPinyinIntegerPrefs; ++i) {
    prefs->RegisterIntegerPref(
        language_prefs::kPinyinIntegerPrefs[i].pref_name,
        language_prefs::kPinyinIntegerPrefs[i].default_pref_value,
        language_prefs::kPinyinIntegerPrefs[i].sync_status);
  }
  prefs->RegisterIntegerPref(
      language_prefs::kPinyinDoublePinyinSchema.pref_name,
      language_prefs::kPinyinDoublePinyinSchema.default_pref_value,
      PrefService::UNSYNCABLE_PREF);

  for (size_t i = 0; i < language_prefs::kNumMozcBooleanPrefs; ++i) {
    prefs->RegisterBooleanPref(
        language_prefs::kMozcBooleanPrefs[i].pref_name,
        language_prefs::kMozcBooleanPrefs[i].default_pref_value,
        language_prefs::kMozcBooleanPrefs[i].sync_status);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    prefs->RegisterStringPref(
        language_prefs::kMozcMultipleChoicePrefs[i].pref_name,
        language_prefs::kMozcMultipleChoicePrefs[i].default_pref_value,
        language_prefs::kMozcMultipleChoicePrefs[i].sync_status);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcIntegerPrefs; ++i) {
    prefs->RegisterIntegerPref(
        language_prefs::kMozcIntegerPrefs[i].pref_name,
        language_prefs::kMozcIntegerPrefs[i].default_pref_value,
        language_prefs::kMozcIntegerPrefs[i].sync_status);
  }
  prefs->RegisterIntegerPref(prefs::kLanguageXkbRemapSearchKeyTo,
                             input_method::kSearchKey,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kLanguageXkbRemapControlKeyTo,
                             input_method::kLeftControlKey,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kLanguageXkbRemapAltKeyTo,
                             input_method::kLeftAltKey,
                             PrefService::SYNCABLE_PREF);
  // We don't sync the following keyboard prefs since they are not user-
  // configurable.
  prefs->RegisterBooleanPref(prefs::kLanguageXkbAutoRepeatEnabled,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kLanguageXkbAutoRepeatDelay,
                             language_prefs::kXkbAutoRepeatDelayInMs,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kLanguageXkbAutoRepeatInterval,
                             language_prefs::kXkbAutoRepeatIntervalInMs,
                             PrefService::UNSYNCABLE_PREF);

  prefs->RegisterDictionaryPref(prefs::kLanguagePreferredVirtualKeyboard,
                                PrefService::SYNCABLE_PREF);

  // Screen lock default to off.
  prefs->RegisterBooleanPref(prefs::kEnableScreenLock,
                             false,
                             PrefService::SYNCABLE_PREF);

  // Mobile plan notifications default to on.
  prefs->RegisterBooleanPref(prefs::kShowPlanNotifications,
                             true,
                             PrefService::SYNCABLE_PREF);

  // 3G first-time usage promo will be shown at least once.
  prefs->RegisterBooleanPref(prefs::kShow3gPromoNotification,
                             true,
                             PrefService::UNSYNCABLE_PREF);

  // OAuth1 all access token and secret pair.
  prefs->RegisterStringPref(prefs::kOAuth1Token,
                            "",
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kOAuth1Secret,
                            "",
                            PrefService::UNSYNCABLE_PREF);
}

void Preferences::Init(PrefService* prefs) {
  tap_to_click_enabled_.Init(prefs::kTapToClickEnabled, prefs, this);
  accessibility_enabled_.Init(prefs::kAccessibilityEnabled, prefs, this);
  sensitivity_.Init(prefs::kTouchpadSensitivity, prefs, this);
  use_24hour_clock_.Init(prefs::kUse24HourClock, prefs, this);
  language_hotkey_next_engine_in_menu_.Init(
      prefs::kLanguageHotkeyNextEngineInMenu, prefs, this);
  language_hotkey_previous_engine_.Init(
      prefs::kLanguageHotkeyPreviousEngine, prefs, this);
  language_preferred_languages_.Init(prefs::kLanguagePreferredLanguages,
                                     prefs, this);
  language_preload_engines_.Init(prefs::kLanguagePreloadEngines, prefs, this);
  for (size_t i = 0; i < language_prefs::kNumChewingBooleanPrefs; ++i) {
    language_chewing_boolean_prefs_[i].Init(
        language_prefs::kChewingBooleanPrefs[i].pref_name, prefs, this);
  }
  for (size_t i = 0; i < language_prefs::kNumChewingMultipleChoicePrefs; ++i) {
    language_chewing_multiple_choice_prefs_[i].Init(
        language_prefs::kChewingMultipleChoicePrefs[i].pref_name, prefs, this);
  }
  language_chewing_hsu_sel_key_type_.Init(
      language_prefs::kChewingHsuSelKeyType.pref_name, prefs, this);
  for (size_t i = 0; i < language_prefs::kNumChewingIntegerPrefs; ++i) {
    language_chewing_integer_prefs_[i].Init(
        language_prefs::kChewingIntegerPrefs[i].pref_name, prefs, this);
  }
  language_hangul_keyboard_.Init(prefs::kLanguageHangulKeyboard, prefs, this);
  language_hangul_hanja_binding_keys_.Init(
      prefs::kLanguageHangulHanjaBindingKeys, prefs, this);
  for (size_t i = 0; i < language_prefs::kNumPinyinBooleanPrefs; ++i) {
    language_pinyin_boolean_prefs_[i].Init(
        language_prefs::kPinyinBooleanPrefs[i].pref_name, prefs, this);
  }
  for (size_t i = 0; i < language_prefs::kNumPinyinIntegerPrefs; ++i) {
    language_pinyin_int_prefs_[i].Init(
        language_prefs::kPinyinIntegerPrefs[i].pref_name, prefs, this);
  }
  language_pinyin_double_pinyin_schema_.Init(
      language_prefs::kPinyinDoublePinyinSchema.pref_name, prefs, this);
  for (size_t i = 0; i < language_prefs::kNumMozcBooleanPrefs; ++i) {
    language_mozc_boolean_prefs_[i].Init(
        language_prefs::kMozcBooleanPrefs[i].pref_name, prefs, this);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    language_mozc_multiple_choice_prefs_[i].Init(
        language_prefs::kMozcMultipleChoicePrefs[i].pref_name, prefs, this);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcIntegerPrefs; ++i) {
    language_mozc_integer_prefs_[i].Init(
        language_prefs::kMozcIntegerPrefs[i].pref_name, prefs, this);
  }
  language_xkb_remap_search_key_to_.Init(
      prefs::kLanguageXkbRemapSearchKeyTo, prefs, this);
  language_xkb_remap_control_key_to_.Init(
      prefs::kLanguageXkbRemapControlKeyTo, prefs, this);
  language_xkb_remap_alt_key_to_.Init(
      prefs::kLanguageXkbRemapAltKeyTo, prefs, this);
  language_xkb_auto_repeat_enabled_.Init(
      prefs::kLanguageXkbAutoRepeatEnabled, prefs, this);
  language_xkb_auto_repeat_delay_pref_.Init(
      prefs::kLanguageXkbAutoRepeatDelay, prefs, this);
  language_xkb_auto_repeat_interval_pref_.Init(
      prefs::kLanguageXkbAutoRepeatInterval, prefs, this);

  enable_screen_lock_.Init(prefs::kEnableScreenLock, prefs, this);

  // Initialize preferences to currently saved state.
  NotifyPrefChanged(NULL);

  // Initialize virtual keyboard settings to currently saved state.
  UpdateVirturalKeyboardPreference(prefs);

  // If a guest is logged in, initialize the prefs as if this is the first
  // login.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession)) {
    LoginUtils::Get()->SetFirstLoginPrefs(prefs);
  }
}

void Preferences::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED)
    NotifyPrefChanged(content::Details<std::string>(details).ptr());
}

void Preferences::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || *pref_name == prefs::kTapToClickEnabled) {
    bool enabled = tap_to_click_enabled_.GetValue();
    system::touchpad_settings::SetTapToClick(enabled);
    if (pref_name)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapToClick.Changed", enabled);
    else
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapToClick.Started", enabled);
  }
  if (!pref_name || *pref_name == prefs::kTouchpadSensitivity) {
    int sensitivity = sensitivity_.GetValue();
    system::touchpad_settings::SetSensitivity(sensitivity);
    if (pref_name) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Touchpad.Sensitivity.Changed", sensitivity, 1, 5, 5);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Touchpad.Sensitivity.Started", sensitivity, 1, 5, 5);
    }
  }

  // We don't handle prefs::kLanguageCurrentInputMethod and PreviousInputMethod
  // here.

  if (!pref_name || *pref_name == prefs::kLanguageHotkeyNextEngineInMenu) {
    SetLanguageConfigStringListAsCSV(
        language_prefs::kHotKeySectionName,
        language_prefs::kNextEngineInMenuConfigName,
        language_hotkey_next_engine_in_menu_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kLanguageHotkeyPreviousEngine) {
    SetLanguageConfigStringListAsCSV(
        language_prefs::kHotKeySectionName,
        language_prefs::kPreviousEngineConfigName,
        language_hotkey_previous_engine_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kLanguagePreferredLanguages) {
    // Unlike kLanguagePreloadEngines and some other input method
    // preferencs, we don't need to send this to ibus-daemon.
  }

  // Here, we set up the the modifier key mapping. This has to be done
  // before changing the current keyboard layout, so that the modifier key
  // preference is properly preserved. For this reason, we should do this
  // before setting preload engines, that could change the current
  // keyboard layout as needed.
  if (!pref_name || (*pref_name == prefs::kLanguageXkbRemapSearchKeyTo ||
                     *pref_name == prefs::kLanguageXkbRemapControlKeyTo ||
                     *pref_name == prefs::kLanguageXkbRemapAltKeyTo)) {
    UpdateModifierKeyMapping();
  }
  if (!pref_name || *pref_name == prefs::kLanguageXkbAutoRepeatEnabled) {
    const bool enabled = language_xkb_auto_repeat_enabled_.GetValue();
    input_method::XKeyboard::SetAutoRepeatEnabled(enabled);
  }
  if (!pref_name || ((*pref_name == prefs::kLanguageXkbAutoRepeatDelay) ||
                     (*pref_name == prefs::kLanguageXkbAutoRepeatInterval))) {
    UpdateAutoRepeatRate();
  }

  if (!pref_name || *pref_name == prefs::kLanguagePreloadEngines) {
    SetLanguageConfigStringListAsCSV(language_prefs::kGeneralSectionName,
                                     language_prefs::kPreloadEnginesConfigName,
                                     language_preload_engines_.GetValue());
  }
  for (size_t i = 0; i < language_prefs::kNumChewingBooleanPrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kChewingBooleanPrefs[i].pref_name) {
      SetLanguageConfigBoolean(
          language_prefs::kChewingSectionName,
          language_prefs::kChewingBooleanPrefs[i].ibus_config_name,
          language_chewing_boolean_prefs_[i].GetValue());
    }
  }
  for (size_t i = 0; i < language_prefs::kNumChewingMultipleChoicePrefs; ++i) {
    if (!pref_name ||
        *pref_name ==
        language_prefs::kChewingMultipleChoicePrefs[i].pref_name) {
      SetLanguageConfigString(
          language_prefs::kChewingSectionName,
          language_prefs::kChewingMultipleChoicePrefs[i].ibus_config_name,
          language_chewing_multiple_choice_prefs_[i].GetValue());
    }
  }
  if (!pref_name ||
      *pref_name == language_prefs::kChewingHsuSelKeyType.pref_name) {
    SetLanguageConfigInteger(
        language_prefs::kChewingSectionName,
        language_prefs::kChewingHsuSelKeyType.ibus_config_name,
        language_chewing_hsu_sel_key_type_.GetValue());
  }
  for (size_t i = 0; i < language_prefs::kNumChewingIntegerPrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kChewingIntegerPrefs[i].pref_name) {
      SetLanguageConfigInteger(
          language_prefs::kChewingSectionName,
          language_prefs::kChewingIntegerPrefs[i].ibus_config_name,
          language_chewing_integer_prefs_[i].GetValue());
    }
  }
  if (!pref_name ||
      *pref_name == prefs::kLanguageHangulKeyboard) {
    SetLanguageConfigString(language_prefs::kHangulSectionName,
                            language_prefs::kHangulKeyboardConfigName,
                            language_hangul_keyboard_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kLanguageHangulHanjaBindingKeys) {
    SetLanguageConfigString(language_prefs::kHangulSectionName,
                            language_prefs::kHangulHanjaBindingKeysConfigName,
                            language_hangul_hanja_binding_keys_.GetValue());
  }
  for (size_t i = 0; i < language_prefs::kNumPinyinBooleanPrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kPinyinBooleanPrefs[i].pref_name) {
      SetLanguageConfigBoolean(
          language_prefs::kPinyinSectionName,
          language_prefs::kPinyinBooleanPrefs[i].ibus_config_name,
          language_pinyin_boolean_prefs_[i].GetValue());
    }
  }
  for (size_t i = 0; i < language_prefs::kNumPinyinIntegerPrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kPinyinIntegerPrefs[i].pref_name) {
      SetLanguageConfigInteger(
          language_prefs::kPinyinSectionName,
          language_prefs::kPinyinIntegerPrefs[i].ibus_config_name,
          language_pinyin_int_prefs_[i].GetValue());
    }
  }
  if (!pref_name ||
      *pref_name == language_prefs::kPinyinDoublePinyinSchema.pref_name) {
    SetLanguageConfigInteger(
        language_prefs::kPinyinSectionName,
        language_prefs::kPinyinDoublePinyinSchema.ibus_config_name,
        language_pinyin_double_pinyin_schema_.GetValue());
  }
  for (size_t i = 0; i < language_prefs::kNumMozcBooleanPrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kMozcBooleanPrefs[i].pref_name) {
      SetLanguageConfigBoolean(
          language_prefs::kMozcSectionName,
          language_prefs::kMozcBooleanPrefs[i].ibus_config_name,
          language_mozc_boolean_prefs_[i].GetValue());
    }
  }
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kMozcMultipleChoicePrefs[i].pref_name) {
      SetLanguageConfigString(
          language_prefs::kMozcSectionName,
          language_prefs::kMozcMultipleChoicePrefs[i].ibus_config_name,
          language_mozc_multiple_choice_prefs_[i].GetValue());
    }
  }
  for (size_t i = 0; i < language_prefs::kNumMozcIntegerPrefs; ++i) {
    if (!pref_name ||
        *pref_name == language_prefs::kMozcIntegerPrefs[i].pref_name) {
      SetLanguageConfigInteger(
          language_prefs::kMozcSectionName,
          language_prefs::kMozcIntegerPrefs[i].ibus_config_name,
          language_mozc_integer_prefs_[i].GetValue());
    }
  }

  // Init or update power manager config.
  if (!pref_name || *pref_name == prefs::kEnableScreenLock) {
    system::screen_locker_settings::EnableScreenLock(
        enable_screen_lock_.GetValue());
  }
}

void Preferences::SetLanguageConfigBoolean(const char* section,
                                           const char* name,
                                           bool value) {
  input_method::ImeConfigValue config;
  config.type = input_method::ImeConfigValue::kValueTypeBool;
  config.bool_value = value;
  input_method::InputMethodManager::GetInstance()->
      SetImeConfig(section, name, config);
}

void Preferences::SetLanguageConfigInteger(const char* section,
                                           const char* name,
                                           int value) {
  input_method::ImeConfigValue config;
  config.type = input_method::ImeConfigValue::kValueTypeInt;
  config.int_value = value;
  input_method::InputMethodManager::GetInstance()->
      SetImeConfig(section, name, config);
}

void Preferences::SetLanguageConfigString(const char* section,
                                          const char* name,
                                          const std::string& value) {
  input_method::ImeConfigValue config;
  config.type = input_method::ImeConfigValue::kValueTypeString;
  config.string_value = value;
  input_method::InputMethodManager::GetInstance()->
      SetImeConfig(section, name, config);
}

void Preferences::SetLanguageConfigStringList(
    const char* section,
    const char* name,
    const std::vector<std::string>& values) {
  input_method::ImeConfigValue config;
  config.type = input_method::ImeConfigValue::kValueTypeStringList;
  for (size_t i = 0; i < values.size(); ++i)
    config.string_list_value.push_back(values[i]);

  input_method::InputMethodManager::GetInstance()->
      SetImeConfig(section, name, config);
}

void Preferences::SetLanguageConfigStringListAsCSV(const char* section,
                                                   const char* name,
                                                   const std::string& value) {
  VLOG(1) << "Setting " << name << " to '" << value << "'";

  std::vector<std::string> split_values;
  if (!value.empty())
    base::SplitString(value, ',', &split_values);

  // We should call the cros API even when |value| is empty, to disable default
  // config.
  SetLanguageConfigStringList(section, name, split_values);
}

void Preferences::UpdateModifierKeyMapping() {
  const int search_remap = language_xkb_remap_search_key_to_.GetValue();
  const int control_remap = language_xkb_remap_control_key_to_.GetValue();
  const int alt_remap = language_xkb_remap_alt_key_to_.GetValue();
  if ((search_remap < input_method::kNumModifierKeys) && (search_remap >= 0) &&
      (control_remap < input_method::kNumModifierKeys) &&
      (control_remap >= 0) &&
      (alt_remap < input_method::kNumModifierKeys) && (alt_remap >= 0)) {
    input_method::ModifierMap modifier_map;
    modifier_map.push_back(
        input_method::ModifierKeyPair(
            input_method::kSearchKey,
            input_method::ModifierKey(search_remap)));
    modifier_map.push_back(
        input_method::ModifierKeyPair(
            input_method::kLeftControlKey,
            input_method::ModifierKey(control_remap)));
    modifier_map.push_back(
        input_method::ModifierKeyPair(
            input_method::kLeftAltKey,
            input_method::ModifierKey(alt_remap)));
    input_method::InputMethodManager::GetInstance()->GetXKeyboard()->
        RemapModifierKeys(modifier_map);
  } else {
    LOG(ERROR) << "Failed to remap modifier keys. Unexpected value(s): "
               << search_remap << ", " << control_remap << ", " << alt_remap;
  }
}

void Preferences::UpdateAutoRepeatRate() {
  input_method::AutoRepeatRate rate;
  rate.initial_delay_in_ms = language_xkb_auto_repeat_delay_pref_.GetValue();
  rate.repeat_interval_in_ms =
      language_xkb_auto_repeat_interval_pref_.GetValue();
  DCHECK(rate.initial_delay_in_ms > 0);
  DCHECK(rate.repeat_interval_in_ms > 0);
  input_method::XKeyboard::SetAutoRepeatRate(rate);
}

void Preferences::UpdateVirturalKeyboardPreference(PrefService* prefs) {
  const DictionaryValue* virtual_keyboard_pref =
      prefs->GetDictionary(prefs::kLanguagePreferredVirtualKeyboard);
  DCHECK(virtual_keyboard_pref);

  input_method::InputMethodManager* input_method_manager =
      input_method::InputMethodManager::GetInstance();
  input_method_manager->ClearAllVirtualKeyboardPreferences();

  std::string url;
  std::vector<std::string> layouts_to_remove;
  for (DictionaryValue::key_iterator iter = virtual_keyboard_pref->begin_keys();
       iter != virtual_keyboard_pref->end_keys();
       ++iter) {
    const std::string& layout_id = *iter;  // e.g. "us", "handwriting-vk"
    if (!virtual_keyboard_pref->GetString(layout_id, &url))
      continue;
    if (!input_method_manager->SetVirtualKeyboardPreference(
            layout_id, GURL(url))) {
      // Either |layout_id| or |url| is invalid. Remove the key from |prefs|
      // later.
      layouts_to_remove.push_back(layout_id);
      LOG(ERROR) << "Removing invalid virtual keyboard pref: layout="
                 << layout_id;
    }
  }

  // Remove invalid prefs.
  DictionaryPrefUpdate updater(prefs, prefs::kLanguagePreferredVirtualKeyboard);
  DictionaryValue* pref_value = updater.Get();
  for (size_t i = 0; i < layouts_to_remove.size(); ++i) {
    pref_value->RemoveWithoutPathExpansion(layouts_to_remove[i], NULL);
  }
}

}  // namespace chromeos
