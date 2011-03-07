// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides a way to access the application's current preferences.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_H_
#pragma once

#include <set>
#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"

class DefaultPrefStore;
class FilePath;
class NotificationObserver;
class PersistentPrefStore;
class PrefChangeObserver;
class PrefNotifier;
class PrefNotifierImpl;
class PrefStore;
class PrefValueStore;
class Profile;

namespace subtle {
class PrefMemberBase;
};

class PrefService : public base::NonThreadSafe {
 public:
  // A helper class to store all the information associated with a preference.
  class Preference {
   public:

    // The type of the preference is determined by the type with which it is
    // registered. This type needs to be a boolean, integer, double, string,
    // dictionary (a branch), or list.  You shouldn't need to construct this on
    // your own; use the PrefService::Register*Pref methods instead.
    Preference(const PrefService* service,
               const char* name,
               Value::ValueType type);
    ~Preference() {}

    // Returns the name of the Preference (i.e., the key, e.g.,
    // browser.window_placement).
    const std::string name() const { return name_; }

    // Returns the registered type of the preference.
    Value::ValueType GetType() const;

    // Returns the value of the Preference, falling back to the registered
    // default value if no other has been set.
    const Value* GetValue() const;

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

    Value::ValueType type_;

    // Reference to the PrefService in which this pref was created.
    const PrefService* pref_service_;

    DISALLOW_COPY_AND_ASSIGN(Preference);
  };

  // Factory method that creates a new instance of a PrefService with the
  // applicable PrefStores. The |pref_filename| points to the user preference
  // file. The |profile| is the one to which these preferences apply; it may be
  // NULL if we're dealing with the local state. This is the usual way to create
  // a new PrefService. |extension_pref_store| is used as the source for
  // extension-controlled preferences and may be NULL. The PrefService takes
  // ownership of |extension_pref_store|.
  static PrefService* CreatePrefService(const FilePath& pref_filename,
                                        PrefStore* extension_pref_store,
                                        Profile* profile);

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

  // Make the PrefService aware of a pref.
  void RegisterBooleanPref(const char* path, bool default_value);
  void RegisterIntegerPref(const char* path, int default_value);
  void RegisterDoublePref(const char* path, double default_value);
  void RegisterStringPref(const char* path, const std::string& default_value);
  void RegisterFilePathPref(const char* path, const FilePath& default_value);
  void RegisterListPref(const char* path);
  void RegisterDictionaryPref(const char* path);
  // These take ownership of the default_value:
  void RegisterListPref(const char* path, ListValue* default_value);
  void RegisterDictionaryPref(const char* path, DictionaryValue* default_value);

  // These variants use a default value from the locale dll instead.
  void RegisterLocalizedBooleanPref(const char* path,
                                    int locale_default_message_id);
  void RegisterLocalizedIntegerPref(const char* path,
                                    int locale_default_message_id);
  void RegisterLocalizedDoublePref(const char* path,
                                   int locale_default_message_id);
  void RegisterLocalizedStringPref(const char* path,
                                   int locale_default_message_id);

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
  const DictionaryValue* GetDictionary(const char* path) const;
  const ListValue* GetList(const char* path) const;

  // Removes a user pref and restores the pref to its default value.
  void ClearPref(const char* path);

  // If the path is valid (i.e., registered), update the pref value in the user
  // prefs. Seting a null value on a preference of List or Dictionary type is
  // equivalent to removing the user value for that preference, allowing the
  // default value to take effect unless another value takes precedence.
  void Set(const char* path, const Value& value);
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
  void RegisterInt64Pref(const char* path, int64 default_value);

  // Used to set the value of dictionary or list values in the pref tree.  This
  // will create a dictionary or list if one does not exist in the pref tree.
  // This method returns NULL only if you're requesting an unregistered pref or
  // a non-dict/non-list pref.
  // WARNING: Changes to the dictionary or list will not automatically notify
  // pref observers.
  // Use a ScopedUserPrefUpdate to update observers on changes.
  // These should really be GetUserMutable... since we will only ever get
  // a mutable from the user preferences store.
  DictionaryValue* GetMutableDictionary(const char* path);
  ListValue* GetMutableList(const char* path);
  // TODO(battre) remove this function (hack).
  void ReportValueChanged(const std::string& key);

  // Returns true if a value has been set for the specified path.
  // NOTE: this is NOT the same as FindPreference. In particular
  // FindPreference returns whether RegisterXXX has been invoked, where as
  // this checks if a value exists for the path.
  bool HasPrefPath(const char* path) const;

  // Returns a dictionary with effective preference values. The ownership
  // is passed to the caller.
  DictionaryValue* GetPreferenceValues() const;

  // A helper method to quickly look up a preference.  Returns NULL if the
  // preference is not registered.
  const Preference* FindPreference(const char* pref_name) const;

  bool ReadOnly() const;

 protected:
  // Construct a new pref service, specifying the pref sources as explicit
  // PrefStore pointers. This constructor is what CreatePrefService() ends up
  // calling. It's also used for unit tests.
  PrefService(PrefStore* managed_platform_prefs,
              PrefStore* managed_cloud_prefs,
              PrefStore* extension_prefs,
              PrefStore* command_line_prefs,
              PersistentPrefStore* user_prefs,
              PrefStore* recommended_platform_prefs,
              PrefStore* recommended_cloud_prefs,
              DefaultPrefStore* default_store);

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

  // Give access to pref_notifier();
  friend class ScopedUserPrefUpdate;

  // Construct an incognito version of the pref service. Use
  // CreateIncognitoPrefService() instead of calling this constructor directly.
  PrefService(const PrefService& original,
              PrefStore* incognito_extension_prefs);

  // Returns a PrefNotifier. If you desire access to this, you will probably
  // want to use a ScopedUserPrefUpdate.
  PrefNotifier* pref_notifier() const;

  // If the pref at the given path changes, we call the observer's Observe
  // method with PREF_CHANGED. Note that observers should not call these methods
  // directly but rather use a PrefChangeRegistrar to make sure the observer
  // gets cleaned up properly.
  virtual void AddPrefObserver(const char* path, NotificationObserver* obs);
  virtual void RemovePrefObserver(const char* path, NotificationObserver* obs);

  // Registers a new preference at |path|. The |default_value| must not be
  // NULL as it determines the preference value's type.
  // RegisterPreference must not be called twice for the same path.
  // This method takes ownership of |default_value|.
  void RegisterPreference(const char* path, Value* default_value);

  // Sets the value for this pref path in the user pref store and informs the
  // PrefNotifier of the change.
  void SetUserPrefValue(const char* path, Value* new_value);

  // Load preferences from storage, attempting to diagnose and handle errors.
  // This should only be called from the constructor.
  void InitFromStorage();

  // The PrefValueStore provides prioritized preference values. It is created
  // and owned by this PrefService. Subclasses may access it for unit testing.
  scoped_ptr<PrefValueStore> pref_value_store_;

  // Pref Stores and profile that we passed to the PrefValueStore.
  scoped_refptr<PersistentPrefStore> user_pref_store_;
  scoped_refptr<DefaultPrefStore> default_store_;

  // Local cache of registered Preference objects. The default_store_
  // is authoritative with respect to what the types and default values
  // of registered preferences are.
  mutable PreferenceSet prefs_;

  DISALLOW_COPY_AND_ASSIGN(PrefService);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_H_
