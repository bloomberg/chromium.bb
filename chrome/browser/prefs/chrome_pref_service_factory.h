// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_CHROME_PREF_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PREFS_CHROME_PREF_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_value_store.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom-forward.h"

namespace base {
class DictionaryValue;
class FilePath;
class SequencedTaskRunner;
class Time;
}

namespace policy {
class PolicyService;
class BrowserPolicyConnector;
}

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefRegistry;
class PrefService;

class PrefStore;
class Profile;
class SupervisedUserSettingsService;

namespace chrome_prefs {

namespace internals {

extern const char kSettingsEnforcementTrialName[];
extern const char kSettingsEnforcementGroupNoEnforcement[];
extern const char kSettingsEnforcementGroupEnforceAlways[];
extern const char kSettingsEnforcementGroupEnforceAlwaysWithDSE[];
extern const char kSettingsEnforcementGroupEnforceAlwaysWithExtensionsAndDSE[];

}  // namespace internals

// Factory methods that create and initialize a new instance of a
// PrefService for Chrome with the applicable PrefStores. The
// |pref_filename| points to the user preference file. This is the
// usual way to create a new PrefService.
// |extension_pref_store| is used as the source for extension-controlled
// preferences and may be NULL.
// |policy_service| is used as the source for mandatory or recommended
// policies.
// |pref_registry| keeps the list of registered prefs and their default values.
// If |async| is true, asynchronous version is used.
// Notifies using PREF_INITIALIZATION_COMPLETED in the end. Details is set to
// the created PrefService or NULL if creation has failed. Note, it is
// guaranteed that in asynchronous version initialization happens after this
// function returned.
std::unique_ptr<PrefService> CreateLocalState(
    const base::FilePath& pref_filename,
    policy::PolicyService* policy_service,
    scoped_refptr<PrefRegistry> pref_registry,
    bool async,
    policy::BrowserPolicyConnector* policy_connector);

std::unique_ptr<sync_preferences::PrefServiceSyncable> CreateProfilePrefs(
    const base::FilePath& pref_filename,
    mojo::PendingRemote<prefs::mojom::TrackedPreferenceValidationDelegate>
        validation_delegate,
    policy::PolicyService* policy_service,
    SupervisedUserSettingsService* supervised_user_settings,
    scoped_refptr<PrefStore> extension_prefs,
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry,
    policy::BrowserPolicyConnector* connector,
    bool async,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner);

// determining the active SettingsEnforcement group. For testing only.
void DisableDomainCheckForTesting();

// Initializes the preferences for the profile at |profile_path| with the
// preference values in |master_prefs|. Returns true on success.
bool InitializePrefsFromMasterPrefs(
    const base::FilePath& profile_path,
    std::unique_ptr<base::DictionaryValue> master_prefs);

// Retrieves the time of the last preference reset event, if any, for the
// provided profile. If no reset has occurred, returns a null |Time|.
base::Time GetResetTime(Profile* profile);

// Clears the time of the last preference reset event, if any, for the provided
// profile.
void ClearResetTime(Profile* profile);

// Register user prefs used by chrome preference system.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Shows notifications which correspond to PersistentPrefStore's reading errors.
void HandlePersistentPrefStoreReadError(
    const base::FilePath& pref_filename,
    PersistentPrefStore::PrefReadError error);

}  // namespace chrome_prefs

#endif  // CHROME_BROWSER_PREFS_CHROME_PREF_SERVICE_FACTORY_H_
