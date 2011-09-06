// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/webdata/autofill_entry.h"

using autofill_helper::AllProfilesMatch;
using autofill_helper::GetAllKeys;
using autofill_helper::GetAllProfiles;
using autofill_helper::GetKeyCount;
using autofill_helper::GetProfileCount;
using autofill_helper::RemoveKey;
using autofill_helper::SetProfiles;

static const int kNumKeys = 150;
static const int kNumProfiles = 150;

class AutofillSyncPerfTest : public SyncTest {
 public:
  AutofillSyncPerfTest()
      : SyncTest(TWO_CLIENT),
        guid_number_(0),
        name_number_(0),
        value_number_(0) {}

  // Adds |num_profiles| new autofill profiles to the sync profile |profile|.
  void AddProfiles(int profile, int num_profiles);

  // Updates all autofill profiles for the sync profile |profile|.
  void UpdateProfiles(int profile);

  // Removes all autofill profiles from |profile|.
  void RemoveProfiles(int profile);

  // Adds |num_keys| new autofill keys to the sync profile |profile|.
  void AddKeys(int profile, int num_keys);

  // Removes all autofill keys from |profile|.
  void RemoveKeys(int profile);

 private:
  // Returns a new unique autofill profile.
  const AutofillProfile NextAutofillProfile();

  // Returns a new unique autofill key.
  const AutofillKey NextAutofillKey();

  // Returns an unused unique guid.
  const std::string NextGUID();

  // Returns a unique guid based on the input integer |n|.
  const std::string IntToGUID(int n);

  // Returns a new unused unique name.
  const std::string NextName();

  // Returns a unique name based on the input integer |n|.
  const std::string IntToName(int n);

  // Returns a new unused unique value for autofill entries.
  const std::string NextValue();

  // Returnes a unique value based on the input integer |n|.
  const std::string IntToValue(int n);

  int guid_number_;
  int name_number_;
  int value_number_;
  DISALLOW_COPY_AND_ASSIGN(AutofillSyncPerfTest);
};

void AutofillSyncPerfTest::AddProfiles(int profile, int num_profiles) {
  const std::vector<AutofillProfile*>& all_profiles =
      GetAllProfiles(profile);
  std::vector<AutofillProfile> autofill_profiles;
  for (size_t i = 0; i < all_profiles.size(); ++i) {
    autofill_profiles.push_back(*all_profiles[i]);
  }
  for (int i = 0; i < num_profiles; ++i) {
    autofill_profiles.push_back(NextAutofillProfile());
  }
  SetProfiles(profile, &autofill_profiles);
}

void AutofillSyncPerfTest::UpdateProfiles(int profile) {
  const std::vector<AutofillProfile*>& all_profiles =
      GetAllProfiles(profile);
  std::vector<AutofillProfile> autofill_profiles;
  for (size_t i = 0; i < all_profiles.size(); ++i) {
    autofill_profiles.push_back(*all_profiles[i]);
    autofill_profiles.back().SetInfo(AutofillFieldType(NAME_FIRST),
                                     UTF8ToUTF16(NextName()));
  }
  SetProfiles(profile, &autofill_profiles);
}

void AutofillSyncPerfTest::RemoveProfiles(int profile) {
  std::vector<AutofillProfile> empty;
  SetProfiles(profile, &empty);
}

void AutofillSyncPerfTest::AddKeys(int profile, int num_keys) {
  std::set<AutofillKey> keys;
  for (int i = 0; i < num_keys; ++i) {
    keys.insert(NextAutofillKey());
  }
  autofill_helper::AddKeys(profile, keys);
}

void AutofillSyncPerfTest::RemoveKeys(int profile) {
  std::set<AutofillEntry> keys = GetAllKeys(profile);
  for (std::set<AutofillEntry>::const_iterator it = keys.begin();
       it != keys.end(); ++it) {
    RemoveKey(profile, it->key());
  }
}

const AutofillProfile AutofillSyncPerfTest::NextAutofillProfile() {
  AutofillProfile profile;
  autofill_test::SetProfileInfoWithGuid(&profile, NextGUID().c_str(),
                                        NextName().c_str(), "", "", "", "", "",
                                        "", "", "", "", "", "", "");
  return profile;
}

const AutofillKey AutofillSyncPerfTest::NextAutofillKey() {
  return AutofillKey(NextName().c_str(), NextName().c_str());
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
  return StringPrintf("Name%d", n);
}

const std::string AutofillSyncPerfTest::NextValue() {
  return IntToValue(value_number_++);
}

const std::string AutofillSyncPerfTest::IntToValue(int n) {
  return StringPrintf("Value%d", n);
}

IN_PROC_BROWSER_TEST_F(AutofillSyncPerfTest, AutofillProfiles_P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // TCM ID - 7557873.
  AddProfiles(0, kNumProfiles);
  base::TimeDelta dt =
      SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumProfiles, GetProfileCount(1));
  SyncTimingHelper::PrintResult("autofill", "add_autofill_profiles", dt);

  // TCM ID - 7549835.
  UpdateProfiles(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumProfiles, GetProfileCount(1));
  SyncTimingHelper::PrintResult("autofill", "update_autofill_profiles", dt);

  // TCM ID - 7553678.
  RemoveProfiles(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0, GetProfileCount(1));
  SyncTimingHelper::PrintResult("autofill", "delete_autofill_profiles", dt);
}

IN_PROC_BROWSER_TEST_F(AutofillSyncPerfTest, Autofill_P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddKeys(0, kNumKeys);
  base::TimeDelta dt =
      SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumKeys, GetKeyCount(1));
  SyncTimingHelper::PrintResult("autofill", "add_autofill_keys", dt);

  RemoveKeys(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0, GetKeyCount(1));
  SyncTimingHelper::PrintResult("autofill", "delete_autofill_keys", dt);
}
