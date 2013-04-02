// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/kiosk_app_local_settings.h"

#include <set>

#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"

namespace chromeos {

KioskAppLocalSettings::KioskAppLocalSettings(
    const NotifyObserversCallback& notify_cb)
    : CrosSettingsProvider(notify_cb),
      default_auto_launch_(""),
      default_disable_bailout_shortcut_(false) {
  // Local state could be NULL in test.
  if (g_browser_process->local_state())
    ReadApps();
}

KioskAppLocalSettings::~KioskAppLocalSettings() {}

void KioskAppLocalSettings::ReadApps() {
  apps_.Clear();

  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dict =
      local_state->GetDictionary(KioskAppManager::kKioskDictionaryName);
  const base::DictionaryValue* apps_dict;
  if (!dict->GetDictionary(KioskAppManager::kKeyApps, &apps_dict))
    return;

  for (base::DictionaryValue::Iterator it(*apps_dict);
       !it.IsAtEnd();
       it.Advance()) {
    apps_.AppendString(it.key());
  }
}

void KioskAppLocalSettings::WriteApps(const base::Value& value) {
  std::set<std::string> old_apps;
  for (base::ListValue::const_iterator it = apps_.begin();
       it != apps_.end();
       ++it) {
    std::string app;
    CHECK((*it)->GetAsString(&app));
    old_apps.insert(app);
  }

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(local_state,
                                   KioskAppManager::kKioskDictionaryName);
  base::DictionaryValue* apps_dict;
  if (!dict_update->GetDictionary(KioskAppManager::kKeyApps, &apps_dict)) {
    apps_dict = new base::DictionaryValue;
    dict_update->Set(KioskAppManager::kKeyApps, apps_dict);
  }

  const base::ListValue* list_value;
  CHECK(value.GetAsList(&list_value));

  // Updates "apps" dictionary and |old_apps| set.
  for (base::ListValue::const_iterator new_it = list_value->begin();
       new_it != list_value->end();
       ++new_it) {
    std::string app;
    CHECK((*new_it)->GetAsString(&app));

    std::set<std::string>::iterator old_it = old_apps.find(app);
    if (old_it != old_apps.end())
      old_apps.erase(old_it);
    else
      apps_dict->Set(app, new base::DictionaryValue);
  }

  const std::string auto_launch_key =
      std::string(kKioskAutoLaunch).substr(kKioskAppSettingsPrefixLength);
  std::string auto_launch;
  dict_update->GetString(auto_launch_key, &auto_launch);
  bool reset_auto_launch = false;

  // Removes remaining |old_apps|.
  for (std::set<std::string>::iterator it = old_apps.begin();
       it != old_apps.end();
       ++it) {
    if (*it == auto_launch)
      reset_auto_launch = true;

    apps_dict->Remove(*it, NULL);
  }

  if (reset_auto_launch) {
    dict_update->Remove(auto_launch_key, NULL);
    NotifyObservers(kKioskAutoLaunch);
  }

  // Updates cached value.
  ReadApps();
}

bool KioskAppLocalSettings::IsEligibleAutoLaunchAppId(
    const base::Value& value) const {
  if (apps_.Find(value) != apps_.end())
    return true;

  // Empty string means no auto launch app and considered as an acceptable
  // value for auto launch setting.
  const base::StringValue empty("");
  return value.Equals(&empty);
}

const base::Value* KioskAppLocalSettings::Get(const std::string& path) const {
  DCHECK(HandlesSetting(path));

  if (path == kKioskApps)
    return &apps_;

  if (path == kKioskAutoLaunch ||
      path == kKioskDisableBailoutShortcut) {
    PrefService* local_state = g_browser_process->local_state();
    const base::DictionaryValue* dict =
        local_state->GetDictionary(KioskAppManager::kKioskDictionaryName);

    const base::Value* value;
    if (dict->Get(path.substr(kKioskAppSettingsPrefixLength), &value))
      return value;
  }

  if (path == kKioskAutoLaunch)
    return &default_auto_launch_;
  if (path == kKioskDisableBailoutShortcut)
    return &default_disable_bailout_shortcut_;

  NOTREACHED() << "Try to get unknown kiosk app setting " << path;
  return NULL;
}

CrosSettingsProvider::TrustedStatus KioskAppLocalSettings::PrepareTrustedValues(
    const base::Closure& callback) {
  return TRUSTED;
}

bool KioskAppLocalSettings::HandlesSetting(const std::string& path) const {
  return StartsWithASCII(path, kKioskAppSettingsPrefix, true);
}

void KioskAppLocalSettings::DoSet(const std::string& path,
                                  const base::Value& in_value) {
  if (path == kKioskApps) {
    WriteApps(in_value);
  } else if (path == kKioskAutoLaunch ||
             path == kKioskDisableBailoutShortcut) {
    if (path == kKioskAutoLaunch && !IsEligibleAutoLaunchAppId(in_value))
      return;

    PrefService* local_state = g_browser_process->local_state();
    DictionaryPrefUpdate dict_update(local_state,
                                     KioskAppManager::kKioskDictionaryName);
    dict_update->Set(path.substr(kKioskAppSettingsPrefixLength),
                     in_value.DeepCopy());
  } else {
    NOTREACHED() << "Try to set unknown kiosk app setting " << path;
    return;
  }

  NotifyObservers(path);
}

}  // namespace chromeos
