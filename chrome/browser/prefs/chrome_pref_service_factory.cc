// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/chrome_pref_service_factory.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/chrome_pref_model_associator_client.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/profile_pref_store_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/component_updater/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/pref_value_store.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/default_search_pref_migration.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync_driver/pref_names.h"
#include "components/syncable_prefs/pref_model_associator.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/syncable_prefs/pref_service_syncable_factory.h"
#include "components/user_prefs/tracked/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "grit/browser_resources.h"
#include "sync/internal_api/public/base/model_type.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/pref_names.h"
#endif

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_pref_store.h"
#endif

#if defined(OS_WIN)
#include "base/win/win_util.h"
#if defined(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"
#endif  // defined(ENABLE_RLZ)
#endif  // defined(OS_WIN)

using content::BrowserContext;
using content::BrowserThread;

namespace {

#if defined(OS_WIN)
// Whether we are in testing mode; can be enabled via
// DisableDomainCheckForTesting(). Forces startup checks to ignore the presence
// of a domain when determining the active SettingsEnforcement group.
bool g_disable_domain_check_for_testing = false;
#endif  // OS_WIN

// These preferences must be kept in sync with the TrackedPreference enum in
// tools/metrics/histograms/histograms.xml. To add a new preference, append it
// to the array and add a corresponding value to the histogram enum. Each
// tracked preference must be given a unique reporting ID.
// See CleanupDeprecatedTrackedPreferences() in pref_hash_filter.cc to remove a
// deprecated tracked preference.
const PrefHashFilter::TrackedPreferenceMetadata kTrackedPrefs[] = {
  {
    0, prefs::kShowHomeButton,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
  {
    1, prefs::kHomePageIsNewTabPage,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
  {
    2, prefs::kHomePage,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
  {
    3, prefs::kRestoreOnStartup,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
  {
    4, prefs::kURLsToRestoreOnStartup,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
#if defined(ENABLE_EXTENSIONS)
  {
    5, extensions::pref_names::kExtensions,
    PrefHashFilter::NO_ENFORCEMENT,
    PrefHashFilter::TRACKING_STRATEGY_SPLIT,
    PrefHashFilter::VALUE_IMPERSONAL
  },
#endif
  {
    6, prefs::kGoogleServicesLastUsername,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_PERSONAL
  },
  {
    7, prefs::kSearchProviderOverrides,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
  {
    8, prefs::kDefaultSearchProviderSearchURL,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
  {
    9, prefs::kDefaultSearchProviderKeyword,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
  {
    10, prefs::kDefaultSearchProviderName,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
#if !defined(OS_ANDROID)
  {
    11, prefs::kPinnedTabs,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
#endif
  {
    14, DefaultSearchManager::kDefaultSearchProviderDataPrefName,
    PrefHashFilter::NO_ENFORCEMENT,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
  {
    // Protecting kPreferenceResetTime does two things:
    //  1) It ensures this isn't accidently set by someone stomping the pref
    //     file.
    //  2) More importantly, it declares kPreferenceResetTime as a protected
    //     pref which is required for it to be visible when queried via the
    //     SegregatedPrefStore. This is because it's written directly in the
    //     protected JsonPrefStore by that store's PrefHashFilter if there was
    //     a reset in FilterOnLoad and SegregatedPrefStore will not look for it
    //     in the protected JsonPrefStore unless it's declared as a protected
    //     preference here.
    15, user_prefs::kPreferenceResetTime,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
  {
    17, sync_driver::prefs::kSyncRemainingRollbackTries,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
  {
    18, prefs::kSafeBrowsingIncidentsSent,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
#if defined(OS_WIN)
  {
    19, prefs::kSwReporterPromptVersion,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
#endif
  // This pref is deprecated and will be removed a few releases after M43.
  // kGoogleServicesAccountId replaces it.
  {
    21, prefs::kGoogleServicesUsername,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_PERSONAL
  },
#if defined(OS_WIN)
  {
    22, prefs::kSwReporterPromptSeed,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_IMPERSONAL
  },
#endif
  {
    23, prefs::kGoogleServicesAccountId,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_PERSONAL
  },
  {
    24, prefs::kGoogleServicesLastAccountId,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC,
    PrefHashFilter::VALUE_PERSONAL
  },
  // See note at top, new items added here also need to be added to
  // histograms.xml's TrackedPreference enum.
};

// One more than the last tracked preferences ID above.
const size_t kTrackedPrefsReportingIDsCount =
    kTrackedPrefs[arraysize(kTrackedPrefs) - 1].reporting_id + 1;

// Each group enforces a superset of the protection provided by the previous
// one.
enum SettingsEnforcementGroup {
  GROUP_NO_ENFORCEMENT,
  // Enforce protected settings on profile loads.
  GROUP_ENFORCE_ALWAYS,
  // Also enforce extension default search.
  GROUP_ENFORCE_ALWAYS_WITH_DSE,
  // Also enforce extension settings and default search.
  GROUP_ENFORCE_ALWAYS_WITH_EXTENSIONS_AND_DSE,
  // The default enforcement group contains all protection features.
  GROUP_ENFORCE_DEFAULT
};

SettingsEnforcementGroup GetSettingsEnforcementGroup() {
# if defined(OS_WIN)
  if (!g_disable_domain_check_for_testing) {
    static bool first_call = true;
    static const bool is_enrolled_to_domain = base::win::IsEnrolledToDomain();
    if (first_call) {
      UMA_HISTOGRAM_BOOLEAN("Settings.TrackedPreferencesNoEnforcementOnDomain",
                            is_enrolled_to_domain);
      first_call = false;
    }
    if (is_enrolled_to_domain)
      return GROUP_NO_ENFORCEMENT;
  }
#endif

  struct {
    const char* group_name;
    SettingsEnforcementGroup group;
  } static const kEnforcementLevelMap[] = {
    { chrome_prefs::internals::kSettingsEnforcementGroupNoEnforcement,
      GROUP_NO_ENFORCEMENT },
    { chrome_prefs::internals::kSettingsEnforcementGroupEnforceAlways,
      GROUP_ENFORCE_ALWAYS },
    { chrome_prefs::internals::
          kSettingsEnforcementGroupEnforceAlwaysWithDSE,
      GROUP_ENFORCE_ALWAYS_WITH_DSE },
    { chrome_prefs::internals::
          kSettingsEnforcementGroupEnforceAlwaysWithExtensionsAndDSE,
      GROUP_ENFORCE_ALWAYS_WITH_EXTENSIONS_AND_DSE },
  };

  // Use the strongest enforcement setting in the absence of a field trial
  // config on Windows. Remember to update the OFFICIAL_BUILD section of
  // extension_startup_browsertest.cc and pref_hash_browsertest.cc when updating
  // the default value below.
  // TODO(gab): Enforce this on all platforms.
  SettingsEnforcementGroup enforcement_group =
#if defined(OS_WIN)
      GROUP_ENFORCE_DEFAULT;
#else
      GROUP_NO_ENFORCEMENT;
#endif
  bool group_determined_from_trial = false;
  base::FieldTrial* trial =
      base::FieldTrialList::Find(
          chrome_prefs::internals::kSettingsEnforcementTrialName);
  if (trial) {
    const std::string& group_name = trial->group_name();
    for (size_t i = 0; i < arraysize(kEnforcementLevelMap); ++i) {
      if (kEnforcementLevelMap[i].group_name == group_name) {
        enforcement_group = kEnforcementLevelMap[i].group;
        group_determined_from_trial = true;
        break;
      }
    }
  }
  UMA_HISTOGRAM_BOOLEAN("Settings.EnforcementGroupDeterminedFromTrial",
                        group_determined_from_trial);
  return enforcement_group;
}

// Returns the effective preference tracking configuration.
std::vector<PrefHashFilter::TrackedPreferenceMetadata>
GetTrackingConfiguration() {
  const SettingsEnforcementGroup enforcement_group =
      GetSettingsEnforcementGroup();

  std::vector<PrefHashFilter::TrackedPreferenceMetadata> result;
  for (size_t i = 0; i < arraysize(kTrackedPrefs); ++i) {
    PrefHashFilter::TrackedPreferenceMetadata data = kTrackedPrefs[i];

    if (GROUP_NO_ENFORCEMENT == enforcement_group) {
      // Remove enforcement for all tracked preferences.
      data.enforcement_level = PrefHashFilter::NO_ENFORCEMENT;
    }

    if (enforcement_group >= GROUP_ENFORCE_ALWAYS_WITH_DSE &&
        data.name == DefaultSearchManager::kDefaultSearchProviderDataPrefName) {
      // Specifically enable default search settings enforcement.
      data.enforcement_level = PrefHashFilter::ENFORCE_ON_LOAD;
    }

#if defined(ENABLE_EXTENSIONS)
    if (enforcement_group >= GROUP_ENFORCE_ALWAYS_WITH_EXTENSIONS_AND_DSE &&
        data.name == extensions::pref_names::kExtensions) {
      // Specifically enable extension settings enforcement.
      data.enforcement_level = PrefHashFilter::ENFORCE_ON_LOAD;
    }
#endif

    result.push_back(data);
  }
  return result;
}

// Shows notifications which correspond to PersistentPrefStore's reading errors.
void HandleReadError(PersistentPrefStore::PrefReadError error) {
  // Sample the histogram also for the successful case in order to get a
  // baseline on the success rate in addition to the error distribution.
  UMA_HISTOGRAM_ENUMERATION("PrefService.ReadError", error,
                            PersistentPrefStore::PREF_READ_ERROR_MAX_ENUM);

  if (error != PersistentPrefStore::PREF_READ_ERROR_NONE) {
#if !defined(OS_CHROMEOS)
    // Failing to load prefs on startup is a bad thing(TM). See bug 38352 for
    // an example problem that this can cause.
    // Do some diagnosis and try to avoid losing data.
    int message_id = 0;
    if (error <= PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE) {
      message_id = IDS_PREFERENCES_CORRUPT_ERROR;
    } else if (error != PersistentPrefStore::PREF_READ_ERROR_NO_FILE) {
      message_id = IDS_PREFERENCES_UNREADABLE_ERROR;
    }

    if (message_id) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::Bind(&ShowProfileErrorDialog,
                                         PROFILE_ERROR_PREFERENCES,
                                         message_id));
    }
#else
    // On ChromeOS error screen with message about broken local state
    // will be displayed.

    // A supplementary error message about broken local state - is included
    // in logs and user feedbacks.
    if (error != PersistentPrefStore::PREF_READ_ERROR_NONE &&
        error != PersistentPrefStore::PREF_READ_ERROR_NO_FILE) {
      LOG(ERROR) << "An error happened during prefs loading: " << error;
    }
#endif
  }
}

scoped_ptr<ProfilePrefStoreManager> CreateProfilePrefStoreManager(
    const base::FilePath& profile_path) {
  std::string device_id;
#if defined(OS_WIN) && defined(ENABLE_RLZ)
  // This is used by
  // chrome/browser/extensions/api/music_manager_private/device_id_win.cc
  // but that API is private (http://crbug.com/276485) and other platforms are
  // not available synchronously.
  // As part of improving pref metrics on other platforms we may want to find
  // ways to defer preference loading until the device ID can be used.
  rlz_lib::GetMachineId(&device_id);
#endif
  std::string seed;
#if defined(GOOGLE_CHROME_BUILD)
  seed = ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_PREF_HASH_SEED_BIN).as_string();
#endif
  return make_scoped_ptr(new ProfilePrefStoreManager(
      profile_path,
      GetTrackingConfiguration(),
      kTrackedPrefsReportingIDsCount,
      seed,
      device_id,
      g_browser_process->local_state()));
}

void PrepareFactory(
    syncable_prefs::PrefServiceSyncableFactory* factory,
    policy::PolicyService* policy_service,
    SupervisedUserSettingsService* supervised_user_settings,
    scoped_refptr<PersistentPrefStore> user_pref_store,
    const scoped_refptr<PrefStore>& extension_prefs,
    bool async) {
#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::BrowserPolicyConnector* policy_connector =
      g_browser_process->browser_policy_connector();
  factory->SetManagedPolicies(policy_service, policy_connector);
  factory->SetRecommendedPolicies(policy_service, policy_connector);
#endif  // ENABLE_CONFIGURATION_POLICY

#if defined(ENABLE_SUPERVISED_USERS)
  if (supervised_user_settings) {
    scoped_refptr<PrefStore> supervised_user_prefs = make_scoped_refptr(
        new SupervisedUserPrefStore(supervised_user_settings));
    // TODO(bauerb): Temporary CHECK while investigating
    // https://crbug.com/425785. Remove when that bug is fixed.
    CHECK(async || supervised_user_prefs->IsInitializationComplete());
    factory->set_supervised_user_prefs(supervised_user_prefs);
  }
#endif

  factory->set_async(async);
  factory->set_extension_prefs(extension_prefs);
  factory->set_command_line_prefs(make_scoped_refptr(
      new CommandLinePrefStore(base::CommandLine::ForCurrentProcess())));
  factory->set_read_error_callback(base::Bind(&HandleReadError));
  factory->set_user_prefs(user_pref_store);
  factory->SetPrefModelAssociatorClient(
      ChromePrefModelAssociatorClient::GetInstance());
}

}  // namespace

namespace chrome_prefs {

namespace internals {

// Group modifications should be reflected in first_run_browsertest.cc and
// pref_hash_browsertest.cc.
const char kSettingsEnforcementTrialName[] = "SettingsEnforcement";
const char kSettingsEnforcementGroupNoEnforcement[] = "no_enforcement";
const char kSettingsEnforcementGroupEnforceAlways[] = "enforce_always";
const char kSettingsEnforcementGroupEnforceAlwaysWithDSE[] =
    "enforce_always_with_dse";
const char kSettingsEnforcementGroupEnforceAlwaysWithExtensionsAndDSE[] =
    "enforce_always_with_extensions_and_dse";

}  // namespace internals

scoped_ptr<PrefService> CreateLocalState(
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    policy::PolicyService* policy_service,
    const scoped_refptr<PrefRegistry>& pref_registry,
    bool async) {
  syncable_prefs::PrefServiceSyncableFactory factory;
  PrepareFactory(
      &factory,
      policy_service,
      NULL,  // supervised_user_settings
      new JsonPrefStore(
          pref_filename, pref_io_task_runner, scoped_ptr<PrefFilter>()),
      NULL,  // extension_prefs
      async);
  return factory.Create(pref_registry.get());
}

scoped_ptr<syncable_prefs::PrefServiceSyncable> CreateProfilePrefs(
    const base::FilePath& profile_path,
    base::SequencedTaskRunner* pref_io_task_runner,
    TrackedPreferenceValidationDelegate* validation_delegate,
    policy::PolicyService* policy_service,
    SupervisedUserSettingsService* supervised_user_settings,
    const scoped_refptr<PrefStore>& extension_prefs,
    const scoped_refptr<user_prefs::PrefRegistrySyncable>& pref_registry,
    bool async) {
  TRACE_EVENT0("browser", "chrome_prefs::CreateProfilePrefs");
  SCOPED_UMA_HISTOGRAM_TIMER("PrefService.CreateProfilePrefsTime");

  // A StartSyncFlare used to kick sync early in case of a reset event. This is
  // done since sync may bring back the user's server value post-reset which
  // could potentially cause a "settings flash" between the factory default and
  // the re-instantiated server value. Starting sync ASAP minimizes the window
  // before the server value is re-instantiated (this window can otherwise be
  // as long as 10 seconds by default).
  const base::Closure start_sync_flare_for_prefs =
      base::Bind(sync_start_util::GetFlareForSyncableService(profile_path),
                 syncer::PREFERENCES);

  syncable_prefs::PrefServiceSyncableFactory factory;
  scoped_refptr<PersistentPrefStore> user_pref_store(
      CreateProfilePrefStoreManager(profile_path)
          ->CreateProfilePrefStore(pref_io_task_runner,
                                   start_sync_flare_for_prefs,
                                   validation_delegate));
  PrepareFactory(&factory,
                 policy_service,
                 supervised_user_settings,
                 user_pref_store,
                 extension_prefs,
                 async);
  scoped_ptr<syncable_prefs::PrefServiceSyncable> pref_service =
      factory.CreateSyncable(pref_registry.get());

  ConfigureDefaultSearchPrefMigrationToDictionaryValue(pref_service.get());

  return pref_service;
}

void DisableDomainCheckForTesting() {
#if defined(OS_WIN)
  g_disable_domain_check_for_testing = true;
#endif  // OS_WIN
}

bool InitializePrefsFromMasterPrefs(
    const base::FilePath& profile_path,
    const base::DictionaryValue& master_prefs) {
  return CreateProfilePrefStoreManager(profile_path)
      ->InitializePrefsFromMasterPrefs(master_prefs);
}

base::Time GetResetTime(Profile* profile) {
  return ProfilePrefStoreManager::GetResetTime(profile->GetPrefs());
}

void ClearResetTime(Profile* profile) {
  ProfilePrefStoreManager::ClearResetTime(profile->GetPrefs());
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  ProfilePrefStoreManager::RegisterProfilePrefs(registry);
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  ProfilePrefStoreManager::RegisterPrefs(registry);
}

}  // namespace chrome_prefs
