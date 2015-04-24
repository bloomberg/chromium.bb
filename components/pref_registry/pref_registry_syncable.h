// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_SYNCABLE_H_
#define COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_SYNCABLE_H_

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
  // Enum of flags used when registering preferences to determine if it should
  // be synced or not. These flags are mutually exclusive, only one of them
  // should ever be specified.
  //
  // Note: These must NOT overlap with PrefRegistry::PrefRegistrationFlags.
  enum PrefRegistrationFlags {
    // The pref will not be synced.
    UNSYNCABLE_PREF = PrefRegistry::NO_REGISTRATION_FLAGS,

    // The pref will be synced.
    SYNCABLE_PREF = 1 << 1,

    // The pref will be synced. The pref will never be encrypted and will be
    // synced before other datatypes. Because they're never encrypted, on first
    // sync, they can be synced down before the user is prompted for a
    // passphrase.
    SYNCABLE_PRIORITY_PREF = 1 << 2,
  };

  typedef base::Callback<void(const char* path, uint32 flags)>
      SyncableRegistrationCallback;

  PrefRegistrySyncable();

  // Exactly one callback can be set for the event of a syncable
  // preference being registered. It will be fired after the
  // registration has occurred.
  //
  // Calling this method after a callback has already been set will
  // make the object forget the previous callback and use the new one
  // instead.
  void SetSyncableRegistrationCallback(const SyncableRegistrationCallback& cb);

  // |flags| is a bitmask of PrefRegistrationFlags.
  void RegisterBooleanPref(const char* path,
                           bool default_value,
                           uint32 flags);
  void RegisterIntegerPref(const char* path, int default_value, uint32 flags);
  void RegisterDoublePref(const char* path,
                          double default_value,
                          uint32 flags);
  void RegisterStringPref(const char* path,
                          const std::string& default_value,
                          uint32 flags);
  void RegisterFilePathPref(const char* path,
                            const base::FilePath& default_value,
                            uint32 flags);
  void RegisterListPref(const char* path, uint32 flags);
  void RegisterDictionaryPref(const char* path, uint32 flags);
  void RegisterListPref(const char* path,
                        base::ListValue* default_value,
                        uint32 flags);
  void RegisterDictionaryPref(const char* path,
                              base::DictionaryValue* default_value,
                              uint32 flags);
  void RegisterInt64Pref(const char* path, int64 default_value, uint32 flags);
  void RegisterUint64Pref(const char* path,
                          uint64 default_value,
                          uint32 flags);

  // Returns a new PrefRegistrySyncable that uses the same defaults
  // store.
  scoped_refptr<PrefRegistrySyncable> ForkForIncognito();

 private:
  ~PrefRegistrySyncable() override;

  // |flags| is a bitmask of PrefRegistrationFlags.
  void RegisterSyncablePreference(const char* path,
                                  base::Value* default_value,
                                  uint32 flags);

  SyncableRegistrationCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PrefRegistrySyncable);
};

}  // namespace user_prefs

#endif  // COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_SYNCABLE_H_
