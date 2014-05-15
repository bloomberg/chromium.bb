// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_SYNCABLE_H_
#define COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_SYNCABLE_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/prefs/pref_registry.h"
#include "components/pref_registry/pref_registry_export.h"

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
class Value;
}

// TODO(tfarina): Change this namespace to pref_registry.
namespace user_prefs {

// A PrefRegistry that forces users to choose whether each registered
// preference is syncable or not.
//
// Classes or components that want to register such preferences should
// define a static function named RegisterUserPrefs that takes a
// PrefRegistrySyncable*, and the top-level application using the
// class or embedding the component should call this function at an
// appropriate time before the PrefService for these preferences is
// constructed. See e.g. chrome/browser/prefs/browser_prefs.cc which
// does this for Chrome.
class PREF_REGISTRY_EXPORT PrefRegistrySyncable : public PrefRegistry {
 public:
  // Enum used when registering preferences to determine if it should
  // be synced or not. Syncable priority preferences are preferences that are
  // never encrypted and are synced before other datatypes. Because they're
  // never encrypted, on first sync, they can be synced down before the user
  // is prompted for a passphrase.
  enum PrefSyncStatus {
    UNSYNCABLE_PREF,
    SYNCABLE_PREF,
    SYNCABLE_PRIORITY_PREF,
  };

  typedef
      base::Callback<void(const char* path, const PrefSyncStatus sync_status)>
          SyncableRegistrationCallback;

  PrefRegistrySyncable();

  typedef std::map<std::string, PrefSyncStatus> PrefToStatus;

  // Retrieve the set of syncable preferences currently registered.
  const PrefToStatus& syncable_preferences() const;

  // Exactly one callback can be set for the event of a syncable
  // preference being registered. It will be fired after the
  // registration has occurred.
  //
  // Calling this method after a callback has already been set will
  // make the object forget the previous callback and use the new one
  // instead.
  void SetSyncableRegistrationCallback(const SyncableRegistrationCallback& cb);

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
                            const base::FilePath& default_value,
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

  // Returns a new PrefRegistrySyncable that uses the same defaults
  // store.
  scoped_refptr<PrefRegistrySyncable> ForkForIncognito();

 private:
  virtual ~PrefRegistrySyncable();

  void RegisterSyncablePreference(const char* path,
                                  base::Value* default_value,
                                  PrefSyncStatus sync_status);

  SyncableRegistrationCallback callback_;

  // Contains the names of all registered preferences that are syncable.
  PrefToStatus syncable_preferences_;

  DISALLOW_COPY_AND_ASSIGN(PrefRegistrySyncable);
};

}  // namespace user_prefs

#endif  // COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_SYNCABLE_H_
