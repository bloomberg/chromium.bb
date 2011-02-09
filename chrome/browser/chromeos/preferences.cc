// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/preferences.h"

#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/cros/touchpad_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "unicode/timezone.h"

namespace chromeos {

static const char kFallbackInputMethodLocale[] = "en-US";

Preferences::Preferences() {}

Preferences::~Preferences() {}

// static
void Preferences::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kTapToClickEnabled, true);
  prefs->RegisterBooleanPref(prefs::kLabsMediaplayerEnabled, false);
  prefs->RegisterBooleanPref(prefs::kLabsAdvancedFilesystemEnabled, false);
  // Check if the accessibility pref is already registered, which can happen
  // in WizardController::RegisterPrefs. We still want to try to register
  // the pref here in case of Chrome/Linux with ChromeOS=1.
  if (prefs->FindPreference(prefs::kAccessibilityEnabled) == NULL) {
    prefs->RegisterBooleanPref(prefs::kAccessibilityEnabled, false);
  }
  prefs->RegisterIntegerPref(prefs::kTouchpadSensitivity, 3);
  prefs->RegisterStringPref(prefs::kLanguageCurrentInputMethod, "");
  prefs->RegisterStringPref(prefs::kLanguagePreviousInputMethod, "");
  prefs->RegisterStringPref(prefs::kLanguageHotkeyNextEngineInMenu,
                            language_prefs::kHotkeyNextEngineInMenu);
  prefs->RegisterStringPref(prefs::kLanguageHotkeyPreviousEngine,
                            language_prefs::kHotkeyPreviousEngine);
  prefs->RegisterStringPref(prefs::kLanguagePreferredLanguages,
                            kFallbackInputMethodLocale);
  prefs->RegisterStringPref(
      prefs::kLanguagePreloadEngines,
      input_method::GetHardwareInputMethodDescriptor().id);
  for (size_t i = 0; i < language_prefs::kNumChewingBooleanPrefs; ++i) {
    prefs->RegisterBooleanPref(
        language_prefs::kChewingBooleanPrefs[i].pref_name,
        language_prefs::kChewingBooleanPrefs[i].default_pref_value);
  }
  for (size_t i = 0; i < language_prefs::kNumChewingMultipleChoicePrefs; ++i) {
    prefs->RegisterStringPref(
        language_prefs::kChewingMultipleChoicePrefs[i].pref_name,
        language_prefs::kChewingMultipleChoicePrefs[i].default_pref_value);
  }
  prefs->RegisterIntegerPref(
      language_prefs::kChewingHsuSelKeyType.pref_name,
      language_prefs::kChewingHsuSelKeyType.default_pref_value);

  for (size_t i = 0; i < language_prefs::kNumChewingIntegerPrefs; ++i) {
    prefs->RegisterIntegerPref(
        language_prefs::kChewingIntegerPrefs[i].pref_name,
        language_prefs::kChewingIntegerPrefs[i].default_pref_value);
  }
  prefs->RegisterStringPref(
      prefs::kLanguageHangulKeyboard,
      language_prefs::kHangulKeyboardNameIDPairs[0].keyboard_id);
  prefs->RegisterStringPref(prefs::kLanguageHangulHanjaKeys,
                            language_prefs::kHangulHanjaKeys);
  for (size_t i = 0; i < language_prefs::kNumPinyinBooleanPrefs; ++i) {
    prefs->RegisterBooleanPref(
        language_prefs::kPinyinBooleanPrefs[i].pref_name,
        language_prefs::kPinyinBooleanPrefs[i].default_pref_value);
  }
  for (size_t i = 0; i < language_prefs::kNumPinyinIntegerPrefs; ++i) {
    prefs->RegisterIntegerPref(
        language_prefs::kPinyinIntegerPrefs[i].pref_name,
        language_prefs::kPinyinIntegerPrefs[i].default_pref_value);
  }
  prefs->RegisterIntegerPref(
      language_prefs::kPinyinDoublePinyinSchema.pref_name,
      language_prefs::kPinyinDoublePinyinSchema.default_pref_value);

  for (size_t i = 0; i < language_prefs::kNumMozcBooleanPrefs; ++i) {
    prefs->RegisterBooleanPref(
        language_prefs::kMozcBooleanPrefs[i].pref_name,
        language_prefs::kMozcBooleanPrefs[i].default_pref_value);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    prefs->RegisterStringPref(
        language_prefs::kMozcMultipleChoicePrefs[i].pref_name,
        language_prefs::kMozcMultipleChoicePrefs[i].default_pref_value);
  }
  for (size_t i = 0; i < language_prefs::kNumMozcIntegerPrefs; ++i) {
    prefs->RegisterIntegerPref(
        language_prefs::kMozcIntegerPrefs[i].pref_name,
        language_prefs::kMozcIntegerPrefs[i].default_pref_value);
  }
  prefs->RegisterIntegerPref(prefs::kLanguageXkbRemapSearchKeyTo, kSearchKey);
  prefs->RegisterIntegerPref(prefs::kLanguageXkbRemapControlKeyTo,
                             kLeftControlKey);
  prefs->RegisterIntegerPref(prefs::kLanguageXkbRemapAltKeyTo, kLeftAltKey);
  prefs->RegisterBooleanPref(prefs::kLanguageXkbAutoRepeatEnabled, true);
  prefs->RegisterIntegerPref(prefs::kLanguageXkbAutoRepeatDelay,
                             language_prefs::kXkbAutoRepeatDelayInMs);
  prefs->RegisterIntegerPref(prefs::kLanguageXkbAutoRepeatInterval,
                             language_prefs::kXkbAutoRepeatIntervalInMs);

  // Screen lock default to off.
  prefs->RegisterBooleanPref(prefs::kEnableScreenLock, false);

  // Mobile plan notifications default to on.
  prefs->RegisterBooleanPref(prefs::kShowPlanNotifications, true);
}

