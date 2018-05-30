// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_SYNCABLE_H_
#define COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_SYNCABLE_H_

#include <stdint.h>

#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "components/prefs/pref_registry_simple.h"

namespace base {
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
//
// TODO(raymes): This class only exists to support SyncableRegistrationCallback
// logic which is only required to support pref registration after the
// PrefService has been created which is only used by tests. We can remove this
// entire class and those tests with some work.
class PrefRegistrySyncable : public PrefRegistrySimple {
 public:
  // Enum of flags used when registering preferences to determine if it should
  // be synced or not. These flags are mutually exclusive, only one of them
  // should ever be specified.
  //
  // Note: These must NOT overlap with PrefRegistry::PrefRegistrationFlags.
  enum PrefRegistrationFlags : uint32_t {
    // The pref will be synced.
    SYNCABLE_PREF = 1 << 0,

    // The pref will be synced. The pref will never be encrypted and will be
    // synced before other datatypes.
    // Because they're never encrypted:
    // -- they can be synced down on first sync before the user is prompted for
    //    a passphrase.
    // -- they are preferred for receiving server-provided data.
    SYNCABLE_PRIORITY_PREF = 1 << 1,
  };

  typedef base::Callback<void(const std::string& path, uint32_t flags)>
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

  // Returns a new PrefRegistrySyncable that uses the same defaults
  // store.
  scoped_refptr<PrefRegistrySyncable> ForkForIncognito();

  // Adds a the preference with name |pref_name| to the whitelist of prefs which
  // will be synced even before they got registered. Note that it's still
  // illegal to read or write a whitelisted preference via the PrefService
  // before its registration.
  void WhitelistLateRegistrationPrefForSync(const std::string& pref_name);

  // Checks weather the preference with name |path| is on the whitelist of
  // sync-supported prefs before registration.
  bool IsWhitelistedLateRegistrationPref(const std::string& path) const;

 private:
  ~PrefRegistrySyncable() override;

  // PrefRegistrySimple overrides.
  void OnPrefRegistered(const std::string& path,
                        base::Value* default_value,
                        uint32_t flags) override;

  SyncableRegistrationCallback callback_;
  std::set<std::string> sync_unknown_prefs_whitelist_;

  DISALLOW_COPY_AND_ASSIGN(PrefRegistrySyncable);
};

}  // namespace user_prefs

#endif  // COMPONENTS_PREF_REGISTRY_PREF_REGISTRY_SYNCABLE_H_
