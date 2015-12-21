// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_settings_bridge_impl.h"

#include <algorithm>

#include "base/json/json_writer.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/arc/common/settings.mojom.h"

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

ArcSettingsBridgeImpl::~ArcSettingsBridgeImpl() {
  ArcBridgeService* bridge_service = ArcBridgeService::Get();
  DCHECK(bridge_service);
  bridge_service->RemoveObserver(this);
}

void ArcSettingsBridgeImpl::StartObservingBridgeServiceChanges() {
  ArcBridgeService* bridge_service = ArcBridgeService::Get();
  DCHECK(bridge_service);
  bridge_service->AddObserver(this);
}

void ArcSettingsBridgeImpl::StartObservingPrefChanges() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  registrar_.Init(profile->GetPrefs());

  AddPrefToObserve(prefs::kWebKitDefaultFixedFontSize);
  AddPrefToObserve(prefs::kWebKitDefaultFontSize);
  AddPrefToObserve(prefs::kWebKitMinimumFontSize);
  AddPrefToObserve(prefs::kAccessibilitySpokenFeedbackEnabled);
}

void ArcSettingsBridgeImpl::SyncAllPrefs() const {
  SyncFontSize();
  SyncSpokenFeedbackEnabled();
}

void ArcSettingsBridgeImpl::StopObservingPrefChanges() {
  registrar_.RemoveAll();
}

void ArcSettingsBridgeImpl::AddPrefToObserve(const std::string& pref_name) {
  registrar_.Add(pref_name, base::Bind(&ArcSettingsBridgeImpl::OnPrefChanged,
                                       base::Unretained(this)));
}

void ArcSettingsBridgeImpl::OnPrefChanged(const std::string& pref_name) const {
  if (pref_name == prefs::kAccessibilitySpokenFeedbackEnabled) {
    SyncSpokenFeedbackEnabled();
  } else if (pref_name == prefs::kWebKitDefaultFixedFontSize ||
             pref_name == prefs::kWebKitDefaultFontSize ||
             pref_name == prefs::kWebKitMinimumFontSize) {
    SyncFontSize();
  } else {
    LOG(ERROR) << "Unknown pref changed.";
  }
}

void ArcSettingsBridgeImpl::OnStateChanged(ArcBridgeService::State state) {
  // ArcBridgeService::State::READY is emitted before ArcSettings app is ready
  // to send broadcasts.  Instead we wait for the SettingsInstance to be ready.
  if (state == ArcBridgeService::State::STOPPING) {
    StopObservingPrefChanges();
  }
}

void ArcSettingsBridgeImpl::OnSettingsInstanceReady() {
  StartObservingPrefChanges();
  SyncAllPrefs();
}

void ArcSettingsBridgeImpl::SyncSpokenFeedbackEnabled() const {
  const PrefService::Preference* pref = registrar_.prefs()->FindPreference(
      prefs::kAccessibilitySpokenFeedbackEnabled);
  DCHECK(pref);
  bool enabled = false;
  bool value_exists = pref->GetValue()->GetAsBoolean(&enabled);
  DCHECK(value_exists);
  base::DictionaryValue extras;
  extras.SetBoolean("enabled", enabled);
  SendSettingsBroadcast("org.chromium.arc.settings.SET_SPOKEN_FEEDBACK_ENABLED",
                        extras);
}

int ArcSettingsBridgeImpl::GetIntegerPref(const std::string& pref_name) const {
  const PrefService::Preference* pref =
      registrar_.prefs()->FindPreference(pref_name);
  DCHECK(pref);
  int val = -1;
  bool value_exists = pref->GetValue()->GetAsInteger(&val);
  DCHECK(value_exists);
  return val;
}

void ArcSettingsBridgeImpl::SyncFontSize() const {
  int default_size = GetIntegerPref(prefs::kWebKitDefaultFontSize);
  int default_fixed_size = GetIntegerPref(prefs::kWebKitDefaultFixedFontSize);
  int minimum_size = GetIntegerPref(prefs::kWebKitMinimumFontSize);

  double android_scale = fontsizes::ConvertFontSizeChromeToAndroid(
      default_size, default_fixed_size, minimum_size);

  base::DictionaryValue extras;
  extras.SetDouble("scale", android_scale);
  SendSettingsBroadcast("org.chromium.arc.settings.SET_FONT_SCALE", extras);
}

void ArcSettingsBridgeImpl::SendSettingsBroadcast(
    const std::string& action,
    const base::DictionaryValue& extras) const {
  ArcBridgeService* bridge_service = ArcBridgeService::Get();
  if (!bridge_service ||
      bridge_service->state() != ArcBridgeService::State::READY) {
    LOG(ERROR) << "Bridge service is not ready.";
    return;
  }

  std::string extras_json;
  bool write_success = base::JSONWriter::Write(extras, &extras_json);
  DCHECK(write_success);
  bridge_service->settings_instance()->SendBroadcast(
      action, "org.chromium.arc.settings",
      "org.chromium.arc.settings.SettingsReceiver", extras_json);
}

}  // namespace arc
