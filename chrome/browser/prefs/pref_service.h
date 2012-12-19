// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides a way to access the application's current preferences.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/hash_tables.h"
#include "base/observer_list.h"
#include "base/prefs/persistent_pref_store.h"
#include "base/prefs/public/pref_service_base.h"
#include "base/threading/non_thread_safe.h"

class CommandLine;
class DefaultPrefStore;
class PrefModelAssociator;
class PrefNotifier;
class PrefNotifierImpl;
class PrefObserver;
class PrefServiceObserver;
class PrefStore;
class PrefValueStore;

namespace syncer {
class SyncableService;
}

namespace subtle {
class ScopedUserPrefUpdateBase;
};

class PrefService : public PrefServiceBase, public base::NonThreadSafe {
 public:
  enum PrefInitializationStatus {
    INITIALIZATION_STATUS_WAITING,
    INITIALIZATION_STATUS_SUCCESS,
    INITIALIZATION_STATUS_CREATED_NEW_PROFILE,
    INITIALIZATION_STATUS_ERROR
  };

  // A helper class to store all the information associated with a preference.
  class Preference : public PrefServiceBase::Preference {
   public:
    // The type of the preference is determined by the type with which it is
    // registered. This type needs to be a boolean, integer, double, string,
    // dictionary (a branch), or list.  You shouldn't need to construct this on
    // your own; use the PrefService::Register*Pref methods instead.
    Preference(const PrefService* service,
               const char* name,
               base::Value::Type type);
    virtual ~Preference() {}

    // PrefServiceBase::Preference implementation.
    virtual const std::string name() const OVERRIDE;
    virtual base::Value::Type GetType() const OVERRIDE;
    virtual const base::Value* GetValue() const OVERRIDE;
    virtual const base::Value* GetRecommendedValue() const OVERRIDE;
    virtual bool IsManaged() const OVERRIDE;
    virtual bool IsRecommended() const OVERRIDE;
    virtual bool HasExtensionSetting() const OVERRIDE;
    virtual bool HasUserSetting() const OVERRIDE;
    virtual bool IsExtensionControlled() const OVERRIDE;
    virtual bool IsUserControlled() const OVERRIDE;
    virtual bool IsDefaultValue() const OVERRIDE;
    virtual bool IsUserModifiable() const OVERRIDE;
    virtual bool IsExtensionModifiable() const OVERRIDE;

   private:
    friend class PrefService;

    PrefValueStore* pref_value_store() const {
      return pref_service_->pref_value_store_.get();
    }

    const std::string name_;

    const base::Value::Type type_;

    // Reference to the PrefService in which this pref was created.
    const PrefService* pref_service_;

  };

  // Creates an incognito copy of the pref service that shares most pref stores
  // but uses a fresh non-persistent overlay for the user pref store and an
  // individual extension pref store (to cache the effective extension prefs for
  // incognito windows).
  PrefService* CreateIncognitoPrefService(PrefStore* incognito_extension_prefs);

  virtual ~PrefService();

  // Reloads the data from file. This should only be called when the importer
  // is running during first run, and the main process may not change pref
  // values while the importer process is running. Returns true on success.
  bool ReloadPersistentPrefs();

  // Lands pending writes to disk. This should only be used if we need to save
  // immediately (basically, during shutdown).
  void CommitPendingWrite();

  void AddObserver(PrefServiceObserver* observer);
  void RemoveObserver(PrefServiceObserver* observer);

  // Returns true if preferences state has synchronized with the remote
  // preferences. If true is returned it can be assumed the local preferences
  // has applied changes from the remote preferences. The two may not be
  // identical if a change is in flight (from either side).
  bool IsSyncing();

  // Invoked internally when the IsSyncing() state changes.
  void OnIsSyncingChanged();

