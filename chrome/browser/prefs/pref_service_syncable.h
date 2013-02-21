// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_H_

#include "base/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_model_associator.h"

class PrefRegistrySyncable;
class PrefServiceSyncableObserver;
class Profile;

namespace syncer {
class SyncableService;
}

// A PrefService that can be synced. Users are forced to declare
// whether preferences are syncable or not when registering them to
// this PrefService.
class PrefServiceSyncable : public PrefService {
 public:
  // PrefServiceSyncable is a PrefService with added integration for
  // sync, and knowledge of how to create an incognito
  // PrefService. For code that does not need to know about the sync
  // integration, you should use only the plain PrefService type.
  //
  // For this reason, Profile does not expose an accessor for the
  // PrefServiceSyncable type. Instead, you can use the utilities
  // below to retrieve the PrefServiceSyncable (or its incognito
  // version) from a Profile.
  static PrefServiceSyncable* FromProfile(Profile* profile);
  static PrefServiceSyncable* IncognitoFromProfile(Profile* profile);

  // You may wish to use PrefServiceBuilder or one of its subclasses
  // for simplified construction.
  PrefServiceSyncable(
      PrefNotifierImpl* pref_notifier,
      PrefValueStore* pref_value_store,
      PersistentPrefStore* user_prefs,
      PrefRegistrySyncable* pref_registry,
      base::Callback<void(PersistentPrefStore::PrefReadError)>
          read_error_callback,
      bool async);
  virtual ~PrefServiceSyncable();

  // Creates an incognito copy of the pref service that shares most pref stores
  // but uses a fresh non-persistent overlay for the user pref store and an
  // individual extension pref store (to cache the effective extension prefs for
  // incognito windows).
  PrefServiceSyncable* CreateIncognitoPrefService(
      PrefStore* incognito_extension_prefs);

  // Returns true if preferences state has synchronized with the remote
  // preferences. If true is returned it can be assumed the local preferences
  // has applied changes from the remote preferences. The two may not be
  // identical if a change is in flight (from either side).
  bool IsSyncing();

  void AddObserver(PrefServiceSyncableObserver* observer);
  void RemoveObserver(PrefServiceSyncableObserver* observer);

  // TODO(zea): Have PrefServiceSyncable implement
  // syncer::SyncableService directly.
  syncer::SyncableService* GetSyncableService();

  // Do not call this after having derived an incognito or per tab pref service.
  virtual void UpdateCommandLinePrefStore(PrefStore* cmd_line_store) OVERRIDE;

 private:
  friend class PrefModelAssociator;

  void AddRegisteredSyncablePreference(const char* path);

  // Invoked internally when the IsSyncing() state changes.
  void OnIsSyncingChanged();

  // Whether CreateIncognitoPrefService() has been called to create a
  // "forked" PrefService.
  bool pref_service_forked_;

  PrefModelAssociator pref_sync_associator_;

  ObserverList<PrefServiceSyncableObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(PrefServiceSyncable);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_H_
