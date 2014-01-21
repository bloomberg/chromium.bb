// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/chrome_pref_service_factory.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/default_pref_store.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_notifier_impl.h"
#include "base/prefs/pref_registry.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_value_store.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/pref_hash_filter.h"
#include "chrome/browser/prefs/pref_hash_store.h"
#include "chrome/browser/prefs/pref_model_associator.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/pref_service_syncable_factory.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/common/policy_types.h"
#endif

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/supervised_user_pref_store.h"
#endif

using content::BrowserContext;
using content::BrowserThread;

namespace {

// These preferences must be kept in sync with the TrackedPreference enum in
// tools/metrics/histograms/histograms.xml. To add a new preference, append it
// to the array and add a corresponding value to the histogram enum. Each
// tracked preference must be given a unique reporting ID.
const PrefHashFilter::TrackedPreference kTrackedPrefs[] = {
  { 0, prefs::kShowHomeButton, true },
  { 1, prefs::kHomePageIsNewTabPage, true },
  { 2, prefs::kHomePage, true },
  { 3, prefs::kRestoreOnStartup, true },
  { 4, prefs::kURLsToRestoreOnStartup, true },
  { 5, extensions::pref_names::kExtensions, false },
  { 6, prefs::kGoogleServicesLastUsername, true },
  { 7, prefs::kSearchProviderOverrides, true },
  { 8, prefs::kDefaultSearchProviderSearchURL, true },
  { 9, prefs::kDefaultSearchProviderKeyword, true },
  { 10, prefs::kDefaultSearchProviderName, true },
#if !defined(OS_ANDROID)
  { 11, prefs::kPinnedTabs, true },
#endif
  { 12, extensions::pref_names::kKnownDisabled, true },
  { 13, prefs::kProfileResetPromptMemento, true },
};

// The count of tracked preferences IDs across all platforms.
const size_t kTrackedPrefsReportingIDsCount = 14;
COMPILE_ASSERT(kTrackedPrefsReportingIDsCount >= arraysize(kTrackedPrefs),
               need_to_increment_ids_count);

PrefHashFilter::EnforcementLevel GetSettingsEnforcementLevel() {
  static const char kSettingsEnforcementExperiment[] = "SettingsEnforcement";
  struct {
    const char* level_name;
    PrefHashFilter::EnforcementLevel level;
  } static const kEnforcementLevelMap[] = {
    {
      "no_enforcement",
      PrefHashFilter::NO_ENFORCEMENT
    },
    {
      "enforce",
      PrefHashFilter::ENFORCE
    },
    {
      "enforce_no_seeding",
      PrefHashFilter::ENFORCE_NO_SEEDING
    },
    {
      "enforce_no_seeding_no_migration",
      PrefHashFilter::ENFORCE_NO_SEEDING_NO_MIGRATION
    },
  };

  base::FieldTrial* trial =
      base::FieldTrialList::Find(kSettingsEnforcementExperiment);
  if (trial) {
    const std::string& group_name = trial->group_name();
    // ARRAYSIZE_UNSAFE must be used since the array is declared locally; it is
    // only unsafe because it could not trigger a compile error on some
    // non-array pointer types; this is fine since kEnforcementLevelMap is
    // clearly an array.
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kEnforcementLevelMap); ++i) {
      if (kEnforcementLevelMap[i].level_name == group_name)
        return kEnforcementLevelMap[i].level;
    }
  }
  // TODO(gab): Switch default to ENFORCE_ALL when the field trial config is up.
  return PrefHashFilter::NO_ENFORCEMENT;
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

void PrepareBuilder(
    PrefServiceSyncableFactory* factory,
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    policy::PolicyService* policy_service,
    ManagedUserSettingsService* managed_user_settings,
    scoped_ptr<PrefHashStore> pref_hash_store,
    const scoped_refptr<PrefStore>& extension_prefs,
    bool async) {
#if defined(OS_LINUX)
  // We'd like to see what fraction of our users have the preferences
  // stored on a network file system, as we've had no end of troubles
  // with NFS/AFS.
  // TODO(evanm): remove this once we've collected state.
  file_util::FileSystemType fstype;
  if (file_util::GetFileSystemType(pref_filename.DirName(), &fstype)) {
    UMA_HISTOGRAM_ENUMERATION("PrefService.FileSystemType",
                              static_cast<int>(fstype),
                              file_util::FILE_SYSTEM_TYPE_COUNT);
  }
#endif

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
  if (pref_hash_store) {
    pref_filter.reset(new PrefHashFilter(pref_hash_store.Pass(),
                                         kTrackedPrefs,
                                         arraysize(kTrackedPrefs),
                                         kTrackedPrefsReportingIDsCount,
                                         GetSettingsEnforcementLevel()));
  }
  factory->set_user_prefs(
      new JsonPrefStore(
          pref_filename,
          pref_io_task_runner,
          pref_filter.Pass()));
}

}  // namespace

namespace chrome_prefs {

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
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    policy::PolicyService* policy_service,
    ManagedUserSettingsService* managed_user_settings,
    scoped_ptr<PrefHashStore> pref_hash_store,
    const scoped_refptr<PrefStore>& extension_prefs,
    const scoped_refptr<user_prefs::PrefRegistrySyncable>& pref_registry,
    bool async) {
  TRACE_EVENT0("browser", "chrome_prefs::CreateProfilePrefs");
  PrefServiceSyncableFactory factory;
  PrepareBuilder(&factory,
                 pref_filename,
                 pref_io_task_runner,
                 policy_service,
                 managed_user_settings,
                 pref_hash_store.Pass(),
                 extension_prefs,
                 async);
  return factory.CreateSyncable(pref_registry.get());
}

}  // namespace chrome_prefs
