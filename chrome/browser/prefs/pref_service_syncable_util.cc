// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_syncable_util.h"

#include <vector>

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/syncable_prefs/pref_service_syncable.h"

#if defined(OS_ANDROID) || defined(OS_IOS)
#include "components/proxy_config/proxy_config_pref_names.h"
#endif

syncable_prefs::PrefServiceSyncable* PrefServiceSyncableFromProfile(
    Profile* profile) {
  return static_cast<syncable_prefs::PrefServiceSyncable*>(profile->GetPrefs());
}

syncable_prefs::PrefServiceSyncable* PrefServiceSyncableIncognitoFromProfile(
    Profile* profile) {
  return static_cast<syncable_prefs::PrefServiceSyncable*>(
      profile->GetOffTheRecordPrefs());
}

syncable_prefs::PrefServiceSyncable* CreateIncognitoPrefServiceSyncable(
    syncable_prefs::PrefServiceSyncable* pref_service,
    PrefStore* incognito_extension_pref_store) {
  // List of keys that cannot be changed in the user prefs file by the incognito
  // profile.  All preferences that store information about the browsing history
  // or behavior of the user should have this property.
  std::vector<const char*> overlay_pref_names;
  overlay_pref_names.push_back(prefs::kBrowserWindowPlacement);
  overlay_pref_names.push_back(prefs::kSaveFileDefaultDirectory);
#if defined(OS_ANDROID) || defined(OS_IOS)
  overlay_pref_names.push_back(proxy_config::prefs::kProxy);
#endif
  return pref_service->CreateIncognitoPrefService(
      incognito_extension_pref_store, overlay_pref_names);
}