void Preferences::Init(PrefService* prefs) {
  tap_to_click_enabled_.Init(prefs::kTapToClickEnabled, prefs, this);
  accessibility_enabled_.Init(prefs::kAccessibilityEnabled, prefs, this);
  sensitivity_.Init(prefs::kTouchpadSensitivity, prefs, this);
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
  language_hangul_hanja_keys_.Init(
      prefs::kLanguageHangulHanjaKeys, prefs, this);
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

  // Initialize touchpad settings to what's saved in user preferences.
  NotifyPrefChanged(NULL);

  // If a guest is logged in, initialize the prefs as if this is the first
  // login.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession)) {
    LoginUtils::Get()->SetFirstLoginPrefs(prefs);
  }
}

void Preferences::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED)
    NotifyPrefChanged(Details<std::string>(details).ptr());
}

void Preferences::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || *pref_name == prefs::kTapToClickEnabled) {
    CrosLibrary::Get()->GetTouchpadLibrary()->SetTapToClick(
        tap_to_click_enabled_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kTouchpadSensitivity) {
    CrosLibrary::Get()->GetTouchpadLibrary()->SetSensitivity(
        sensitivity_.GetValue());
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
    CrosLibrary::Get()->GetKeyboardLibrary()->SetAutoRepeatEnabled(enabled);
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
  if (!pref_name || *pref_name == prefs::kLanguageHangulHanjaKeys) {
    SetLanguageConfigString(language_prefs::kHangulSectionName,
                            language_prefs::kHangulHanjaKeysConfigName,
                            language_hangul_hanja_keys_.GetValue());
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
    CrosLibrary::Get()->GetPowerLibrary()->EnableScreenLock(
        enable_screen_lock_.GetValue());
  }
}

void Preferences::SetLanguageConfigBoolean(const char* section,
                                           const char* name,
                                           bool value) {
  ImeConfigValue config;
  config.type = ImeConfigValue::kValueTypeBool;
  config.bool_value = value;
  CrosLibrary::Get()->GetInputMethodLibrary()->
      SetImeConfig(section, name, config);
}

void Preferences::SetLanguageConfigInteger(const char* section,
                                           const char* name,
                                           int value) {
  ImeConfigValue config;
  config.type = ImeConfigValue::kValueTypeInt;
  config.int_value = value;
  CrosLibrary::Get()->GetInputMethodLibrary()->
      SetImeConfig(section, name, config);
}

void Preferences::SetLanguageConfigString(const char* section,
                                          const char* name,
                                          const std::string& value) {
  ImeConfigValue config;
  config.type = ImeConfigValue::kValueTypeString;
  config.string_value = value;
  CrosLibrary::Get()->GetInputMethodLibrary()->
      SetImeConfig(section, name, config);
}

void Preferences::SetLanguageConfigStringList(
    const char* section,
    const char* name,
    const std::vector<std::string>& values) {
  ImeConfigValue config;
  config.type = ImeConfigValue::kValueTypeStringList;
  for (size_t i = 0; i < values.size(); ++i)
    config.string_list_value.push_back(values[i]);

  CrosLibrary::Get()->GetInputMethodLibrary()->
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
  if ((search_remap < kNumModifierKeys) && (search_remap >= 0) &&
      (control_remap < kNumModifierKeys) && (control_remap >= 0) &&
      (alt_remap < kNumModifierKeys) && (alt_remap >= 0)) {
    chromeos::ModifierMap modifier_map;
    modifier_map.push_back(
        ModifierKeyPair(kSearchKey, ModifierKey(search_remap)));
    modifier_map.push_back(
        ModifierKeyPair(kLeftControlKey, ModifierKey(control_remap)));
    modifier_map.push_back(
        ModifierKeyPair(kLeftAltKey, ModifierKey(alt_remap)));
    CrosLibrary::Get()->GetKeyboardLibrary()->RemapModifierKeys(modifier_map);
  } else {
    LOG(ERROR) << "Failed to remap modifier keys. Unexpected value(s): "
               << search_remap << ", " << control_remap << ", " << alt_remap;
  }
}

void Preferences::UpdateAutoRepeatRate() {
  AutoRepeatRate rate;
  rate.initial_delay_in_ms = language_xkb_auto_repeat_delay_pref_.GetValue();
  rate.repeat_interval_in_ms =
      language_xkb_auto_repeat_interval_pref_.GetValue();
  DCHECK(rate.initial_delay_in_ms > 0);
  DCHECK(rate.repeat_interval_in_ms > 0);
  CrosLibrary::Get()->GetKeyboardLibrary()->SetAutoRepeatRate(rate);
}

}  // namespace chromeos
