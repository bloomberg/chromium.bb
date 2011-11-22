// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides a way to access the application's current preferences.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_H_
#pragma once

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "chrome/common/json_pref_store.h"

class DefaultPrefStore;
class FilePath;
class PersistentPrefStore;
class PrefModelAssociator;
class PrefNotifier;
class PrefNotifierImpl;
class PrefStore;
class PrefValueStore;
class Profile;
class SyncableService;

namespace content {
class NotificationObserver;
}

namespace subtle {
class PrefMemberBase;
class ScopedUserPrefUpdateBase;
};

class PrefService;

class PrefService : public base::NonThreadSafe {
 public:
  // Enum used when registering preferences to determine if it should be synced
  // or not. This is only used for profile prefs, not local state prefs.
  // See the Register*Pref methods for profile prefs below.
  enum PrefSyncStatus {
    UNSYNCABLE_PREF,
    SYNCABLE_PREF
  };

  // A helper class to store all the information associated with a preference.
  class Preference {
   public:

    // The type of the preference is determined by the type with which it is
    // registered. This type needs to be a boolean, integer, double, string,
    // dictionary (a branch), or list.  You shouldn't need to construct this on
    // your own; use the PrefService::Register*Pref methods instead.
    Preference(const PrefService* service,
               const char* name,
               base::Value::Type type);
    ~Preference() {}

    // Returns the name of the Preference (i.e., the key, e.g.,
    // browser.window_placement).
    const std::string name() const { return name_; }

    // Returns the registered type of the preference.
    base::Value::Type GetType() const;

    // Returns the value of the Preference, falling back to the registered
    // default value if no other has been set.
    const base::Value* GetValue() const;

    // Returns true if the Preference is managed, i.e. set by an admin policy.
    // Since managed prefs have the highest priority, this also indicates
    // whether the pref is actually being controlled by the policy setting.
    bool IsManaged() const;

    // Returns true if the Preference has a value set by an extension, even if
    // that value is being overridden by a higher-priority source.
    bool HasExtensionSetting() const;

    // Returns true if the Preference has a user setting, even if that value is
    // being overridden by a higher-priority source.
    bool HasUserSetting() const;

    // Returns true if the Preference value is currently being controlled by an
    // extension, and not by any higher-priority source.
    bool IsExtensionControlled() const;

    // Returns true if the Preference value is currently being controlled by a
    // user setting, and not by any higher-priority source.
    bool IsUserControlled() const;

    // Returns true if the Preference is currently using its default value,
    // and has not been set by any higher-priority source (even with the same
    // value).
    bool IsDefaultValue() const;

    // Returns true if the user can change the Preference value, which is the
    // case if no higher-priority source than the user store controls the
    // Preference.
    bool IsUserModifiable() const;

    // Returns true if an extension can change the Preference value, which is
    // the case if no higher-priority source than the extension store controls
    // the Preference.
    bool IsExtensionModifiable() const;

   private:
    friend class PrefService;

    PrefValueStore* pref_value_store() const {
      return pref_service_->pref_value_store_.get();
    }

    std::string name_;

    base::Value::Type type_;

    // Reference to the PrefService in which this pref was created.
    const PrefService* pref_service_;

    DISALLOW_COPY_AND_ASSIGN(Preference);
  };

  // Factory method that creates a new instance of a PrefService with the
  // applicable PrefStores. The |pref_filename| points to the user preference
  // file. This is the usual way to create a new PrefService.
  // |extension_pref_store| is used as the source for extension-controlled
  // preferences and may be NULL. The PrefService takes ownership of
  // |extension_pref_store|. If |async| is true, asynchronous version is used.
  // Notifies using PREF_INITIALIZATION_COMPLETED in the end. Details is set to
  // the created PrefService or NULL if creation has failed. Note, it is
  // guaranteed that in asynchronous version initialization happens after this
  // function returned.
  static PrefService* CreatePrefService(const FilePath& pref_filename,
                                        PrefStore* extension_pref_store,
                                        bool async);