  // PrefServiceBase implementation.
  virtual bool IsManagedPreference(const char* pref_name) const OVERRIDE;
  virtual bool IsUserModifiablePreference(const char* pref_name) const OVERRIDE;
  virtual void RegisterBooleanPref(const char* path,
                                   bool default_value) OVERRIDE;
  virtual void RegisterIntegerPref(const char* path,
                                   int default_value) OVERRIDE;
  virtual void RegisterDoublePref(const char* path,
                                  double default_value) OVERRIDE;
  virtual void RegisterStringPref(const char* path,
                                  const std::string& default_value) OVERRIDE;
  virtual void RegisterFilePathPref(const char* path,
                                    const FilePath& default_value) OVERRIDE;
  virtual void RegisterListPref(const char* path) OVERRIDE;
  virtual void RegisterDictionaryPref(const char* path) OVERRIDE;
  virtual void RegisterListPref(const char* path,
                                base::ListValue* default_value) OVERRIDE;
  virtual void RegisterDictionaryPref(
      const char* path, base::DictionaryValue* default_value) OVERRIDE;
  virtual void RegisterLocalizedBooleanPref(
      const char* path, int locale_default_message_id) OVERRIDE;
  virtual void RegisterLocalizedIntegerPref(
      const char* path, int locale_default_message_id) OVERRIDE;
  virtual void RegisterLocalizedDoublePref(
      const char* path, int locale_default_message_id) OVERRIDE;
  virtual void RegisterLocalizedStringPref(
      const char* path, int locale_default_message_id) OVERRIDE;
  virtual void RegisterInt64Pref(const char* path,
                                 int64 default_value) OVERRIDE;
  virtual void RegisterBooleanPref(const char* path,
                                   bool default_value,
                                   PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterIntegerPref(const char* path,
                                   int default_value,
                                   PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterDoublePref(const char* path,
                                  double default_value,
                                  PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterStringPref(const char* path,
                                  const std::string& default_value,
                                  PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterFilePathPref(const char* path,
                                    const FilePath& default_value,
                                    PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterListPref(const char* path,
                                PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterDictionaryPref(const char* path,
                                      PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterListPref(const char* path,
                                base::ListValue* default_value,
                                PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterDictionaryPref(const char* path,
                                      base::DictionaryValue* default_value,
                                      PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterLocalizedBooleanPref(
      const char* path,
      int locale_default_message_id,
      PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterLocalizedIntegerPref(
      const char* path,
      int locale_default_message_id,
      PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterLocalizedDoublePref(
      const char* path,
      int locale_default_message_id,
      PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterLocalizedStringPref(
      const char* path,
      int locale_default_message_id,
      PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterInt64Pref(const char* path,
                                 int64 default_value,
                                 PrefSyncStatus sync_status) OVERRIDE;
  virtual void RegisterUint64Pref(const char* path,
                                  uint64 default_value,
                                  PrefSyncStatus sync_status) OVERRIDE;
  virtual void UnregisterPreference(const char* path) OVERRIDE;
  virtual const PrefService::Preference* FindPreference(
      const char* path) const OVERRIDE;
  virtual bool GetBoolean(const char* path) const OVERRIDE;
  virtual int GetInteger(const char* path) const OVERRIDE;
  virtual double GetDouble(const char* path) const OVERRIDE;
  virtual std::string GetString(const char* path) const OVERRIDE;
  virtual FilePath GetFilePath(const char* path) const OVERRIDE;
  virtual const base::DictionaryValue* GetDictionary(
      const char* path) const OVERRIDE;
  virtual const base::ListValue* GetList(const char* path) const OVERRIDE;
  virtual void ClearPref(const char* path) OVERRIDE;
  virtual void Set(const char* path, const base::Value& value) OVERRIDE;
  virtual void SetBoolean(const char* path, bool value) OVERRIDE;
  virtual void SetInteger(const char* path, int value) OVERRIDE;
  virtual void SetDouble(const char* path, double value) OVERRIDE;
  virtual void SetString(const char* path, const std::string& value) OVERRIDE;
  virtual void SetFilePath(const char* path, const FilePath& value) OVERRIDE;
  virtual void SetInt64(const char* path, int64 value) OVERRIDE;
  virtual int64 GetInt64(const char* path) const OVERRIDE;
  virtual void SetUint64(const char* path, uint64 value) OVERRIDE;
  virtual uint64 GetUint64(const char* path) const OVERRIDE;

  // Returns the value of the given preference, from the user pref store. If
  // the preference is not set in the user pref store, returns NULL.
  const base::Value* GetUserPrefValue(const char* path) const;

  // Returns the default value of the given preference. |path| must point to a
  // registered preference. In that case, will never return NULL.
  const base::Value* GetDefaultPrefValue(const char* path) const;

  // Returns true if a value has been set for the specified path.
  // NOTE: this is NOT the same as FindPreference. In particular
  // FindPreference returns whether RegisterXXX has been invoked, where as
  // this checks if a value exists for the path.
  bool HasPrefPath(const char* path) const;

  // Returns a dictionary with effective preference values. The ownership
  // is passed to the caller.
  base::DictionaryValue* GetPreferenceValues() const;

  bool ReadOnly() const;

  PrefInitializationStatus GetInitializationStatus() const;

  // syncer::SyncableService getter.
  // TODO(zea): Have PrefService implement syncer::SyncableService directly.
  syncer::SyncableService* GetSyncableService();

  // Tell our PrefValueStore to update itself using |command_line|.
  // Do not call this after having derived an incognito or per tab pref service.
  void UpdateCommandLinePrefStore(CommandLine* command_line);

  // We run the callback once, when initialization completes. The bool
  // parameter will be set to true for successful initialization,
  // false for unsuccessful.
  void AddPrefInitObserver(base::Callback<void(bool)> callback);

 protected:
  // Construct a new pref service. This constructor is what
  // factory methods end up calling and what is used for unit tests.
  PrefService(PrefNotifierImpl* pref_notifier,
              PrefValueStore* pref_value_store,
              PersistentPrefStore* user_prefs,
              DefaultPrefStore* default_store,
              PrefModelAssociator* pref_sync_associator,
              base::Callback<void(PersistentPrefStore::PrefReadError)>
                  read_error_callback,
              bool async);

  // The PrefNotifier handles registering and notifying preference observers.
  // It is created and owned by this PrefService. Subclasses may access it for
  // unit testing.
  scoped_ptr<PrefNotifierImpl> pref_notifier_;

 private:
  // Hash map expected to be fastest here since it minimises expensive
  // string comparisons. Order is unimportant, and deletions are rare.
  // Confirmed on Android where this speeded Chrome startup by roughly 50ms
  // vs. std::map, and by roughly 180ms vs. std::set of Preference pointers.
  typedef base::hash_map<std::string, Preference> PreferenceMap;

  friend class PrefServiceBuilder;

  // Give access to ReportUserPrefChanged() and GetMutableUserPref().
  friend class subtle::ScopedUserPrefUpdateBase;

  // PrefServiceBase implementation (protected in base, private here).
  virtual void AddPrefObserver(const char* path,
                               PrefObserver* obs) OVERRIDE;
  virtual void RemovePrefObserver(const char* path,
                                  PrefObserver* obs) OVERRIDE;

  // Sends notification of a changed preference. This needs to be called by
  // a ScopedUserPrefUpdate if a DictionaryValue or ListValue is changed.
  void ReportUserPrefChanged(const std::string& key);

  // Registers a new preference at |path|. The |default_value| must not be
  // NULL as it determines the preference value's type.
  // RegisterPreference must not be called twice for the same path.
  // This method takes ownership of |default_value|.
  void RegisterPreference(const char* path,
                          base::Value* default_value,
                          PrefSyncStatus sync_status);

  // Sets the value for this pref path in the user pref store and informs the
  // PrefNotifier of the change.
  void SetUserPrefValue(const char* path, base::Value* new_value);

  // Load preferences from storage, attempting to diagnose and handle errors.
  // This should only be called from the constructor.
  void InitFromStorage(bool async);

  // Used to set the value of dictionary or list values in the user pref store.
  // This will create a dictionary or list if one does not exist in the user
  // pref store. This method returns NULL only if you're requesting an
  // unregistered pref or a non-dict/non-list pref.
  // |type| may only be Values::TYPE_DICTIONARY or Values::TYPE_LIST and
  // |path| must point to a registered preference of type |type|.
  // Ownership of the returned value remains at the user pref store.
  base::Value* GetMutableUserPref(const char* path,
                                  base::Value::Type type);

  // GetPreferenceValue is the equivalent of FindPreference(path)->GetValue(),
  // it has been added for performance. If is faster because it does
  // not need to find or create a Preference object to get the
  // value (GetValue() calls back though the preference service to
  // actually get the value.).
  const base::Value* GetPreferenceValue(const std::string& path) const;

  // The PrefValueStore provides prioritized preference values. It is owned by
  // this PrefService. Subclasses may access it for unit testing.
  scoped_ptr<PrefValueStore> pref_value_store_;

  // Pref Stores and profile that we passed to the PrefValueStore.
  scoped_refptr<PersistentPrefStore> user_pref_store_;
  scoped_refptr<DefaultPrefStore> default_store_;

  // Local cache of registered Preference objects. The default_store_
  // is authoritative with respect to what the types and default values
  // of registered preferences are.
  mutable PreferenceMap prefs_map_;

  // The model associator that maintains the links with the sync db.
  scoped_ptr<PrefModelAssociator> pref_sync_associator_;

  // Callback to call when a read error occurs.
  base::Callback<void(PersistentPrefStore::PrefReadError)> read_error_callback_;

  // Whether CreateIncognitoPrefService() has been called to create a
  // "forked" PrefService.
  bool pref_service_forked_;

  ObserverList<PrefServiceObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(PrefService);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_H_
