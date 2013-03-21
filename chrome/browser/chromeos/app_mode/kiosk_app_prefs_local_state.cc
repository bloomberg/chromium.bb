// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_prefs_local_state.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_prefs_observer.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"

namespace chromeos {

// Keys for local state data. See sample layout in KioskAppManager.
static const char kKeyAutoLaunch[] = "auto_launch";

std::string KioskAppPrefsLocalState::GetAutoLaunchApp() const {
  const base::DictionaryValue* dict =
      local_state_->GetDictionary(KioskAppManager::kKioskDictionaryName);

  std::string startup_app;
  if (dict->GetString(kKeyAutoLaunch, &startup_app))
    return startup_app;

  return std::string();
}

void KioskAppPrefsLocalState::SetAutoLaunchApp(const std::string& app_id) {
  if ((!app_id.empty() && !HasApp(app_id)) || GetAutoLaunchApp() == app_id)
    return;

  DictionaryPrefUpdate dict_update(local_state_,
                                   KioskAppManager::kKioskDictionaryName);
  dict_update->SetString(kKeyAutoLaunch, app_id);

  FOR_EACH_OBSERVER(KioskAppPrefsObserver,
                    observers_,
                    OnKioskAutoLaunchAppChanged());
}

void KioskAppPrefsLocalState::GetApps(AppIds* app_ids) const {
  const base::DictionaryValue* dict =
      local_state_->GetDictionary(KioskAppManager::kKioskDictionaryName);

  const base::DictionaryValue* apps_dict;
  if (dict->GetDictionary(KioskAppManager::kKeyApps, &apps_dict)) {
    for (base::DictionaryValue::Iterator it(*apps_dict);
         !it.IsAtEnd();
         it.Advance()) {
      app_ids->push_back(it.key());
    }
  }
}

void KioskAppPrefsLocalState::AddApp(const std::string& app_id) {
  if (HasApp(app_id))
    return;

  DictionaryPrefUpdate dict_update(local_state_,
                                   KioskAppManager::kKioskDictionaryName);

  std::string app_key = std::string(KioskAppManager::kKeyApps) + '.' + app_id;
  dict_update->Set(app_key, new base::DictionaryValue);

  FOR_EACH_OBSERVER(KioskAppPrefsObserver,
                    observers_,
                    OnKioskAppsChanged());
  return;
}

void KioskAppPrefsLocalState::RemoveApp(const std::string& app_id) {
  if (!HasApp(app_id))
    return;

  if (GetAutoLaunchApp() == app_id)
    SetAutoLaunchApp(std::string());

  DictionaryPrefUpdate dict_update(local_state_,
                                   KioskAppManager::kKioskDictionaryName);

  std::string app_key = std::string(KioskAppManager::kKeyApps) + '.' + app_id;
  dict_update->Remove(app_key, NULL);

  FOR_EACH_OBSERVER(KioskAppPrefsObserver,
                    observers_,
                    OnKioskAppsChanged());
}

void KioskAppPrefsLocalState::AddObserver(KioskAppPrefsObserver* observer) {
  observers_.AddObserver(observer);
}

void KioskAppPrefsLocalState::RemoveObserver(KioskAppPrefsObserver* observer) {
  observers_.RemoveObserver(observer);
}

KioskAppPrefsLocalState::KioskAppPrefsLocalState()
    : local_state_(g_browser_process->local_state()) {
}

KioskAppPrefsLocalState::~KioskAppPrefsLocalState() {}

bool KioskAppPrefsLocalState::HasApp(const std::string& app_id) const {
  const base::DictionaryValue* dict =
      local_state_->GetDictionary(KioskAppManager::kKioskDictionaryName);

  const base::DictionaryValue* apps_dict;
  if (dict->GetDictionary(KioskAppManager::kKeyApps, &apps_dict))
    return apps_dict->HasKey(app_id);

  return false;
}

}  // namespace