  // Creates an incognito copy of the pref service that shares most pref stores
  // but uses a fresh non-persistent overlay for the user pref store and an
  // individual extension pref store (to cache the effective extension prefs for
  // incognito windows).
  PrefService* CreateIncognitoPrefService(PrefStore* incognito_extension_prefs);

  // Creates a per-tab copy of the pref service that shares most pref stores
  // and allows WebKit-related preferences to be overridden on per-tab basis.
  PrefService* CreatePrefServiceWithPerTabPrefStore();

  virtual ~PrefService();

  // Reloads the data from file. This should only be called when the importer
  // is running during first run, and the main process may not change pref
  // values while the importer process is running. Returns true on success.
  bool ReloadPersistentPrefs();

  // Returns true if the preference for the given preference name is available
  // and is managed.
  bool IsManagedPreference(const char* pref_name) const;

  // Writes the data to disk. The return value only reflects whether
  // serialization was successful; we don't know whether the data actually made
  // it on disk (since it's on a different thread).  This should only be used if
  // we need to save immediately (basically, during shutdown).  Otherwise, you
  // should use ScheduleSavePersistentPrefs.
  bool SavePersistentPrefs();

  // Serializes the data and schedules save using ImportantFileWriter.
  void ScheduleSavePersistentPrefs();

  // Lands pending writes to disk.
  void CommitPendingWrite();

  // Make the PrefService aware of a pref.
  // TODO(zea): split local state and profile prefs into their own subclasses.
  // ---------- Local state prefs  ----------
  void RegisterBooleanPref(const char* path, bool default_value);
  void RegisterIntegerPref(const char* path, int default_value);
  void RegisterDoublePref(const char* path, double default_value);
  void RegisterStringPref(const char* path, const std::string& default_value);
  void RegisterFilePathPref(const char* path, const FilePath& default_value);
  void RegisterListPref(const char* path);
  void RegisterDictionaryPref(const char* path);
  // These take ownership of the default_value:
  void RegisterListPref(const char* path,
                        base::ListValue* default_value);
  void RegisterDictionaryPref(const char* path,
                              base::DictionaryValue* default_value);
  // These variants use a default value from the locale dll instead.
  void RegisterLocalizedBooleanPref(const char* path,
                                    int locale_default_message_id);
  void RegisterLocalizedIntegerPref(const char* path,
                                    int locale_default_message_id);
  void RegisterLocalizedDoublePref(const char* path,
                                   int locale_default_message_id);
  void RegisterLocalizedStringPref(const char* path,
                                   int locale_default_message_id);
  void RegisterInt64Pref(const char* path, int64 default_value);

  //  ---------- Profile prefs  ----------
  // Profile prefs must specify whether the pref should be synchronized across
  // machines or not (see PrefSyncStatus enum above).
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
  void RegisterListPref(const char* path, PrefSyncStatus sync_status);
  void RegisterDictionaryPref(const char* path, PrefSyncStatus sync_status);
  // These take ownership of the default_value:
  void RegisterListPref(const char* path,
                        base::ListValue* default_value,
                        PrefSyncStatus sync_status);
  void RegisterDictionaryPref(const char* path,
                              base::DictionaryValue* default_value,
                              PrefSyncStatus sync_status);
  // These variants use a default value from the locale dll instead.
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

  // If the path is valid and the value at the end of the path matches the type
  // specified, it will return the specified value.  Otherwise, the default
  // value (set when the pref was registered) will be returned.
  bool GetBoolean(const char* path) const;
  int GetInteger(const char* path) const;
  double GetDouble(const char* path) const;
  std::string GetString(const char* path) const;
  FilePath GetFilePath(const char* path) const;

  // Returns the branch if it exists, or the registered default value otherwise.
  // Note that |path| must point to a registered preference. In that case, these
  // functions will never return NULL.
  const base::DictionaryValue* GetDictionary(const char* path) const;
  const base::ListValue* GetList(const char* path) const;

  // Removes a user pref and restores the pref to its default value.
  void ClearPref(const char* path);

