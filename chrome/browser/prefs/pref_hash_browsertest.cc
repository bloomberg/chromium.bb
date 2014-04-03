// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/pref_hash_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// An observer that returns back to test code after a new profile is
// initialized.
void OnUnblockOnProfileCreation(const base::Closure& callback,
                                Profile* profile,
                                Profile::CreateStatus status) {
  switch (status) {
    case Profile::CREATE_STATUS_CREATED:
      // Wait for CREATE_STATUS_INITIALIZED.
      break;
    case Profile::CREATE_STATUS_INITIALIZED:
      callback.Run();
      break;
    default:
      ADD_FAILURE() << "Unexpected Profile::CreateStatus: " << status;
      callback.Run();
      break;
  }
}

// Finds a profile path corresponding to a profile that has not been loaded yet.
base::FilePath GetUnloadedProfilePath() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  const std::vector<Profile*> loaded_profiles =
      profile_manager->GetLoadedProfiles();
  std::set<base::FilePath> profile_paths;
  for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i)
    profile_paths.insert(cache.GetPathOfProfileAtIndex(i));
  for (size_t i = 0; i < loaded_profiles.size(); ++i)
    EXPECT_EQ(1U, profile_paths.erase(loaded_profiles[i]->GetPath()));
  if (profile_paths.size())
    return *profile_paths.begin();
  return base::FilePath();
}

// Returns the number of times |histogram_name| was reported so far; adding the
// results of the first 100 buckets (there are only ~14 reporting IDs as of this
// writting; varies depending on the platform). If |expect_zero| is true, this
// method will explicitly report IDs that are non-zero for ease of diagnosis.
int GetTrackedPrefHistogramCount(const char* histogram_name, bool expect_zero) {
  const base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(histogram_name);
  if (!histogram)
    return 0;

  scoped_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
  int sum = 0;
  for (int i = 0; i < 100; ++i) {
    int count_for_id = samples->GetCount(i);
    sum += count_for_id;

    if (expect_zero)
      EXPECT_EQ(0, count_for_id) << "Faulty reporting_id: " << i;
  }
  return sum;
}

}  // namespace

class PrefHashBrowserTest : public InProcessBrowserTest,
                            public testing::WithParamInterface<std::string> {
 public:
  PrefHashBrowserTest()
      : is_unloaded_profile_seeding_allowed_(
            IsUnloadedProfileSeedingAllowed()) {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kForceFieldTrials,
        std::string(chrome_prefs::internals::kSettingsEnforcementTrialName) +
            "/" + GetParam() + "/");
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    // Force the delayed PrefHashStore update task to happen immediately with
    // no domain check (bots are on a domain).
    chrome_prefs::DisableDelaysAndDomainCheckForTesting();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    // content::RunAllPendingInMessageLoop() is already called before
    // SetUpOnMainThread() in in_process_browser_test.cc which guarantees that
    // UpdateAllPrefHashStoresIfRequired() has already been called.

    // Now flush the blocking pool to force any pending JsonPrefStore async read
    // requests.
    content::BrowserThread::GetBlockingPool()->FlushForTesting();

    // And finally run tasks on this message loop again to process the OnRead()
    // callbacks resulting from the file reads above.
    content::RunAllPendingInMessageLoop();
  }

 protected:
  const bool is_unloaded_profile_seeding_allowed_;

 private:
  bool IsUnloadedProfileSeedingAllowed() const {
    const bool allowed_from_param =
        GetParam() !=
            chrome_prefs::internals::kSettingsEnforcementGroupEnforceAlways &&
        GetParam() != chrome_prefs::internals::
                          kSettingsEnforcementGroupEnforceAlwaysWithExtensions;
    const bool blocked_by_configuration =
#if defined(OFFICIAL_BUILD)
        // SettingsEnforcement can't be forced via --force-fieldtrials in
        // official builds. And since the default is the strongest enforcement
        // level, nothing is allowed.
        true;
#else
        false;
#endif
    return allowed_from_param && !blocked_by_configuration;
  }
};

#if defined(OS_CHROMEOS)
// PrefHash service has been disabled on ChromeOS: crbug.com/343261
#define MAYBE_PRE_PRE_InitializeUnloadedProfiles DISABLED_PRE_PRE_InitializeUnloadedProfiles
#define MAYBE_PRE_InitializeUnloadedProfiles DISABLED_PRE_InitializeUnloadedProfiles
#define MAYBE_InitializeUnloadedProfiles DISABLED_InitializeUnloadedProfiles
#else
#define MAYBE_PRE_PRE_InitializeUnloadedProfiles PRE_PRE_InitializeUnloadedProfiles
#define MAYBE_PRE_InitializeUnloadedProfiles PRE_InitializeUnloadedProfiles
#define MAYBE_InitializeUnloadedProfiles InitializeUnloadedProfiles
#endif

IN_PROC_BROWSER_TEST_P(PrefHashBrowserTest,
                       MAYBE_PRE_PRE_InitializeUnloadedProfiles) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create an additional profile.
  const base::FilePath new_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  const scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  profile_manager->CreateProfileAsync(
      new_path,
      base::Bind(&OnUnblockOnProfileCreation, runner->QuitClosure()),
      base::string16(),
      base::string16(),
      std::string());

  // Spin to allow profile creation to take place, loop is terminated
  // by OnUnblockOnProfileCreation when the profile is created.
  runner->Run();

  // No profile should have gone through the unloaded profile initialization in
  // this phase as both profiles should have been loaded normally.
  EXPECT_EQ(
      0, GetTrackedPrefHistogramCount(
             "Settings.TrackedPreferencesAlternateStoreVersionUpdatedFrom",
             true));
}

