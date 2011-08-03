// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/autofill_helper.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "chrome/test/live_sync/performance/sync_timing_helper.h"

// TODO(braffert): Move kNumBenchmarkPoints and kBenchmarkPoints for all
// datatypes into a performance test base class, once it is possible to do so.
static const int kNumProfiles = 150;
static const int kNumBenchmarkPoints = 18;
static const int kBenchmarkPoints[] = {1, 10, 20, 30, 40, 50, 75, 100, 125,
                                       150, 175, 200, 225, 250, 300, 350, 400,
                                       500};

class AutofillSyncPerfTest : public LiveSyncTest {
 public:
  AutofillSyncPerfTest()
      : LiveSyncTest(TWO_CLIENT),
        guid_number_(0),
        name_number_(0) {}

  // Adds |num_profiles| new autofill profiles to the sync profile |profile|.
  void AddProfiles(int profile, int num_profiles);

  // Updates all autofill profiles for the sync profile |profile|.
  void UpdateProfiles(int profile);

  // Removes all bookmarks in the bookmark bar for |profile|.
  void RemoveProfiles(int profile);

  // Removes all autofill profiles in all sync profiles.  Called between
  // benchmark iterations.
  void Cleanup();

 private:
  // Returns a new unique autofill profile.
  const AutofillProfile NextAutofillProfile();

  // Returns an unused unique guid.
  const std::string NextGUID();

  // Returns a unique guid based on the input integer |n|.
  const std::string IntToGUID(int n);

  // Returns a new unused unique name.
  const std::string NextName();

  // Returns a unique name based on the input integer |n|.
  const std::string IntToName(int n);

  int guid_number_;
  int name_number_;
  DISALLOW_COPY_AND_ASSIGN(AutofillSyncPerfTest);
};

void AutofillSyncPerfTest::AddProfiles(int profile, int num_profiles) {
  const std::vector<AutofillProfile*>& all_profiles =
      AutofillHelper::GetAllProfiles(profile);
  std::vector<AutofillProfile> autofill_profiles;
  for (size_t i = 0; i < all_profiles.size(); ++i) {
    autofill_profiles.push_back(*all_profiles[i]);
  }
  for (int i = 0; i < num_profiles; ++i) {
    autofill_profiles.push_back(NextAutofillProfile());
  }
  AutofillHelper::SetProfiles(profile, &autofill_profiles);
}

void AutofillSyncPerfTest::UpdateProfiles(int profile) {
  const std::vector<AutofillProfile*>& all_profiles =
      AutofillHelper::GetAllProfiles(profile);
  std::vector<AutofillProfile> autofill_profiles;
  for (size_t i = 0; i < all_profiles.size(); ++i) {
    autofill_profiles.push_back(*all_profiles[i]);
    autofill_profiles.back().SetInfo(AutofillFieldType(NAME_FIRST),
                               UTF8ToUTF16(NextName()));
  }
  AutofillHelper::SetProfiles(profile, &autofill_profiles);
}

void AutofillSyncPerfTest::RemoveProfiles(int profile) {
  std::vector<AutofillProfile> empty;
  AutofillHelper::SetProfiles(profile, &empty);
}

void AutofillSyncPerfTest::Cleanup() {
  for (int i = 0; i < num_clients(); ++i) {
    RemoveProfiles(i);
  }
  ASSERT_TRUE(AwaitQuiescence());
}

const AutofillProfile AutofillSyncPerfTest::NextAutofillProfile() {
  AutofillProfile profile;
  autofill_test::SetProfileInfoWithGuid(&profile, NextGUID().c_str(),
                                        NextName().c_str(), "", "", "", "", "",
                                        "", "", "", "", "", "", "");
  return profile;
}

const std::string AutofillSyncPerfTest::NextGUID() {
  return IntToGUID(guid_number_++);
}

const std::string AutofillSyncPerfTest::IntToGUID(int n) {
  return StringPrintf("00000000-0000-0000-0000-%012X", n);
}

const std::string AutofillSyncPerfTest::NextName() {
  return IntToName(name_number_++);
}

const std::string AutofillSyncPerfTest::IntToName(int n) {
  return StringPrintf("Name%d" , n);
}

IN_PROC_BROWSER_TEST_F(AutofillSyncPerfTest, P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // TCM ID - 7557873.
  AddProfiles(0, kNumProfiles);
  base::TimeDelta dt =
      SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumProfiles, GetProfileCount(1));
  SyncTimingHelper::PrintResult("autofill", "add", dt);

  // TCM ID - 7549835.
  UpdateProfiles(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumProfiles, GetProfileCount(1));
  SyncTimingHelper::PrintResult("autofill", "update", dt);

  // TCM ID - 7553678.
  RemoveProfiles(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0, GetProfileCount(1));
  SyncTimingHelper::PrintResult("autofill", "delete", dt);
}

IN_PROC_BROWSER_TEST_F(AutofillSyncPerfTest, DISABLED_Benchmark) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  for (int i = 0; i < kNumBenchmarkPoints; ++i) {
    int num_profiles = kBenchmarkPoints[i];
    AddProfiles(0, num_profiles);
    base::TimeDelta dt_add =
        SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_profiles, AutofillHelper::GetProfileCount(0));
    ASSERT_TRUE(AutofillHelper::AllProfilesMatch());
    VLOG(0) << std::endl << "Add: " << num_profiles << " "
            << dt_add.InSecondsF();

    UpdateProfiles(0);
    base::TimeDelta dt_update =
        SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_profiles, AutofillHelper::GetProfileCount(0));
    ASSERT_TRUE(AutofillHelper::AllProfilesMatch());
    VLOG(0) << std::endl << "Update: " << num_profiles << " "
            << dt_update.InSecondsF();

    RemoveProfiles(0);
    base::TimeDelta dt_delete =
        SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(0, AutofillHelper::GetProfileCount(0));
    ASSERT_TRUE(AutofillHelper::AllProfilesMatch());
    VLOG(0) << std::endl << "Delete: " << num_profiles << " "
            << dt_delete.InSecondsF();

    Cleanup();
  }
}
