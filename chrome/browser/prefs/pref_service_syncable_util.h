// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_UTIL_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_UTIL_H_

#include <set>

#include "components/prefs/pref_value_store.h"

class PrefStore;
class Profile;

namespace sync_preferences {
class PrefServiceSyncable;
}

// sync_preferences::PrefServiceSyncable is a PrefService with added integration
// for sync, and knowledge of how to create an incognito PrefService. For code
// that does not need to know about the sync integration, you should use only
// the plain PrefService type.
//
// For this reason, Profile does not expose an accessor for the
// sync_preferences::PrefServiceSyncable type. Instead, you can use the
// utilities below to retrieve the sync_preferences::PrefServiceSyncable (or its
// incognito version) from a Profile.
sync_preferences::PrefServiceSyncable* PrefServiceSyncableFromProfile(
    Profile* profile);
sync_preferences::PrefServiceSyncable* PrefServiceSyncableIncognitoFromProfile(
    Profile* profile);

// Creates an incognito copy of |pref_service| that shares most prefs but uses
// a fresh non-persistent overlay for the user pref store and an individual
// extension pref store (to cache the effective extension prefs for incognito
// windows).
//
// If the Mojo pref service is in use |incognito_connector| and |user_connector|
// must be non-null.
std::unique_ptr<sync_preferences::PrefServiceSyncable>
CreateIncognitoPrefServiceSyncable(
    sync_preferences::PrefServiceSyncable* pref_service,
    PrefStore* incognito_extension_pref_store,
    std::unique_ptr<PrefValueStore::Delegate> delegate);

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_UTIL_H_
