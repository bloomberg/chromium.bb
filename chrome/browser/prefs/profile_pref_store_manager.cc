// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/profile_pref_store_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/user_prefs/tracked/pref_hash_store_impl.h"
#include "components/user_prefs/tracked/segregated_pref_store.h"
#include "components/user_prefs/tracked/tracked_preferences_migration.h"

#if defined(OS_WIN)
#include "chrome/installer/util/browser_distribution.h"
#include "components/user_prefs/tracked/registry_hash_store_contents_win.h"
#endif

namespace {

void RemoveValueSilently(const base::WeakPtr<JsonPrefStore> pref_store,
                         const std::string& key) {
  if (pref_store) {
    pref_store->RemoveValueSilently(
        key, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}

#if defined(OS_WIN)
// Forces a different registry key to be used for storing preference validation
// MACs. See |SetPreferenceValidationRegistryPathForTesting|.
const base::string16* g_preference_validation_registry_path_for_testing =
    nullptr;
#endif  // OS_WIN

}  // namespace

// Preference tracking and protection is not required on platforms where other
// apps do not have access to chrome's persistent storage.
const bool ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking =
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    false;
#else
    true;
#endif

ProfilePrefStoreManager::ProfilePrefStoreManager(
    const base::FilePath& profile_path,
    const std::vector<PrefHashFilter::TrackedPreferenceMetadata>&
        tracking_configuration,
    size_t reporting_ids_count,
    const std::string& seed,
    const std::string& legacy_device_id,
    PrefService* local_state)
    : profile_path_(profile_path),
      tracking_configuration_(tracking_configuration),
      reporting_ids_count_(reporting_ids_count),
      seed_(seed),
      legacy_device_id_(legacy_device_id),
      local_state_(local_state) {}

ProfilePrefStoreManager::~ProfilePrefStoreManager() {}

// static
void ProfilePrefStoreManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  PrefHashFilter::RegisterProfilePrefs(registry);
}

//  static
base::Time ProfilePrefStoreManager::GetResetTime(PrefService* pref_service) {
  return PrefHashFilter::GetResetTime(pref_service);
}

// static
void ProfilePrefStoreManager::ClearResetTime(PrefService* pref_service) {
  PrefHashFilter::ClearResetTime(pref_service);
}

#if defined(OS_WIN)
// static
void ProfilePrefStoreManager::SetPreferenceValidationRegistryPathForTesting(
    const base::string16* path) {
  DCHECK(!path->empty());
  g_preference_validation_registry_path_for_testing = path;
}
#endif  // OS_WIN

PersistentPrefStore* ProfilePrefStoreManager::CreateProfilePrefStore(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    const base::Closure& on_reset_on_load,
    TrackedPreferenceValidationDelegate* validation_delegate) {
  std::unique_ptr<PrefFilter> pref_filter;
  if (!kPlatformSupportsPreferenceTracking) {
    return new JsonPrefStore(profile_path_.Append(chrome::kPreferencesFilename),
                             io_task_runner.get(),
                             std::unique_ptr<PrefFilter>());
  }

  std::vector<PrefHashFilter::TrackedPreferenceMetadata>
      unprotected_configuration;
  std::vector<PrefHashFilter::TrackedPreferenceMetadata>
      protected_configuration;
  std::set<std::string> protected_pref_names;
  std::set<std::string> unprotected_pref_names;
  for (std::vector<PrefHashFilter::TrackedPreferenceMetadata>::const_iterator
           it = tracking_configuration_.begin();
       it != tracking_configuration_.end();
       ++it) {
    if (it->enforcement_level > PrefHashFilter::NO_ENFORCEMENT) {
      protected_configuration.push_back(*it);
      protected_pref_names.insert(it->name);
    } else {
      unprotected_configuration.push_back(*it);
      unprotected_pref_names.insert(it->name);
    }
  }

  std::unique_ptr<PrefHashFilter> unprotected_pref_hash_filter(
      new PrefHashFilter(GetPrefHashStore(false),
                         GetExternalVerificationPrefHashStorePair(),
                         unprotected_configuration, base::Closure(),
                         validation_delegate, reporting_ids_count_, false));
  std::unique_ptr<PrefHashFilter> protected_pref_hash_filter(new PrefHashFilter(
      GetPrefHashStore(true), GetExternalVerificationPrefHashStorePair(),
      protected_configuration, on_reset_on_load, validation_delegate,
      reporting_ids_count_, true));

  PrefHashFilter* raw_unprotected_pref_hash_filter =
      unprotected_pref_hash_filter.get();
  PrefHashFilter* raw_protected_pref_hash_filter =
      protected_pref_hash_filter.get();

  scoped_refptr<JsonPrefStore> unprotected_pref_store(new JsonPrefStore(
      profile_path_.Append(chrome::kPreferencesFilename), io_task_runner.get(),
      std::move(unprotected_pref_hash_filter)));
  // TODO(gab): Remove kDeprecatedProtectedPreferencesFilename as an alternate
  // file in M40+.
  scoped_refptr<JsonPrefStore> protected_pref_store(new JsonPrefStore(
      profile_path_.Append(chrome::kSecurePreferencesFilename),
      profile_path_.Append(chrome::kProtectedPreferencesFilenameDeprecated),
      io_task_runner.get(), std::move(protected_pref_hash_filter)));

  SetupTrackedPreferencesMigration(
      unprotected_pref_names, protected_pref_names,
      base::Bind(&RemoveValueSilently, unprotected_pref_store->AsWeakPtr()),
      base::Bind(&RemoveValueSilently, protected_pref_store->AsWeakPtr()),
      base::Bind(&JsonPrefStore::RegisterOnNextSuccessfulWriteReply,
                 unprotected_pref_store->AsWeakPtr()),
      base::Bind(&JsonPrefStore::RegisterOnNextSuccessfulWriteReply,
                 protected_pref_store->AsWeakPtr()),
      GetPrefHashStore(false), GetPrefHashStore(true),
      raw_unprotected_pref_hash_filter, raw_protected_pref_hash_filter);

  return new SegregatedPrefStore(unprotected_pref_store, protected_pref_store,
                                 protected_pref_names);
}

bool ProfilePrefStoreManager::InitializePrefsFromMasterPrefs(
    std::unique_ptr<base::DictionaryValue> master_prefs) {
  // Create the profile directory if it doesn't exist yet (very possible on
  // first run).
  if (!base::CreateDirectory(profile_path_))
    return false;

  if (kPlatformSupportsPreferenceTracking) {
    PrefHashFilter(GetPrefHashStore(false),
                   GetExternalVerificationPrefHashStorePair(),
                   tracking_configuration_, base::Closure(), NULL,
                   reporting_ids_count_, false)
        .Initialize(master_prefs.get());
  }

  // This will write out to a single combined file which will be immediately
  // migrated to two files on load.
  JSONFileValueSerializer serializer(
      profile_path_.Append(chrome::kPreferencesFilename));

  // Call Serialize (which does IO) on the main thread, which would _normally_
  // be verboten. In this case however, we require this IO to synchronously
  // complete before Chrome can start (as master preferences seed the Local
  // State and Preferences files). This won't trip ThreadIORestrictions as they
  // won't have kicked in yet on the main thread.
  bool success = serializer.Serialize(*master_prefs);

  UMA_HISTOGRAM_BOOLEAN("Settings.InitializedFromMasterPrefs", success);
  return success;
}

std::unique_ptr<PrefHashStore> ProfilePrefStoreManager::GetPrefHashStore(
    bool use_super_mac) {
  DCHECK(kPlatformSupportsPreferenceTracking);

  return std::unique_ptr<PrefHashStore>(
      new PrefHashStoreImpl(seed_, legacy_device_id_, use_super_mac));
}

std::pair<std::unique_ptr<PrefHashStore>, std::unique_ptr<HashStoreContents>>
ProfilePrefStoreManager::GetExternalVerificationPrefHashStorePair() {
  DCHECK(kPlatformSupportsPreferenceTracking);
#if defined(OS_WIN)
  return std::make_pair(
      base::MakeUnique<PrefHashStoreImpl>(
          "ChromeRegistryHashStoreValidationSeed", legacy_device_id_,
          false /* use_super_mac */),
      g_preference_validation_registry_path_for_testing
          ? base::MakeUnique<RegistryHashStoreContentsWin>(
                *g_preference_validation_registry_path_for_testing,
                profile_path_.BaseName().LossyDisplayName())
          : base::MakeUnique<RegistryHashStoreContentsWin>(
                BrowserDistribution::GetDistribution()->GetRegistryPath(),
                profile_path_.BaseName().LossyDisplayName()));
#else
  return std::make_pair(nullptr, nullptr);
#endif
}
