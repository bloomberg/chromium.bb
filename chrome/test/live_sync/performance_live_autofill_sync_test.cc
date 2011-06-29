// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_autofill_sync_test.h"
#include "chrome/test/live_sync/live_sync_timing_helper.h"

static const int kNumProfiles = 150;

// TODO(braffert): Consider the range / resolution of these test points.
static const int kNumBenchmarkPoints = 18;
static const int kBenchmarkPoints[] = {1, 10, 20, 30, 40, 50, 75, 100, 125,
                                       150, 175, 200, 225, 250, 300, 350, 400,
                                       500};

// TODO(braffert): Move this class into its own .h/.cc files.  What should the
// class files be named as opposed to the file containing the tests themselves?
class PerformanceLiveAutofillSyncTest
    : public TwoClientLiveAutofillSyncTest {
 public:
  PerformanceLiveAutofillSyncTest() : guid_number_(0), name_number_(0) {}

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
  DISALLOW_COPY_AND_ASSIGN(PerformanceLiveAutofillSyncTest);
};

void PerformanceLiveAutofillSyncTest::AddProfiles(int profile,
                                                  int num_profiles) {
  const std::vector<AutofillProfile*>& all_profiles = GetAllProfiles(profile);
  std::vector<AutofillProfile> autofill_profiles;
  for (size_t i = 0; i < all_profiles.size(); ++i) {
    autofill_profiles.push_back(*all_profiles[i]);
  }
  for (int i = 0; i < num_profiles; ++i) {
    autofill_profiles.push_back(NextAutofillProfile());
  }
  SetProfiles(profile, &autofill_profiles);
}

void PerformanceLiveAutofillSyncTest::UpdateProfiles(int profile) {
  const std::vector<AutofillProfile*>& all_profiles = GetAllProfiles(profile);
  std::vector<AutofillProfile> autofill_profiles;
  for (size_t i = 0; i < all_profiles.size(); ++i) {
    autofill_profiles.push_back(*all_profiles[i]);
    autofill_profiles.back().SetInfo(AutofillFieldType(NAME_FIRST),
                               UTF8ToUTF16(NextName()));
  }
  SetProfiles(profile, &autofill_profiles);
}

void PerformanceLiveAutofillSyncTest::RemoveProfiles(int profile) {
  std::vector<AutofillProfile> empty;
  SetProfiles(profile, &empty);
}

void PerformanceLiveAutofillSyncTest::Cleanup() {
  for (int i = 0; i < num_clients(); ++i) {
    RemoveProfiles(i);
  }
  ASSERT_TRUE(AwaitQuiescence());
}

const AutofillProfile PerformanceLiveAutofillSyncTest::NextAutofillProfile() {
  AutofillProfile profile;
  autofill_test::SetProfileInfoWithGuid(&profile, NextGUID().c_str(),
                                        NextName().c_str(), "", "", "", "", "",
                                        "", "", "", "", "", "", "");
  return profile;
}

const std::string PerformanceLiveAutofillSyncTest::NextGUID() {
  return IntToGUID(guid_number_++);
}

const std::string PerformanceLiveAutofillSyncTest::IntToGUID(int n) {
  return StringPrintf("00000000-0000-0000-0000-%012X", n);
}

const std::string PerformanceLiveAutofillSyncTest::NextName() {
  return IntToName(name_number_++);
}

const std::string PerformanceLiveAutofillSyncTest::IntToName(int n) {
  return StringPrintf("Name%d" , n);
}

// TCM ID - 7557873.
IN_PROC_BROWSER_TEST_F(PerformanceLiveAutofillSyncTest, Add) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddProfiles(0, kNumProfiles);
  base::TimeDelta dt =
      LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumProfiles, GetProfileCount(0));
  ASSERT_TRUE(AllProfilesMatch());

  // TODO(braffert): Compare timings against some target value.
  VLOG(0) << std::endl << "dt: " << dt.InSecondsF() << " s";
}

// TCM ID - 7549835.
IN_PROC_BROWSER_TEST_F(PerformanceLiveAutofillSyncTest, Update) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddProfiles(0, kNumProfiles);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  UpdateProfiles(0);
  base::TimeDelta dt =
      LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumProfiles, GetProfileCount(0));
  ASSERT_TRUE(AllProfilesMatch());

  // TODO(braffert): Compare timings against some target value.
  VLOG(0) << std::endl << "dt: " << dt.InSecondsF() << " s";
}

// TCM ID - 7553678.
IN_PROC_BROWSER_TEST_F(PerformanceLiveAutofillSyncTest, Delete) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddProfiles(0, kNumProfiles);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  RemoveProfiles(0);
  base::TimeDelta dt =
      LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0, GetProfileCount(0));
  ASSERT_TRUE(AllProfilesMatch());

  // TODO(braffert): Compare timings against some target value.
  VLOG(0) << std::endl << "dt: " << dt.InSecondsF() << " s";
}

IN_PROC_BROWSER_TEST_F(PerformanceLiveAutofillSyncTest, DISABLED_Benchmark) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  for (int i = 0; i < kNumBenchmarkPoints; ++i) {
    int num_profiles = kBenchmarkPoints[i];
    AddProfiles(0, num_profiles);
    base::TimeDelta dt_add =
        LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_profiles, GetProfileCount(0));
    ASSERT_TRUE(AllProfilesMatch());
    VLOG(0) << std::endl << "Add: " << num_profiles << " "
            << dt_add.InSecondsF();

    UpdateProfiles(0);
    base::TimeDelta dt_update =
        LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_profiles, GetProfileCount(0));
    ASSERT_TRUE(AllProfilesMatch());
    VLOG(0) << std::endl << "Update: " << num_profiles << " "
            << dt_update.InSecondsF();

    RemoveProfiles(0);
    base::TimeDelta dt_delete =
        LiveSyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(0, GetProfileCount(0));
    ASSERT_TRUE(AllProfilesMatch());
    VLOG(0) << std::endl << "Delete: " << num_profiles << " "
            << dt_delete.InSecondsF();

    Cleanup();
  }
}
