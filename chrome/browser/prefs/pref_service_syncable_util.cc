// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_syncable_util.h"

#include <vector>

#include "chrome/browser/prefs/pref_service_incognito_whitelist.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync_preferences/pref_service_syncable.h"


sync_preferences::PrefServiceSyncable* PrefServiceSyncableFromProfile(
    Profile* profile) {
  return static_cast<sync_preferences::PrefServiceSyncable*>(
      profile->GetPrefs());
}

sync_preferences::PrefServiceSyncable* PrefServiceSyncableIncognitoFromProfile(
    Profile* profile) {
  return static_cast<sync_preferences::PrefServiceSyncable*>(
      profile->GetOffTheRecordPrefs());
}

std::unique_ptr<sync_preferences::PrefServiceSyncable>
CreateIncognitoPrefServiceSyncable(
    sync_preferences::PrefServiceSyncable* pref_service,
    PrefStore* incognito_extension_pref_store,
    std::unique_ptr<PrefValueStore::Delegate> delegate) {
  // List of keys that can be changed in the user prefs file by the incognito
  // profile.
  std::vector<const char*> persistent_pref_names;

  // TODO(https://crbug.com/861722): Remove |GetIncognitoWhitelist| and its
  // file. This list is ONLY added for transition of code from blacklist to
  // whitelist. All whitelisted prefs should be added here to
  // |persistent_pref_names|.
  prefs::GetIncognitoWhitelist(&persistent_pref_names);

  // TODO(https://crbug.com/861722): Current implementation does not cover
  // preferences from iOS. The code should be refactored to cover it.

  return pref_service->CreateIncognitoPrefService(
      incognito_extension_pref_store, persistent_pref_names,
      std::move(delegate));
}
