// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_CHROME_PREF_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PREFS_CHROME_PREF_SERVICE_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
class FilePath;
class SequencedTaskRunner;
class Time;
}

namespace policy {
class PolicyService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefHashStore;
class PrefRegistry;
class PrefRegistrySimple;
class PrefService;
class PrefServiceSyncable;
class PrefStore;
class Profile;
class SupervisedUserSettingsService;
class TrackedPreferenceValidationDelegate;

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

scoped_ptr<PrefService> CreateLocalState(
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    policy::PolicyService* policy_service,
    const scoped_refptr<PrefRegistry>& pref_registry,
    bool async);

scoped_ptr<PrefServiceSyncable> CreateProfilePrefs(
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    TrackedPreferenceValidationDelegate* validation_delegate,
    policy::PolicyService* policy_service,
    SupervisedUserSettingsService* supervised_user_settings,
    const scoped_refptr<PrefStore>& extension_prefs,
    const scoped_refptr<user_prefs::PrefRegistrySyncable>& pref_registry,
    bool async);

// Schedules verification of the path resolution of the preferences file under
// |profile_path|.
void SchedulePrefsFilePathVerification(const base::FilePath& profile_path);

// Call before startup tasks kick in to disable delays in
// chrome_prefs::Schedule*() methods and ignore presence of a domain when
// determining the active SettingsEnforcement group. For testing only.
void DisableDelaysAndDomainCheckForTesting();

// Schedules an update check for all PrefHashStores, stores whose version
// doesn't match the latest version will then be updated. Clears all pref hash
// state on platforms that don't yet support a pref hash store.
void SchedulePrefHashStoresUpdateCheck(
    const base::FilePath& initial_profile_path);

// Initializes the preferences for the profile at |profile_path| with the
// preference values in |master_prefs|. Returns true on success.
bool InitializePrefsFromMasterPrefs(
    const base::FilePath& profile_path,
    const base::DictionaryValue& master_prefs);

// Retrieves the time of the last preference reset event, if any, for the
// provided profile. If no reset has occurred, returns a null |Time|.
base::Time GetResetTime(Profile* profile);

// Clears the time of the last preference reset event, if any, for the provided
// profile.
void ClearResetTime(Profile* profile);

// Register local state prefs used by chrome preference system.
void RegisterPrefs(PrefRegistrySimple* registry);

// Register user prefs used by chrome preference system.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace chrome_prefs

#endif  // CHROME_BROWSER_PREFS_CHROME_PREF_SERVICE_FACTORY_H_
