// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_H_

#include "base/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_model_associator.h"
#include "chrome/browser/prefs/synced_pref_observer.h"
#include "components/pref_registry/pref_registry_syncable.h"

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

  // You may wish to use PrefServiceFactory or one of its subclasses
  // for simplified construction.
  PrefServiceSyncable(
      PrefNotifierImpl* pref_notifier,
      PrefValueStore* pref_value_store,
      PersistentPrefStore* user_prefs,
      user_prefs::PrefRegistrySyncable* pref_registry,
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
  //
  // TODO(albertb): Given that we now support priority preferences, callers of
  // this method are likely better off making the preferences they care about
  // into priority preferences and calling IsPrioritySyncing().
  bool IsSyncing();

  // Returns true if priority preferences state has synchronized with the remote
  // priority preferences.
  bool IsPrioritySyncing();

  // Returns true if the pref under the given name is pulled down from sync.
  // Note this does not refer to SYNCABLE_PREF.
  bool IsPrefSynced(const std::string& name) const;

  void AddObserver(PrefServiceSyncableObserver* observer);
  void RemoveObserver(PrefServiceSyncableObserver* observer);

  // TODO(zea): Have PrefServiceSyncable implement
  // syncer::SyncableService directly.
  syncer::SyncableService* GetSyncableService(const syncer::ModelType& type);

  // Do not call this after having derived an incognito or per tab pref service.
  virtual void UpdateCommandLinePrefStore(PrefStore* cmd_line_store) OVERRIDE;

  void AddSyncedPrefObserver(const std::string& name,
                             SyncedPrefObserver* observer);
  void RemoveSyncedPrefObserver(const std::string& name,
                                SyncedPrefObserver* observer);

 private:
  friend class PrefModelAssociator;

  void AddRegisteredSyncablePreference(
      const char* path,
      const user_prefs::PrefRegistrySyncable::PrefSyncStatus sync_status);

  // Invoked internally when the IsSyncing() state changes.
  void OnIsSyncingChanged();

  // Process a local preference change. This can trigger new SyncChanges being
  // sent to the syncer.
  void ProcessPrefChange(const std::string& name);

  // Whether CreateIncognitoPrefService() has been called to create a
  // "forked" PrefService.
  bool pref_service_forked_;

  PrefModelAssociator pref_sync_associator_;
  PrefModelAssociator priority_pref_sync_associator_;

  ObserverList<PrefServiceSyncableObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(PrefServiceSyncable);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_H_
