// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/chrome_pref_service_factory.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/default_pref_store.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_filter.h"
#include "base/prefs/pref_notifier_impl.h"
#include "base/prefs/pref_registry.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_store.h"
#include "base/prefs/pref_value_store.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/pref_hash_filter.h"
#include "chrome/browser/prefs/pref_hash_store.h"
#include "chrome/browser/prefs/pref_hash_store_impl.h"
#include "chrome/browser/prefs/pref_model_associator.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/pref_service_syncable_factory.h"
#include "chrome/browser/profiles/file_path_verifier_win.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/pref_names.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/common/policy_types.h"
#endif

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/supervised_user_pref_store.h"
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

// The delay in seconds before actual work kicks in after calling
// SchedulePrefHashStoresUpdateCheck can be set to 0 for tests.
int g_pref_hash_store_update_check_delay_seconds = 55;

// These preferences must be kept in sync with the TrackedPreference enum in
// tools/metrics/histograms/histograms.xml. To add a new preference, append it
// to the array and add a corresponding value to the histogram enum. Each
// tracked preference must be given a unique reporting ID.
const PrefHashFilter::TrackedPreferenceMetadata kTrackedPrefs[] = {
  {
    0, prefs::kShowHomeButton,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
  {
    1, prefs::kHomePageIsNewTabPage,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
  {
    2, prefs::kHomePage,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
  {
    3, prefs::kRestoreOnStartup,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
  {
    4, prefs::kURLsToRestoreOnStartup,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
  {
    5, extensions::pref_names::kExtensions,
    PrefHashFilter::NO_ENFORCEMENT,
    PrefHashFilter::TRACKING_STRATEGY_SPLIT
  },
  {
    6, prefs::kGoogleServicesLastUsername,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
  {
    7, prefs::kSearchProviderOverrides,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
  {
    8, prefs::kDefaultSearchProviderSearchURL,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
  {
    9, prefs::kDefaultSearchProviderKeyword,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
  {
    10, prefs::kDefaultSearchProviderName,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
#if !defined(OS_ANDROID)
  {
    11, prefs::kPinnedTabs,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
#endif
  {
    12, extensions::pref_names::kKnownDisabled,
    PrefHashFilter::NO_ENFORCEMENT,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
  {
    13, prefs::kProfileResetPromptMemento,
    PrefHashFilter::ENFORCE_ON_LOAD,
    PrefHashFilter::TRACKING_STRATEGY_ATOMIC
  },
};

// The count of tracked preferences IDs across all platforms.
const size_t kTrackedPrefsReportingIDsCount = 14;
COMPILE_ASSERT(kTrackedPrefsReportingIDsCount >= arraysize(kTrackedPrefs),
               need_to_increment_ids_count);

enum SettingsEnforcementGroup {
  GROUP_NO_ENFORCEMENT,
  // Only enforce settings on profile loads; still allow seeding of unloaded
  // profiles.
  GROUP_ENFORCE_ON_LOAD,
  GROUP_ENFORCE_ALWAYS
};

SettingsEnforcementGroup GetSettingsEnforcementGroup() {
# if defined(OS_WIN)
  static bool first_call = true;
  static const bool is_enrolled_to_domain = base::win::IsEnrolledToDomain();
  if (first_call) {
    UMA_HISTOGRAM_BOOLEAN("Settings.TrackedPreferencesNoEnforcementOnDomain",
                          is_enrolled_to_domain);
    first_call = false;
  }
  if (is_enrolled_to_domain)
    return GROUP_NO_ENFORCEMENT;
#endif

  struct {
    const char* group_name;
    SettingsEnforcementGroup group;
  } static const kEnforcementLevelMap[] = {
    { chrome_prefs::internals::kSettingsEnforcementGroupNoEnforcement,
      GROUP_NO_ENFORCEMENT },
    { chrome_prefs::internals::kSettingsEnforcementGroupEnforceOnload,
      GROUP_ENFORCE_ON_LOAD },
    { chrome_prefs::internals::kSettingsEnforcementGroupEnforceAlways,
      GROUP_ENFORCE_ALWAYS },
  };

  // TODO(gab): Switch the default to GROUP_ENFORCE_ALWAYS.
  SettingsEnforcementGroup enforcement_group = GROUP_NO_ENFORCEMENT;
  bool group_determined_from_trial = false;
  base::FieldTrial* trial =
      base::FieldTrialList::Find(
          chrome_prefs::internals::kSettingsEnforcementTrialName);
  if (trial) {
    const std::string& group_name = trial->group_name();
    // ARRAYSIZE_UNSAFE must be used since the array is declared locally; it is
    // only unsafe because it could not trigger a compile error on some
    // non-array pointer types; this is fine since kEnforcementLevelMap is
    // clearly an array.
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kEnforcementLevelMap); ++i) {
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
#endif
  }
}

base::FilePath GetPrefFilePathFromProfilePath(
    const base::FilePath& profile_path) {
  return profile_path.Append(chrome::kPreferencesFilename);
}

// Returns the PrefHashStoreImpl for the profile at |profile_path|; may be NULL
// on some platforms.
scoped_ptr<PrefHashStoreImpl> GetPrefHashStoreImpl(
    const base::FilePath& profile_path) {
  // TODO(erikwright): Enable this on Android when race condition is sorted out.
#if defined(OS_ANDROID)
  return scoped_ptr<PrefHashStoreImpl>();
#else
  std::string seed = ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_PREF_HASH_SEED_BIN).as_string();
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

  return make_scoped_ptr(new PrefHashStoreImpl(
      profile_path.AsUTF8Unsafe(),
      seed,
      device_id,
      g_browser_process->local_state()));
#endif
}

void HandleResetEvent() {
  g_browser_process->local_state()->SetInt64(
      prefs::kProfilePreferenceResetTime,
      base::Time::Now().ToInternalValue());
}

scoped_ptr<PrefHashFilter> CreatePrefHashFilter(
    scoped_ptr<PrefHashStore> pref_hash_store) {
  const PrefHashFilter::EnforcementLevel enforcement_level =
      GetSettingsEnforcementGroup() == GROUP_NO_ENFORCEMENT ?
          PrefHashFilter::NO_ENFORCEMENT : PrefHashFilter::ENFORCE_ON_LOAD;
  return make_scoped_ptr(new PrefHashFilter(pref_hash_store.Pass(),
                                            kTrackedPrefs,
                                            arraysize(kTrackedPrefs),
                                            kTrackedPrefsReportingIDsCount,
                                            enforcement_level,
                                            base::Bind(&HandleResetEvent)));
}

void PrepareBuilder(
    PrefServiceSyncableFactory* factory,
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    policy::PolicyService* policy_service,
    ManagedUserSettingsService* managed_user_settings,
    scoped_ptr<PrefHashStore> pref_hash_store,
    const scoped_refptr<PrefStore>& extension_prefs,
    bool async) {
#if defined(ENABLE_CONFIGURATION_POLICY)
  using policy::ConfigurationPolicyPrefStore;
  factory->set_managed_prefs(
      make_scoped_refptr(new ConfigurationPolicyPrefStore(
          policy_service,
          g_browser_process->browser_policy_connector()->GetHandlerList(),
          policy::POLICY_LEVEL_MANDATORY)));
  factory->set_recommended_prefs(
      make_scoped_refptr(new ConfigurationPolicyPrefStore(
          policy_service,
          g_browser_process->browser_policy_connector()->GetHandlerList(),
          policy::POLICY_LEVEL_RECOMMENDED)));
#endif  // ENABLE_CONFIGURATION_POLICY

#if defined(ENABLE_MANAGED_USERS)
  if (managed_user_settings) {
    factory->set_supervised_user_prefs(
        make_scoped_refptr(new SupervisedUserPrefStore(managed_user_settings)));
  }
#endif

  factory->set_async(async);
  factory->set_extension_prefs(extension_prefs);
  factory->set_command_line_prefs(
      make_scoped_refptr(
          new CommandLinePrefStore(CommandLine::ForCurrentProcess())));
  factory->set_read_error_callback(base::Bind(&HandleReadError));
  scoped_ptr<PrefFilter> pref_filter;
  if (pref_hash_store)
    pref_filter = CreatePrefHashFilter(pref_hash_store.Pass());
  factory->set_user_prefs(
      new JsonPrefStore(
          pref_filename,
          pref_io_task_runner,
          pref_filter.Pass()));
}

// An in-memory PrefStore backed by an immutable DictionaryValue.
class DictionaryPrefStore : public PrefStore {
 public:
  explicit DictionaryPrefStore(const base::DictionaryValue* dictionary)
      : dictionary_(dictionary) {}

  virtual bool GetValue(const std::string& key,
                        const base::Value** result) const OVERRIDE {
    const base::Value* tmp = NULL;
    if (!dictionary_->Get(key, &tmp))
      return false;

    if (result)
      *result = tmp;
    return true;
  }

 private:
  virtual ~DictionaryPrefStore() {}

  const base::DictionaryValue* dictionary_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryPrefStore);
};

// Waits for a PrefStore to be initialized and then initializes the
// corresponding PrefHashStore.
// The observer deletes itself when its work is completed.
class InitializeHashStoreObserver : public PrefStore::Observer {
 public:
  // Creates an observer that will initialize |pref_hash_store| with the
  // contents of |pref_store| when the latter is fully loaded.
  InitializeHashStoreObserver(
      const scoped_refptr<PrefStore>& pref_store,
      scoped_ptr<PrefHashStoreImpl> pref_hash_store_impl)
      : pref_store_(pref_store),
        pref_hash_store_impl_(pref_hash_store_impl.Pass()) {}

  virtual ~InitializeHashStoreObserver();

  // PrefStore::Observer implementation.
  virtual void OnPrefValueChanged(const std::string& key) OVERRIDE;
  virtual void OnInitializationCompleted(bool succeeded) OVERRIDE;

 private:
  scoped_refptr<PrefStore> pref_store_;
  scoped_ptr<PrefHashStoreImpl> pref_hash_store_impl_;

  DISALLOW_COPY_AND_ASSIGN(InitializeHashStoreObserver);
};

InitializeHashStoreObserver::~InitializeHashStoreObserver() {}

void InitializeHashStoreObserver::OnPrefValueChanged(const std::string& key) {}

void InitializeHashStoreObserver::OnInitializationCompleted(bool succeeded) {
  // If we successfully loaded the preferences _and_ the PrefHashStoreImpl
  // hasn't been initialized by someone else in the meantime, initialize it now.
  const PrefHashStoreImpl::StoreVersion pre_update_version =
      pref_hash_store_impl_->GetCurrentVersion();
  if (succeeded && pre_update_version < PrefHashStoreImpl::VERSION_LATEST) {
    CreatePrefHashFilter(pref_hash_store_impl_.PassAs<PrefHashStore>())->
        Initialize(*pref_store_);
    UMA_HISTOGRAM_ENUMERATION(
        "Settings.TrackedPreferencesAlternateStoreVersionUpdatedFrom",
        pre_update_version,
        PrefHashStoreImpl::VERSION_LATEST + 1);
  }
  pref_store_->RemoveObserver(this);
  delete this;
}

// Initializes/updates the PrefHashStore for the profile preferences file under
// |profile_path| without actually loading that profile. Also reports the
// version of that PrefHashStore via UMA, whether it proceeds with initializing
// it or not.
void UpdatePrefHashStoreIfRequired(
    const base::FilePath& profile_path) {
  scoped_ptr<PrefHashStoreImpl> pref_hash_store_impl(
      GetPrefHashStoreImpl(profile_path));

  const PrefHashStoreImpl::StoreVersion current_version =
      pref_hash_store_impl->GetCurrentVersion();
  UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferencesAlternateStoreVersion",
                            current_version,
                            PrefHashStoreImpl::VERSION_LATEST + 1);

  // Update the pref hash store if it's not at the latest version and the
  // SettingsEnforcement group allows seeding of unloaded profiles.
  if (current_version != PrefHashStoreImpl::VERSION_LATEST &&
      GetSettingsEnforcementGroup() < GROUP_ENFORCE_ALWAYS) {
    const base::FilePath pref_file(
        GetPrefFilePathFromProfilePath(profile_path));
    scoped_refptr<JsonPrefStore> pref_store(
        new JsonPrefStore(pref_file,
                          JsonPrefStore::GetTaskRunnerForFile(
                              pref_file, BrowserThread::GetBlockingPool()),
                          scoped_ptr<PrefFilter>()));
    pref_store->AddObserver(
        new InitializeHashStoreObserver(pref_store,
                                        pref_hash_store_impl.Pass()));
    pref_store->ReadPrefsAsync(NULL);
  }
}

// Initialize/update preference hash stores for all profiles but the one whose
// path matches |ignored_profile_path|.
void UpdateAllPrefHashStoresIfRequired(
    const base::FilePath& ignored_profile_path) {
  const ProfileInfoCache& profile_info_cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  const size_t n_profiles = profile_info_cache.GetNumberOfProfiles();
  for (size_t i = 0; i < n_profiles; ++i) {
    const base::FilePath profile_path =
        profile_info_cache.GetPathOfProfileAtIndex(i);
    if (profile_path != ignored_profile_path)
      UpdatePrefHashStoreIfRequired(profile_path);
  }
}

}  // namespace

namespace chrome_prefs {

namespace internals {

const char kSettingsEnforcementTrialName[] = "SettingsEnforcement";
const char kSettingsEnforcementGroupNoEnforcement[] = "no_enforcement";
const char kSettingsEnforcementGroupEnforceOnload[] = "enforce_on_load";
const char kSettingsEnforcementGroupEnforceAlways[] = "enforce_always";

}  // namespace internals

scoped_ptr<PrefService> CreateLocalState(
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    policy::PolicyService* policy_service,
    const scoped_refptr<PrefRegistry>& pref_registry,
    bool async) {
  PrefServiceSyncableFactory factory;
  PrepareBuilder(&factory,
                 pref_filename,
                 pref_io_task_runner,
                 policy_service,
                 NULL,
                 scoped_ptr<PrefHashStore>(),
                 NULL,
                 async);
  return factory.Create(pref_registry.get());
}

scoped_ptr<PrefServiceSyncable> CreateProfilePrefs(
    const base::FilePath& profile_path,
    base::SequencedTaskRunner* pref_io_task_runner,
    policy::PolicyService* policy_service,
    ManagedUserSettingsService* managed_user_settings,
    const scoped_refptr<PrefStore>& extension_prefs,
    const scoped_refptr<user_prefs::PrefRegistrySyncable>& pref_registry,
    bool async) {
  TRACE_EVENT0("browser", "chrome_prefs::CreateProfilePrefs");
  PrefServiceSyncableFactory factory;
  PrepareBuilder(&factory,
                 GetPrefFilePathFromProfilePath(profile_path),
                 pref_io_task_runner,
                 policy_service,
                 managed_user_settings,
                 GetPrefHashStoreImpl(profile_path).PassAs<PrefHashStore>(),
                 extension_prefs,
                 async);
  return factory.CreateSyncable(pref_registry.get());
}

void SchedulePrefsFilePathVerification(const base::FilePath& profile_path) {
#if defined(OS_WIN)
  // Only do prefs file verification on Windows.
  const int kVerifyPrefsFileDelaySeconds = 60;
  BrowserThread::GetBlockingPool()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&VerifyPreferencesFile,
                   GetPrefFilePathFromProfilePath(profile_path)),
        base::TimeDelta::FromSeconds(kVerifyPrefsFileDelaySeconds));
#endif
}

void EnableZeroDelayPrefHashStoreUpdateForTesting() {
  g_pref_hash_store_update_check_delay_seconds = 0;
}

void SchedulePrefHashStoresUpdateCheck(
    const base::FilePath& initial_profile_path) {
  BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&UpdateAllPrefHashStoresIfRequired,
                   initial_profile_path),
        base::TimeDelta::FromSeconds(
            g_pref_hash_store_update_check_delay_seconds));
}

void ResetPrefHashStore(const base::FilePath& profile_path) {
  GetPrefHashStoreImpl(profile_path)->Reset();
}

bool InitializePrefsFromMasterPrefs(
    const base::FilePath& profile_path,
    const base::DictionaryValue& master_prefs) {
  // Create the profile directory if it doesn't exist yet (very possible on
  // first run).
  if (!base::CreateDirectory(profile_path))
    return false;

  JSONFileValueSerializer serializer(
      GetPrefFilePathFromProfilePath(profile_path));

  // Call Serialize (which does IO) on the main thread, which would _normally_
  // be verboten. In this case however, we require this IO to synchronously
  // complete before Chrome can start (as master preferences seed the Local
  // State and Preferences files). This won't trip ThreadIORestrictions as they
  // won't have kicked in yet on the main thread.
  bool success = serializer.Serialize(master_prefs);

  if (success) {
    scoped_refptr<const PrefStore> pref_store(
        new DictionaryPrefStore(&master_prefs));
    CreatePrefHashFilter(
        GetPrefHashStoreImpl(profile_path).PassAs<PrefHashStore>())->
            Initialize(*pref_store);
  }

  UMA_HISTOGRAM_BOOLEAN("Settings.InitializedFromMasterPrefs", success);
  return success;
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kProfilePreferenceResetTime, 0L);
  PrefHashStoreImpl::RegisterPrefs(registry);
}

}  // namespace chrome_prefs