  // If the path is valid (i.e., registered), update the pref value in the user
  // prefs.
  // To set the value of dictionary or list values in the pref tree use
  // Set(), but to modify the value of a dictionary or list use either
  // ListPrefUpdate or DictionaryPrefUpdate from scoped_user_pref_update.h.
  void Set(const char* path, const base::Value& value);
  void SetBoolean(const char* path, bool value);
  void SetInteger(const char* path, int value);
  void SetDouble(const char* path, double value);
  void SetString(const char* path, const std::string& value);
  void SetFilePath(const char* path, const FilePath& value);

  // Int64 helper methods that actually store the given value as a string.
  // Note that if obtaining the named value via GetDictionary or GetList, the
  // Value type will be TYPE_STRING.
  void SetInt64(const char* path, int64 value);
  int64 GetInt64(const char* path) const;

  // Returns true if a value has been set for the specified path.
  // NOTE: this is NOT the same as FindPreference. In particular
  // FindPreference returns whether RegisterXXX has been invoked, where as
  // this checks if a value exists for the path.
  bool HasPrefPath(const char* path) const;

  // Returns a dictionary with effective preference values. The ownership
  // is passed to the caller.
  base::DictionaryValue* GetPreferenceValues() const;

  // A helper method to quickly look up a preference.  Returns NULL if the
  // preference is not registered.
  const Preference* FindPreference(const char* pref_name) const;

  bool ReadOnly() const;

  // SyncableService getter.
  // TODO(zea): Have PrefService implement SyncableService directly.
  SyncableService* GetSyncableService();

 protected:
  // Construct a new pref service. This constructor is what
  // factory methods end up calling and what is used for unit tests.
  PrefService(PrefNotifierImpl* pref_notifier,
              PrefValueStore* pref_value_store,
              PersistentPrefStore* user_prefs,
              DefaultPrefStore* default_store,
              PrefModelAssociator* pref_sync_associator,
              bool async);

  // The PrefNotifier handles registering and notifying preference observers.
  // It is created and owned by this PrefService. Subclasses may access it for
  // unit testing.
  scoped_ptr<PrefNotifierImpl> pref_notifier_;

 private:
  class PreferencePathComparator {
   public:
    bool operator() (Preference* lhs, Preference* rhs) const {
      return lhs->name() < rhs->name();
    }
  };
  typedef std::set<Preference*, PreferencePathComparator> PreferenceSet;

  friend class PrefServiceMockBuilder;

  // Registration of pref change observers must be done using the
  // PrefChangeRegistrar, which is declared as a friend here to grant it
  // access to the otherwise protected members Add/RemovePrefObserver.
  // PrefMember registers for preferences changes notification directly to
  // avoid the storage overhead of the registrar, so its base class must be
  // declared as a friend, too.
  friend class PrefChangeRegistrar;
  friend class subtle::PrefMemberBase;

  // Give access to ReportUserPrefChanged() and GetMutableUserPref().
  friend class subtle::ScopedUserPrefUpdateBase;

  // Sends notification of a changed preference. This needs to be called by
  // a ScopedUserPrefUpdate if a DictionaryValue or ListValue is changed.
  void ReportUserPrefChanged(const std::string& key);

  // If the pref at the given path changes, we call the observer's Observe
  // method with PREF_CHANGED. Note that observers should not call these methods
  // directly but rather use a PrefChangeRegistrar to make sure the observer
  // gets cleaned up properly.
  virtual void AddPrefObserver(const char* path,
                               content::NotificationObserver* obs);
  virtual void RemovePrefObserver(const char* path,
                                  content::NotificationObserver* obs);

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

  // The PrefValueStore provides prioritized preference values. It is owned by
  // this PrefService. Subclasses may access it for unit testing.
  scoped_ptr<PrefValueStore> pref_value_store_;

  // Pref Stores and profile that we passed to the PrefValueStore.
  scoped_refptr<PersistentPrefStore> user_pref_store_;
  scoped_refptr<DefaultPrefStore> default_store_;

  // Local cache of registered Preference objects. The default_store_
  // is authoritative with respect to what the types and default values
  // of registered preferences are.
  mutable PreferenceSet prefs_;

  // The model associator that maintains the links with the sync db.
  scoped_ptr<PrefModelAssociator> pref_sync_associator_;

  DISALLOW_COPY_AND_ASSIGN(PrefService);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_H_
