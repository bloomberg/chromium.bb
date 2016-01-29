// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/settings_bridge.h"

#include <algorithm>

#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"

using ::chromeos::system::TimezoneSettings;

namespace arc {

namespace fontsizes {

double ConvertFontSizeChromeToAndroid(int default_size,
                                      int default_fixed_size,
                                      int minimum_size) {
  // kWebKitDefaultFixedFontSize is automatically set to be 3 pixels smaller
  // than kWebKitDefaultFontSize when Chrome's settings page's main font
  // dropdown control is adjusted.  If the user specifically sets a higher
  // fixed font size we will want to take into account the adjustment.
  default_fixed_size += 3;
  int max_chrome_size =
      std::max(std::max(default_fixed_size, default_size), minimum_size);

  double android_scale = kAndroidFontScaleSmall;
  if (max_chrome_size >= kChromeFontSizeVeryLarge) {
    android_scale = kAndroidFontScaleHuge;
  } else if (max_chrome_size >= kChromeFontSizeLarge) {
    android_scale = kAndroidFontScaleLarge;
  } else if (max_chrome_size >= kChromeFontSizeNormal) {
    android_scale = kAndroidFontScaleNormal;
  }

  return android_scale;
}

}  // namespace fontsizes

SettingsBridge::SettingsBridge(SettingsBridge::Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  StartObservingSettingsChanges();
  SyncAllPrefs();
}

SettingsBridge::~SettingsBridge() {
  StopObservingSettingsChanges();
}

void SettingsBridge::StartObservingSettingsChanges() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  registrar_.Init(profile->GetPrefs());

  AddPrefToObserve(prefs::kWebKitDefaultFixedFontSize);
  AddPrefToObserve(prefs::kWebKitDefaultFontSize);
  AddPrefToObserve(prefs::kWebKitMinimumFontSize);
  AddPrefToObserve(prefs::kAccessibilitySpokenFeedbackEnabled);
  AddPrefToObserve(prefs::kUse24HourClock);

  TimezoneSettings::GetInstance()->AddObserver(this);
}

void SettingsBridge::SyncAllPrefs() const {
  SyncFontSize();
  SyncLocale();
  SyncSpokenFeedbackEnabled();
  SyncTimeZone();
  SyncUse24HourClock();
}

void SettingsBridge::StopObservingSettingsChanges() {
  registrar_.RemoveAll();

  TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void SettingsBridge::AddPrefToObserve(const std::string& pref_name) {
  registrar_.Add(pref_name, base::Bind(&SettingsBridge::OnPrefChanged,
                                       base::Unretained(this)));
}

void SettingsBridge::OnPrefChanged(const std::string& pref_name) const {
  if (pref_name == prefs::kAccessibilitySpokenFeedbackEnabled) {
    SyncSpokenFeedbackEnabled();
  } else if (pref_name == prefs::kWebKitDefaultFixedFontSize ||
             pref_name == prefs::kWebKitDefaultFontSize ||
             pref_name == prefs::kWebKitMinimumFontSize) {
    SyncFontSize();
  } else if (pref_name == prefs::kUse24HourClock) {
    SyncUse24HourClock();
  } else {
    LOG(ERROR) << "Unknown pref changed.";
  }
}

void SettingsBridge::TimezoneChanged(const icu::TimeZone& timezone) {
  SyncTimeZone();
}

int SettingsBridge::GetIntegerPref(const std::string& pref_name) const {
  const PrefService::Preference* pref =
      registrar_.prefs()->FindPreference(pref_name);
  DCHECK(pref);
  int val = -1;
  bool value_exists = pref->GetValue()->GetAsInteger(&val);
  DCHECK(value_exists);
  return val;
}

void SettingsBridge::SyncFontSize() const {
  int default_size = GetIntegerPref(prefs::kWebKitDefaultFontSize);
  int default_fixed_size = GetIntegerPref(prefs::kWebKitDefaultFixedFontSize);
  int minimum_size = GetIntegerPref(prefs::kWebKitMinimumFontSize);

  double android_scale = fontsizes::ConvertFontSizeChromeToAndroid(
      default_size, default_fixed_size, minimum_size);

  base::DictionaryValue extras;
  extras.SetDouble("scale", android_scale);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_FONT_SCALE",
                        extras);
}

void SettingsBridge::SyncSpokenFeedbackEnabled() const {
  const PrefService::Preference* pref = registrar_.prefs()->FindPreference(
      prefs::kAccessibilitySpokenFeedbackEnabled);
  DCHECK(pref);
  bool enabled = false;
  bool value_exists = pref->GetValue()->GetAsBoolean(&enabled);
  DCHECK(value_exists);
  base::DictionaryValue extras;
  extras.SetBoolean("enabled", enabled);
  SendSettingsBroadcast(
      "org.chromium.arc.intent_helper.SET_SPOKEN_FEEDBACK_ENABLED", extras);
}

void SettingsBridge::SyncLocale() const {
  const PrefService::Preference* pref =
      registrar_.prefs()->FindPreference(prefs::kApplicationLocale);
  DCHECK(pref);
  std::string locale;
  bool value_exists = pref->GetValue()->GetAsString(&locale);
  DCHECK(value_exists);
  base::DictionaryValue extras;
  extras.SetString("locale", locale);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_LOCALE", extras);
}

void SettingsBridge::SyncTimeZone() const {
  TimezoneSettings* timezone_settings = TimezoneSettings::GetInstance();
  base::string16 timezoneID = timezone_settings->GetCurrentTimezoneID();
  base::DictionaryValue extras;
  extras.SetString("olsonTimeZone", timezoneID);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_TIME_ZONE", extras);
}

void SettingsBridge::SyncUse24HourClock() const {
  const PrefService::Preference* pref =
      registrar_.prefs()->FindPreference(prefs::kUse24HourClock);
  DCHECK(pref);
  bool use24HourClock = false;
  bool value_exists = pref->GetValue()->GetAsBoolean(&use24HourClock);
  DCHECK(value_exists);
  base::DictionaryValue extras;
  extras.SetBoolean("use24HourClock", use24HourClock);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_USE_24_HOUR_CLOCK",
                        extras);
}

void SettingsBridge::SendSettingsBroadcast(
    const std::string& action,
    const base::DictionaryValue& extras) const {
  delegate_->OnBroadcastNeeded(action, extras);
}

}  // namespace arc
