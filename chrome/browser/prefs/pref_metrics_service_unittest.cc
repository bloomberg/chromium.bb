// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_metrics_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// TestingProfile may register some real preferences; to avoid interference,
// define fake preferences for testing.
const char* kTrackedPrefs[] = {
  "pref_metrics_service_test.pref1",
  "pref_metrics_service_test.pref2",
};

const int kTrackedPrefCount = arraysize(kTrackedPrefs);

const char kTestDeviceId[] = "test_device_id1";
const char kOtherTestDeviceId[] = "test_device_id2";

}  // namespace

class PrefMetricsServiceTest : public testing::Test {
 protected:
  virtual void SetUp() {
    pref1_changed_ = 0;
    pref2_changed_ = 0;
    pref1_cleared_ = 0;
    pref2_cleared_ = 0;
    pref1_initialized_ = 0;
    pref2_initialized_ = 0;
    pref1_migrated_ = 0;
    pref2_migrated_ = 0;
    pref1_unchanged_ = 0;
    pref2_unchanged_ = 0;

    base::StatisticsRecorder::Initialize();

    // Reset and set up the profile manager.
    profile_manager_.reset(new TestingProfileManager(
        TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    // Check that PrefMetricsService behaves with a '.' in the profile name.
    profile_ = profile_manager_->CreateTestingProfile("test@example.com");

    profile_name_ = profile_->GetPath().AsUTF8Unsafe();

    prefs_ = profile_->GetTestingPrefService();

    // Register our test-only tracked prefs as string values.
    for (int i = 0; i < kTrackedPrefCount; ++i) {
      prefs_->registry()->RegisterStringPref(
          kTrackedPrefs[i],
          "test_default_value",
          user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    }

    // Initialize pref in local state that holds hashed values.
    PrefMetricsService::RegisterPrefs(local_state_.registry());

    // Update global counts in case another test left stray samples.
    UpdateHistogramSamples();
  }

  scoped_ptr<PrefMetricsService> CreatePrefMetricsService(
      const std::string& device_id) {
    return scoped_ptr<PrefMetricsService>(
        new PrefMetricsService(profile_,
                               &local_state_,
                               device_id,
                               kTrackedPrefs,
                               kTrackedPrefCount));
  }

  std::string GetHashedPrefValue(PrefMetricsService* service,
                                 const char* path,
                                 const base::Value* value) {
    return service->GetHashedPrefValue(
        path, value, PrefMetricsService::HASHED_PREF_STYLE_NEW);
  }

  std::string GetOldStyleHashedPrefValue(PrefMetricsService* service,
                                         const char* path,
                                         const base::Value* value) {
    return service->GetHashedPrefValue(
        path, value, PrefMetricsService::HASHED_PREF_STYLE_DEPRECATED);
  }

  void GetSamples(const char* histogram_name, int* bucket1, int* bucket2) {
    base::HistogramBase* histogram =
        base::StatisticsRecorder::FindHistogram(histogram_name);
    if (!histogram) {
      *bucket1 = 0;
      *bucket2 = 0;
    } else {
      scoped_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
      *bucket1 = samples->GetCount(0);
      *bucket2 = samples->GetCount(1);
    }
  }

  void UpdateHistogramSamples() {
    int changed1, changed2;
    GetSamples("Settings.TrackedPreferenceChanged", &changed1, &changed2);
    pref1_changed_ = changed1 - pref1_changed_total;
    pref2_changed_ = changed2 - pref2_changed_total;
    pref1_changed_total = changed1;
    pref2_changed_total = changed2;

    int cleared1, cleared2;
    GetSamples("Settings.TrackedPreferenceCleared", &cleared1, &cleared2);
    pref1_cleared_ = cleared1 - pref1_cleared_total;
    pref2_cleared_ = cleared2 - pref2_cleared_total;
    pref1_cleared_total = cleared1;
    pref2_cleared_total = cleared2;

    int inited1, inited2;
    GetSamples("Settings.TrackedPreferenceInitialized", &inited1, &inited2);
    pref1_initialized_ = inited1 - pref1_initialized_total;
    pref2_initialized_ = inited2 - pref2_initialized_total;
    pref1_initialized_total = inited1;
    pref2_initialized_total = inited2;

    int migrated1, migrated2;
    GetSamples("Settings.TrackedPreferenceMigrated", &migrated1, &migrated2);
    pref1_migrated_ = migrated1 - pref1_migrated_total;
    pref2_migrated_ = migrated2 - pref2_migrated_total;
    pref1_migrated_total = migrated1;
    pref2_migrated_total = migrated2;

    int unchanged1, unchanged2;
    GetSamples("Settings.TrackedPreferenceUnchanged", &unchanged1, &unchanged2);
    pref1_unchanged_ = unchanged1 - pref1_unchanged_total;
    pref2_unchanged_ = unchanged2 - pref2_unchanged_total;
    pref1_unchanged_total = unchanged1;
    pref2_unchanged_total = unchanged2;
  }

  TestingProfile* profile_;
  std::string profile_name_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  TestingPrefServiceSyncable* prefs_;
  TestingPrefServiceSimple local_state_;

  // Since histogram samples are recorded by a global StatisticsRecorder, we
  // need to maintain total counts so we can compute deltas for individual
  // tests.
  static int pref1_changed_total;
  static int pref2_changed_total;
  static int pref1_cleared_total;
  static int pref2_cleared_total;
  static int pref1_initialized_total;
  static int pref2_initialized_total;
  static int pref1_migrated_total;
  static int pref2_migrated_total;
  static int pref1_unchanged_total;
  static int pref2_unchanged_total;

  // Counts of samples recorded since UpdateHistogramSamples was last called.
  int pref1_changed_;
  int pref2_changed_;
  int pref1_cleared_;
  int pref2_cleared_;
  int pref1_initialized_;
  int pref2_initialized_;
  int pref1_migrated_;
  int pref2_migrated_;
  int pref1_unchanged_;
  int pref2_unchanged_;
};

int PrefMetricsServiceTest::pref1_changed_total;
int PrefMetricsServiceTest::pref2_changed_total;
int PrefMetricsServiceTest::pref1_cleared_total;
int PrefMetricsServiceTest::pref2_cleared_total;
int PrefMetricsServiceTest::pref1_initialized_total;
int PrefMetricsServiceTest::pref2_initialized_total;
int PrefMetricsServiceTest::pref1_migrated_total;
int PrefMetricsServiceTest::pref2_migrated_total;
int PrefMetricsServiceTest::pref1_unchanged_total;
int PrefMetricsServiceTest::pref2_unchanged_total;

TEST_F(PrefMetricsServiceTest, StartupNoUserPref) {
  // Local state is empty and no user prefs are set. We should record that we
  // checked preferences once but since there are no user preference values, no
  // other histogram data should be collected.
  scoped_ptr<PrefMetricsService> service =
      CreatePrefMetricsService(kTestDeviceId);
  UpdateHistogramSamples();
  EXPECT_EQ(0, pref1_changed_);
  EXPECT_EQ(0, pref2_changed_);
  EXPECT_EQ(0, pref1_cleared_);
  EXPECT_EQ(0, pref2_cleared_);
  EXPECT_EQ(0, pref1_initialized_);
  EXPECT_EQ(0, pref2_initialized_);
  EXPECT_EQ(0, pref1_migrated_);
  EXPECT_EQ(0, pref2_migrated_);
  EXPECT_EQ(1, pref1_unchanged_);
  EXPECT_EQ(1, pref2_unchanged_);
}

TEST_F(PrefMetricsServiceTest, StartupUserPref) {
  // Local state is empty. Set a value for one tracked pref. We should record
  // that we checked preferences once and initialized a hash for the pref.
  prefs_->SetString(kTrackedPrefs[0], "foo");
  {
    scoped_ptr<PrefMetricsService> service =
        CreatePrefMetricsService(kTestDeviceId);
    UpdateHistogramSamples();
    EXPECT_EQ(0, pref1_changed_);
    EXPECT_EQ(0, pref2_changed_);
    EXPECT_EQ(0, pref1_cleared_);
    EXPECT_EQ(0, pref2_cleared_);
    EXPECT_EQ(1, pref1_initialized_);
    EXPECT_EQ(0, pref2_initialized_);
    EXPECT_EQ(0, pref1_migrated_);
    EXPECT_EQ(0, pref2_migrated_);
    EXPECT_EQ(0, pref1_unchanged_);
    EXPECT_EQ(1, pref2_unchanged_);

    // Change the pref. This should be observed by the PrefMetricsService, which
    // will update the hash in local_state_ to stay in sync.
    prefs_->SetString(kTrackedPrefs[0], "bar");
  }
  // The next startup should record no changes.
  {
    scoped_ptr<PrefMetricsService> service =
        CreatePrefMetricsService(kTestDeviceId);
    UpdateHistogramSamples();
    EXPECT_EQ(0, pref1_changed_);
    EXPECT_EQ(0, pref2_changed_);
    EXPECT_EQ(0, pref1_cleared_);
    EXPECT_EQ(0, pref2_cleared_);
    EXPECT_EQ(0, pref1_initialized_);
    EXPECT_EQ(0, pref2_initialized_);
    EXPECT_EQ(0, pref1_migrated_);
    EXPECT_EQ(0, pref2_migrated_);
    EXPECT_EQ(1, pref1_unchanged_);
    EXPECT_EQ(1, pref2_unchanged_);
  }
}

TEST_F(PrefMetricsServiceTest, ChangedUserPref) {
  // Local state is empty. Set a value for the tracked pref. We should record
  // that we checked preferences once and initialized a hash for the pref.
  prefs_->SetString(kTrackedPrefs[0], "foo");
  {
    scoped_ptr<PrefMetricsService> service =
        CreatePrefMetricsService(kTestDeviceId);
    UpdateHistogramSamples();
    EXPECT_EQ(0, pref1_changed_);
    EXPECT_EQ(0, pref2_changed_);
    EXPECT_EQ(0, pref1_cleared_);
    EXPECT_EQ(0, pref2_cleared_);
    EXPECT_EQ(1, pref1_initialized_);
    EXPECT_EQ(0, pref2_initialized_);
    EXPECT_EQ(0, pref1_migrated_);
    EXPECT_EQ(0, pref2_migrated_);
    EXPECT_EQ(0, pref1_unchanged_);
    EXPECT_EQ(1, pref2_unchanged_);
    // Hashed prefs should now be stored in local state.
  }
  // Change the value of the tracked pref while there is no PrefMetricsService
  // to update the hash. We should observe a pref value change.
  prefs_->SetString(kTrackedPrefs[0], "bar");
  {
    scoped_ptr<PrefMetricsService> service =
        CreatePrefMetricsService(kTestDeviceId);
    UpdateHistogramSamples();
    EXPECT_EQ(1, pref1_changed_);
    EXPECT_EQ(0, pref2_changed_);
    EXPECT_EQ(0, pref1_cleared_);
    EXPECT_EQ(0, pref2_cleared_);
    EXPECT_EQ(0, pref1_initialized_);
    EXPECT_EQ(0, pref2_initialized_);
    EXPECT_EQ(0, pref1_migrated_);
    EXPECT_EQ(0, pref2_migrated_);
    EXPECT_EQ(0, pref1_unchanged_);
    EXPECT_EQ(1, pref2_unchanged_);
  }
  // Clear the value of the tracked pref while there is no PrefMetricsService
  // to update the hash. We should observe a pref value removal.
  prefs_->ClearPref(kTrackedPrefs[0]);
  {
    scoped_ptr<PrefMetricsService> service =
        CreatePrefMetricsService(kTestDeviceId);
    UpdateHistogramSamples();
    EXPECT_EQ(0, pref1_changed_);
    EXPECT_EQ(0, pref2_changed_);
    EXPECT_EQ(1, pref1_cleared_);
    EXPECT_EQ(0, pref2_cleared_);
    EXPECT_EQ(0, pref1_initialized_);
    EXPECT_EQ(0, pref2_initialized_);
    EXPECT_EQ(0, pref1_migrated_);
    EXPECT_EQ(0, pref2_migrated_);
    EXPECT_EQ(0, pref1_unchanged_);
    EXPECT_EQ(1, pref2_unchanged_);
  }
}

TEST_F(PrefMetricsServiceTest, MigratedUserPref) {
  // Initialize both preferences and get the old style hash for the first pref
  // from the PrefMetricsService before shutting it down.
  prefs_->SetString(kTrackedPrefs[0], "foo");
  prefs_->SetString(kTrackedPrefs[1], "bar");
  std::string old_style_hash;
  {
    scoped_ptr<PrefMetricsService> service =
        CreatePrefMetricsService(kTestDeviceId);
    UpdateHistogramSamples();
    EXPECT_EQ(0, pref1_changed_);
    EXPECT_EQ(0, pref2_changed_);
    EXPECT_EQ(0, pref1_cleared_);
    EXPECT_EQ(0, pref2_cleared_);
    EXPECT_EQ(1, pref1_initialized_);
    EXPECT_EQ(1, pref2_initialized_);
    EXPECT_EQ(0, pref1_migrated_);
    EXPECT_EQ(0, pref2_migrated_);
    EXPECT_EQ(0, pref1_unchanged_);
    EXPECT_EQ(0, pref2_unchanged_);

    old_style_hash =
        GetOldStyleHashedPrefValue(service.get(), kTrackedPrefs[0],
                                   prefs_->GetUserPrefValue(kTrackedPrefs[0]));
  }

  // Update the pref's hash to use the old style while the PrefMetricsService
  // isn't running.
  {
    DictionaryPrefUpdate update(&local_state_, prefs::kProfilePreferenceHashes);
    DictionaryValue* child_dictionary = NULL;
    // Get the dictionary corresponding to the profile name,
    // which may have a '.'
    ASSERT_TRUE(update->GetDictionaryWithoutPathExpansion(profile_name_,
                                                          &child_dictionary));
    child_dictionary->SetString(kTrackedPrefs[0], old_style_hash);
  }

  // Relaunch the service and make sure the first preference got migrated.
  {
    scoped_ptr<PrefMetricsService> service =
        CreatePrefMetricsService(kTestDeviceId);
    UpdateHistogramSamples();
    EXPECT_EQ(0, pref1_changed_);
    EXPECT_EQ(0, pref2_changed_);
    EXPECT_EQ(0, pref1_cleared_);
    EXPECT_EQ(0, pref2_cleared_);
    EXPECT_EQ(0, pref1_initialized_);
    EXPECT_EQ(0, pref2_initialized_);
    EXPECT_EQ(1, pref1_migrated_);
    EXPECT_EQ(0, pref2_migrated_);
    EXPECT_EQ(0, pref1_unchanged_);
    EXPECT_EQ(1, pref2_unchanged_);
  }
  // Make sure the migration happens only once.
  {
    scoped_ptr<PrefMetricsService> service =
        CreatePrefMetricsService(kTestDeviceId);
    UpdateHistogramSamples();
    EXPECT_EQ(0, pref1_changed_);
    EXPECT_EQ(0, pref2_changed_);
    EXPECT_EQ(0, pref1_cleared_);
    EXPECT_EQ(0, pref2_cleared_);
    EXPECT_EQ(0, pref1_initialized_);
    EXPECT_EQ(0, pref2_initialized_);
    EXPECT_EQ(0, pref1_migrated_);
    EXPECT_EQ(0, pref2_migrated_);
    EXPECT_EQ(1, pref1_unchanged_);
    EXPECT_EQ(1, pref2_unchanged_);
  }
}

// Make sure that the new algorithm is still able to generate old style hashes
// as they were before this change.
TEST_F(PrefMetricsServiceTest, OldStyleHashAsExpected) {
  scoped_ptr<PrefMetricsService> service =
      CreatePrefMetricsService(kTestDeviceId);

  // Verify the hashes match the values previously used in the
  // "PrefHashStability" test below.
  DictionaryValue dict;
  dict.Set("a", new StringValue("foo"));
  dict.Set("d", new StringValue("bad"));
  dict.Set("b", new StringValue("bar"));
  dict.Set("c", new StringValue("baz"));
  EXPECT_EQ("C503FB7C65EEFD5C07185F616A0AA67923C069909933F362022B1F187E73E9A2",
            GetOldStyleHashedPrefValue(service.get(), "pref.path1", &dict));
  ListValue list;
  list.Set(0, new base::FundamentalValue(true));
  list.Set(1, new base::FundamentalValue(100));
  list.Set(2, new base::FundamentalValue(1.0));
  EXPECT_EQ("3163EC3C96263143AF83EA5C9860DFB960EE2263413C7D7D8A9973FCC00E7692",
            GetOldStyleHashedPrefValue(service.get(), "pref.path2", &list));
}

// Tests that serialization of dictionary values is stable. If the order of
// the entries or any whitespace changes, it would cause a spike in pref change
// UMA events as every hash would change.
TEST_F(PrefMetricsServiceTest, PrefHashStability) {
  scoped_ptr<PrefMetricsService> service =
      CreatePrefMetricsService(kTestDeviceId);

  DictionaryValue dict;
  dict.Set("a", new StringValue("foo"));
  dict.Set("d", new StringValue("bad"));
  dict.Set("b", new StringValue("bar"));
  dict.Set("c", new StringValue("baz"));
  EXPECT_EQ("A50FE7EB31BFBC32B8A27E71730AF15421178A9B5815644ACE174B18966735B9",
            GetHashedPrefValue(service.get(), "pref.path1", &dict));

  ListValue list;
  list.Set(0, new base::FundamentalValue(true));
  list.Set(1, new base::FundamentalValue(100));
  list.Set(2, new base::FundamentalValue(1.0));
  EXPECT_EQ("5CE37D7EBCBC9BE510F0F5E7C326CA92C1673713C3717839610AEA1A217D8BB8",
            GetHashedPrefValue(service.get(), "pref.path2", &list));
}

// Tests that different hashes are generated for different device IDs.
TEST_F(PrefMetricsServiceTest, HashIsBasedOnDeviceId) {
  scoped_ptr<PrefMetricsService> service =
      CreatePrefMetricsService(kTestDeviceId);
  scoped_ptr<PrefMetricsService> other_service =
      CreatePrefMetricsService(kOtherTestDeviceId);

  StringValue test_value("test value");
  EXPECT_EQ("49CA276F9F2AEDCF6BFA1CD9FC4747476E1315BBBBC27DD33548B23CD36E2EEE",
            GetHashedPrefValue(service.get(), "pref.path", &test_value));
  EXPECT_EQ("13EEDA99C38777ADA8B87C23A3C5CD1FD31ADE1491823E255D3520E5B56C4BC7",
            GetHashedPrefValue(other_service.get(), "pref.path", &test_value));
}

// Tests that different hashes are generated for different paths.
TEST_F(PrefMetricsServiceTest, HashIsBasedOnPath) {
  scoped_ptr<PrefMetricsService> service =
      CreatePrefMetricsService(kTestDeviceId);

  StringValue test_value("test value");
  EXPECT_EQ("2A5DCB1294F212DB26DF9C08C46F11C272D80136AAD3B4AAE5B7D008DF5F3F22",
            GetHashedPrefValue(service.get(), "pref.path1", &test_value));
  EXPECT_EQ("455EC2A7E192E9F1C06294BBB3B66BBD81B8D1A8550D518EA5D5C8F70FCF6EF3",
            GetHashedPrefValue(service.get(), "pref.path2", &test_value));
}
