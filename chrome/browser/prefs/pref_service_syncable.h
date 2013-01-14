// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_H_

#include "chrome/browser/prefs/pref_model_associator.h"
#include "chrome/browser/prefs/pref_service.h"

class PrefServiceSyncableObserver;

namespace syncer {
class SyncableService;
}

// A PrefService that can be synced. Users are forced to declare
// whether preferences are syncable or not when registering them to
// this PrefService.
class PrefServiceSyncable : public PrefService {
 public:
  // Enum used when registering preferences to determine if it should be synced
  // or not. This is only used for profile prefs, not local state prefs.
  // See the Register*Pref methods for profile prefs below.
  enum PrefSyncStatus {
    UNSYNCABLE_PREF,
    SYNCABLE_PREF
  };

  // You may wish to use PrefServiceBuilder or one of its subclasses
  // for simplified construction.
  PrefServiceSyncable(
      PrefNotifierImpl* pref_notifier,
      PrefValueStore* pref_value_store,
      PersistentPrefStore* user_prefs,
      DefaultPrefStore* default_store,
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

  virtual void UnregisterPreference(const char* path) OVERRIDE;

  void RegisterBooleanPref(const char* path,
                           bool default_value,
                           PrefSyncStatus sync_status);
  void RegisterIntegerPref(const char* path,
                           int default_value,
                           PrefSyncStatus sync_status);
  void RegisterDoublePref(const char* path,
                          double default_value,
                          PrefSyncStatus sync_status);
  void RegisterStringPref(const char* path,
                          const std::string& default_value,
                          PrefSyncStatus sync_status);
  void RegisterFilePathPref(const char* path,
                            const FilePath& default_value,
                            PrefSyncStatus sync_status);
  void RegisterListPref(const char* path,
                        PrefSyncStatus sync_status);
  void RegisterDictionaryPref(const char* path,
                              PrefSyncStatus sync_status);
  void RegisterListPref(const char* path,
                        base::ListValue* default_value,
                        PrefSyncStatus sync_status);
  void RegisterDictionaryPref(const char* path,
                              base::DictionaryValue* default_value,
                              PrefSyncStatus sync_status);
  void RegisterLocalizedBooleanPref(const char* path,
                                    int locale_default_message_id,
                                    PrefSyncStatus sync_status);
  void RegisterLocalizedIntegerPref(const char* path,
                                    int locale_default_message_id,
                                    PrefSyncStatus sync_status);
  void RegisterLocalizedDoublePref(const char* path,
                                   int locale_default_message_id,
                                   PrefSyncStatus sync_status);
  void RegisterLocalizedStringPref(const char* path,
                                   int locale_default_message_id,
                                   PrefSyncStatus sync_status);
  void RegisterInt64Pref(const char* path,
                         int64 default_value,
                         PrefSyncStatus sync_status);
  void RegisterUint64Pref(const char* path,
                          uint64 default_value,
                          PrefSyncStatus sync_status);

  // TODO(zea): Have PrefServiceSyncable implement
  // syncer::SyncableService directly.
  syncer::SyncableService* GetSyncableService();

  // Do not call this after having derived an incognito or per tab pref service.
  virtual void UpdateCommandLinePrefStore(PrefStore* cmd_line_store) OVERRIDE;

 private:
  friend class PrefModelAssociator;

  // Invoked internally when the IsSyncing() state changes.
  void OnIsSyncingChanged();

  // Registers a preference at |path| with |default_value|. If the
  // preference is syncable per |sync_status|, also registers it with
  // PrefModelAssociator.
  void RegisterSyncablePreference(const char* path,
                                  Value* default_value,
                                  PrefSyncStatus sync_status);

  // Whether CreateIncognitoPrefService() has been called to create a
  // "forked" PrefService.
  bool pref_service_forked_;

  PrefModelAssociator pref_sync_associator_;

  ObserverList<PrefServiceSyncableObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(PrefServiceSyncable);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_H_
