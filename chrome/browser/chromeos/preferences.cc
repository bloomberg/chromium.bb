// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/preferences.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/language_library.h"
#include "chrome/browser/chromeos/cros/synaptics_library.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "unicode/timezone.h"

namespace {

// Section and config names for the IBus configuration daemon.
const char kGeneralSectionName[] = "general";
const char kUseGlobalEngineConfigName[] = "use_global_engine";
const char kHangulSectionName[] = "engine/Hangul";
const char kHangulKeyboardConfigName[] = "HangulKeyboard";

}  // namespace

namespace chromeos {

// static
void Preferences::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kTimeZone, L"US/Pacific");
  prefs->RegisterBooleanPref(prefs::kTapToClickEnabled, false);
  prefs->RegisterBooleanPref(prefs::kVertEdgeScrollEnabled, false);
  prefs->RegisterIntegerPref(prefs::kTouchpadSpeedFactor, 5);
  prefs->RegisterIntegerPref(prefs::kTouchpadSensitivity, 5);
  prefs->RegisterBooleanPref(prefs::kLanguageUseGlobalEngine, true);
  prefs->RegisterStringPref(prefs::kLanguagePreloadEngines,
                            kDefaultPreloadEngine);
  prefs->RegisterStringPref(prefs::kLanguageHangulKeyboard,
                            kHangulKeyboardNameIDPairs[0].keyboard_id);
}

void Preferences::Init(PrefService* prefs) {
  timezone_.Init(prefs::kTimeZone, prefs, this);
  tap_to_click_enabled_.Init(prefs::kTapToClickEnabled, prefs, this);
  vert_edge_scroll_enabled_.Init(prefs::kVertEdgeScrollEnabled, prefs, this);
  speed_factor_.Init(prefs::kTouchpadSpeedFactor, prefs, this);
  sensitivity_.Init(prefs::kTouchpadSensitivity, prefs, this);
  language_use_global_engine_.Init(
      prefs::kLanguageUseGlobalEngine, prefs, this);
  language_preload_engines_.Init(prefs::kLanguagePreloadEngines, prefs, this);
  language_hangul_keyboard_.Init(prefs::kLanguageHangulKeyboard, prefs, this);

  // Initialize touchpad settings to what's saved in user preferences.
  NotifyPrefChanged(NULL);
}

void Preferences::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED)
    NotifyPrefChanged(Details<std::wstring>(details).ptr());
}

void Preferences::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kTimeZone)
    SetTimeZone(timezone_.GetValue());
  if (!pref_name || *pref_name == prefs::kTapToClickEnabled)
    CrosLibrary::Get()->GetSynapticsLibrary()->SetBoolParameter(
        PARAM_BOOL_TAP_TO_CLICK,
        tap_to_click_enabled_.GetValue());
  if (!pref_name || *pref_name == prefs::kVertEdgeScrollEnabled)
    CrosLibrary::Get()->GetSynapticsLibrary()->SetBoolParameter(
        PARAM_BOOL_VERTICAL_EDGE_SCROLLING,
        vert_edge_scroll_enabled_.GetValue());
  if (!pref_name || *pref_name == prefs::kTouchpadSpeedFactor)
    CrosLibrary::Get()->GetSynapticsLibrary()->SetRangeParameter(
        PARAM_RANGE_SPEED_SENSITIVITY,
        speed_factor_.GetValue());
  if (!pref_name || *pref_name == prefs::kTouchpadSensitivity)
    CrosLibrary::Get()->GetSynapticsLibrary()->SetRangeParameter(
          PARAM_RANGE_TOUCH_SENSITIVITY,
          sensitivity_.GetValue());
  if (!pref_name || *pref_name == prefs::kLanguageUseGlobalEngine)
    SetLanguageConfigBoolean(kGeneralSectionName, kUseGlobalEngineConfigName,
                             language_use_global_engine_.GetValue());
  if (!pref_name || *pref_name == prefs::kLanguagePreloadEngines)
    SetPreloadEngines(language_preload_engines_.GetValue());
  if (!pref_name || *pref_name == prefs::kLanguageHangulKeyboard)
    SetLanguageConfigString(kHangulSectionName, kHangulKeyboardConfigName,
                            language_hangul_keyboard_.GetValue());
}

void Preferences::SetTimeZone(const std::wstring& id) {
  icu::TimeZone* timezone = icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8(WideToASCII(id)));
  icu::TimeZone::adoptDefault(timezone);
}

void Preferences::SetLanguageConfigBoolean(const char* section,
                                           const char* name,
                                           bool value) {
  ImeConfigValue config;
  config.type = ImeConfigValue::kValueTypeBool;
  config.bool_value = value;
  CrosLibrary::Get()->GetLanguageLibrary()->SetImeConfig(section, name, config);
}

void Preferences::SetLanguageConfigString(const char* section,
                                          const char* name,
                                          const std::wstring& value) {
  ImeConfigValue config;
  config.type = ImeConfigValue::kValueTypeString;
  config.string_value = WideToUTF8(value);
  CrosLibrary::Get()->GetLanguageLibrary()->SetImeConfig(section, name, config);
}

void Preferences::SetPreloadEngines(const std::wstring& value) {
  // TODO(yusukes): might be better to change the cros API signature so it
  // could accept the comma separated |value| as-is.

  LanguageLibrary* library = CrosLibrary::Get()->GetLanguageLibrary();
  std::vector<std::wstring> engine_ids;
  SplitString(value, L',', &engine_ids);
  LOG(INFO) << "Setting preload_engines to '" << value << "'";

  // Activate languages in |value|.
  for (size_t i = 0; i < engine_ids.size(); ++i) {
    library->SetLanguageActivated(
        LANGUAGE_CATEGORY_IME, WideToUTF8(engine_ids[i]), true);
  }

  // Deactivate languages that are currently active, but are not in |value|.
  const std::set<std::wstring> id_set(engine_ids.begin(), engine_ids.end());
  scoped_ptr<InputLanguageList> active_engines(library->GetActiveLanguages());
  for (size_t i = 0; i < active_engines->size(); ++i) {
    const InputLanguage& active_engine = active_engines->at(i);
    if (id_set.count(UTF8ToWide(active_engine.id)) == 0) {
      library->SetLanguageActivated(
          active_engine.category, active_engine.id, false);
    }
  }
}

}  // namespace chromeos