IN_PROC_BROWSER_TEST_P(PrefHashBrowserTest,
                       MAYBE_PRE_InitializeUnloadedProfiles) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  // Creating the profile would have initialized its hash store. Also, we don't
  // know whether the newly created or original profile will be launched (does
  // creating a profile cause it to be the most recently used?).

  // So we will find the profile that isn't loaded, reset its hash store, and
  // then verify in the _next_ launch that it is, indeed, restored despite not
  // having been loaded.

  const base::DictionaryValue* hashes =
      g_browser_process->local_state()->GetDictionary(
          prefs::kProfilePreferenceHashes);

  // 4 is for hash_of_hashes, versions_dict, default profile, and new profile.
  EXPECT_EQ(4U, hashes->size());

  // One of the two profiles should not have been loaded. Reset its hash store.
  const base::FilePath unloaded_profile_path = GetUnloadedProfilePath();
  chrome_prefs::ResetPrefHashStore(unloaded_profile_path);

  // One of the profile hash collections should be gone.
  EXPECT_EQ(3U, hashes->size());

  // No profile should have gone through the unloaded profile initialization in
  // this phase as both profiles were already initialized at the beginning of
  // this phase (resetting the unloaded profile's PrefHashStore should only
  // force initialization in the next phase's startup).
  EXPECT_EQ(
      0, GetTrackedPrefHistogramCount(
             "Settings.TrackedPreferencesAlternateStoreVersionUpdatedFrom",
             true));
}

IN_PROC_BROWSER_TEST_P(PrefHashBrowserTest,
                       MAYBE_InitializeUnloadedProfiles) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  const base::DictionaryValue* hashes =
      g_browser_process->local_state()->GetDictionary(
          prefs::kProfilePreferenceHashes);

  // The deleted hash collection should be restored only if the current
  // SettingsEnforcement group allows it.
  if (is_unloaded_profile_seeding_allowed_) {
    EXPECT_EQ(4U, hashes->size());

    // Verify that the initialization truly did occur in this phase's startup;
    // rather than in the previous phase's shutdown.
    EXPECT_EQ(
        1, GetTrackedPrefHistogramCount(
               "Settings.TrackedPreferencesAlternateStoreVersionUpdatedFrom",
               false));
  } else {
    EXPECT_EQ(3U, hashes->size());

    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               "Settings.TrackedPreferencesAlternateStoreVersionUpdatedFrom",
               true));
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Verify that only one profile was loaded. We assume that the unloaded
  // profile is the same one that wasn't loaded in the last launch (i.e., it's
  // the one whose hash store we reset, and the fact that it is now restored is
  // evidence that we restored the hashes of an unloaded profile.).
  ASSERT_EQ(1U, profile_manager->GetLoadedProfiles().size());

  // Loading the first profile should only have produced unchanged reports.
  EXPECT_EQ(
      0, GetTrackedPrefHistogramCount(
             "Settings.TrackedPreferenceChanged", true));
  EXPECT_EQ(
      0, GetTrackedPrefHistogramCount(
             "Settings.TrackedPreferenceCleared", true));
  EXPECT_EQ(
      0, GetTrackedPrefHistogramCount(
             "Settings.TrackedPreferenceInitialized", true));
  EXPECT_EQ(
      0, GetTrackedPrefHistogramCount(
             "Settings.TrackedPreferenceTrustedInitialized", true));
  EXPECT_EQ(
      0, GetTrackedPrefHistogramCount(
             "Settings.TrackedPreferenceMigrated", true));
  int initial_unchanged_count =
      GetTrackedPrefHistogramCount("Settings.TrackedPreferenceUnchanged",
                                   false);
  EXPECT_GT(initial_unchanged_count, 0);

  if (is_unloaded_profile_seeding_allowed_) {
    // Explicitly load the unloaded profile.
    profile_manager->GetProfile(GetUnloadedProfilePath());
    ASSERT_EQ(2U, profile_manager->GetLoadedProfiles().size());

    // Loading the unloaded profile should only generate unchanged pings; and
    // should have produced as many of them as loading the first profile.
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               "Settings.TrackedPreferenceChanged", true));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               "Settings.TrackedPreferenceCleared", true));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               "Settings.TrackedPreferenceInitialized", true));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               "Settings.TrackedPreferenceTrustedInitialized", true));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               "Settings.TrackedPreferenceMigrated", true));
    EXPECT_EQ(
        initial_unchanged_count * 2,
        GetTrackedPrefHistogramCount("Settings.TrackedPreferenceUnchanged",
                                     false));
  }
}

INSTANTIATE_TEST_CASE_P(
    PrefHashBrowserTestInstance,
    PrefHashBrowserTest,
    testing::Values(
        chrome_prefs::internals::kSettingsEnforcementGroupNoEnforcement,
        chrome_prefs::internals::kSettingsEnforcementGroupEnforceOnload,
        chrome_prefs::internals::kSettingsEnforcementGroupEnforceAlways,
        chrome_prefs::internals::
            kSettingsEnforcementGroupEnforceAlwaysWithExtensions));
