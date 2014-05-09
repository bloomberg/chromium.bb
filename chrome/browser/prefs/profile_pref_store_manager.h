// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PROFILE_PREF_STORE_MANAGER_H_
#define CHROME_BROWSER_PREFS_PROFILE_PREF_STORE_MANAGER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/prefs/pref_hash_filter.h"

class PersistentPrefStore;
class PrefHashStoreImpl;
class PrefService;

namespace base {
class DictionaryValue;
class SequencedTaskRunner;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefRegistrySimple;

// Provides a facade through which the user preference store may be accessed and
// managed.
class ProfilePrefStoreManager {
 public:
  // Instantiates a ProfilePrefStoreManager with the configuration required to
  // manage the user preferences of the profile at |profile_path|.
  // |tracking_configuration| is used for preference tracking.
  // |reporting_ids_count| is the count of all possible tracked preference IDs
  // (possibly greater than |tracking_configuration.size()|).
  // |seed| and |device_id| are used to track preference value changes and must
  // be the same on each launch in order to verify loaded preference values.
  ProfilePrefStoreManager(
      const base::FilePath& profile_path,
      const std::vector<PrefHashFilter::TrackedPreferenceMetadata>&
          tracking_configuration,
      size_t reporting_ids_count,
      const std::string& seed,
      const std::string& device_id,
      PrefService* local_state);

  ~ProfilePrefStoreManager();

  static const bool kPlatformSupportsPreferenceTracking;

  // Register local state prefs used by the profile preferences system.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Register user prefs used by the profile preferences system.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Determines the user preferences filename for the profile at |profile_path|.
  static base::FilePath GetPrefFilePathFromProfilePath(
      const base::FilePath& profile_path);

  // Deletes stored hashes for all profiles from |local_state|.
  static void ResetAllPrefHashStores(PrefService* local_state);

  // Retrieves the time of the last preference reset event, if any, for
  // |pref_service|. Assumes that |pref_service| is backed by a PrefStore that
  // was built by ProfilePrefStoreManager.
  // If no reset has occurred, returns a null |Time|.
  static base::Time GetResetTime(PrefService* pref_service);

  // Clears the time of the last preference reset event, if any, for
  // |pref_service|. Assumes that |pref_service| is backed by a PrefStore that
  // was built by ProfilePrefStoreManager.
  static void ClearResetTime(PrefService* pref_service);

  // Deletes stored hashes for the managed profile.
  void ResetPrefHashStore();

  // Creates a PersistentPrefStore providing access to the user preferences of
  // the managed profile.
  PersistentPrefStore* CreateProfilePrefStore(
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);

  // Checks the presence/version of the hash store for the managed profile and
  // creates or updates it if necessary. Completes asynchronously and is safe if
  // the preferences/hash store are concurrently loaded/manipulated by another
  // task.
  void UpdateProfileHashStoreIfRequired(
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);

  // Initializes the preferences for the managed profile with the preference
  // values in |master_prefs|. Acts synchronously, including blocking IO.
  // Returns true on success.
  bool InitializePrefsFromMasterPrefs(
      const base::DictionaryValue& master_prefs);

  // Creates a single-file PrefStore as was used in M34 and earlier. Used only
  // for testing migration.
  PersistentPrefStore* CreateDeprecatedCombinedProfilePrefStore(
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);

 private:
  // Returns a PrefHashStoreImpl for the managed profile. Should only be called
  // if |kPlatformSupportsPreferenceTracking|.
  scoped_ptr<PrefHashStoreImpl> GetPrefHashStoreImpl();

  // Returns a PrefHashStore that is a copy of the current state of the real
  // hash store.
  scoped_ptr<PrefHashStore> CopyPrefHashStore();

  const base::FilePath profile_path_;
  const std::vector<PrefHashFilter::TrackedPreferenceMetadata>
      tracking_configuration_;
  const size_t reporting_ids_count_;
  const std::string seed_;
  const std::string device_id_;
  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(ProfilePrefStoreManager);
};

#endif  // CHROME_BROWSER_PREFS_PROFILE_PREF_STORE_MANAGER_H_
