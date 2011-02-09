// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_autofill_sync_test.h"

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, WebDataServiceSanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Client0 adds a key.
  std::set<AutofillKey> keys;
  keys.insert(AutofillKey("name0", "value0"));
  AddKeys(0, keys);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(KeysMatch(0, 1));
  ASSERT_EQ(1U, GetAllKeys(0).size());

  // Client1 adds a key.
  keys.clear();
  keys.insert(AutofillKey("name1", "value1-0"));
  AddKeys(1, keys);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(KeysMatch(0, 1));
  ASSERT_EQ(2U, GetAllKeys(0).size());

  // Client0 adds a key with the same name.
  keys.clear();
  keys.insert(AutofillKey("name1", "value1-1"));
  AddKeys(0, keys);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(KeysMatch(0, 1));
  ASSERT_EQ(3U, GetAllKeys(0).size());

  // Client1 removes a key.
  RemoveKey(1, AutofillKey("name1", "value1-0"));
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(KeysMatch(0, 1));
  ASSERT_EQ(2U, GetAllKeys(0).size());

  // Client0 removes the rest.
  RemoveKey(0, AutofillKey("name0", "value0"));
  RemoveKey(0, AutofillKey("name1", "value1-1"));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(KeysMatch(0, 1));
  ASSERT_EQ(0U, GetAllKeys(0).size());
}

// TestScribe ID - 426758.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, AddUnicodeProfile) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  std::set<AutofillKey> keys;
  keys.insert(AutofillKey(WideToUTF16(L"Sigur R\u00F3s"),
                          WideToUTF16(L"\u00C1g\u00E6tis byrjun")));
  AddKeys(0, keys);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(KeysMatch(0, 1));
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest,
                       AddDuplicateNamesToSameProfile) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  std::set<AutofillKey> keys;
  keys.insert(AutofillKey("name0", "value0-0"));
  keys.insert(AutofillKey("name0", "value0-1"));
  keys.insert(AutofillKey("name1", "value1"));
  AddKeys(0, keys);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(KeysMatch(0, 1));
  ASSERT_EQ(2U, GetAllKeys(0).size());
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest,
                       AddDuplicateNamesToDifferentProfiles) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  std::set<AutofillKey> keys0;
  keys0.insert(AutofillKey("name0", "value0-0"));
  keys0.insert(AutofillKey("name1", "value1"));
  AddKeys(0, keys0);

  std::set<AutofillKey> keys1;
  keys1.insert(AutofillKey("name0", "value0-1"));
  keys1.insert(AutofillKey("name2", "value2"));
  keys1.insert(AutofillKey("name3", "value3"));
  AddKeys(1, keys1);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(KeysMatch(0, 1));
  ASSERT_EQ(5U, GetAllKeys(0).size());
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest,
                       PersonalDataManagerSanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Client0 adds a profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(1U, GetAllProfiles(0).size());

  // Client1 adds a profile.
  AddProfile(1, CreateAutofillProfile(PROFILE_MARION));
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(2U, GetAllProfiles(0).size());

  // Client0 adds the same profile.
  AddProfile(0, CreateAutofillProfile(PROFILE_MARION));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(2U, GetAllProfiles(0).size());

  // Client1 removes a profile.
  RemoveProfile(1, GetAllProfiles(1)[0]->guid());
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(1U, GetAllProfiles(0).size());

  // Client0 updates a profile.
  UpdateProfile(0, GetAllProfiles(0)[0]->guid(), AutoFillType(NAME_FIRST),
      ASCIIToUTF16("Bart"));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(1U, GetAllProfiles(0).size());

  // Client1 removes remaining profile.
  RemoveProfile(1, GetAllProfiles(1)[0]->guid());
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(0U, GetAllProfiles(0).size());
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, AddDuplicateProfiles) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(1U, GetAllProfiles(0).size());
}

// TestScribe ID - 426761.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, SameProfileWithConflict) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AutoFillProfile profile0 = CreateAutofillProfile(PROFILE_HOMER);
  AutoFillProfile profile1 = CreateAutofillProfile(PROFILE_HOMER);
  profile1.SetInfo(AutoFillType(PHONE_FAX_WHOLE_NUMBER),
                   ASCIIToUTF16("1234567890"));

  AddProfile(0, profile0);
  AddProfile(1, profile1);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(1U, GetAllProfiles(0).size());
}

// TestScribe ID - 426757.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, AddEmptyProfile) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddProfile(0, CreateAutofillProfile(PROFILE_NULL));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(0U, GetAllProfiles(0).size());
}

// TestScribe ID - 422839.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, AddProfile) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(1U, GetAllProfiles(0).size());
}

// TestScribe ID - 425335.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, AddMultipleProfiles) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(0, CreateAutofillProfile(PROFILE_MARION));
  AddProfile(0, CreateAutofillProfile(PROFILE_FRASIER));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(3U, GetAllProfiles(0).size());
}

// TestScribe ID - 425337.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, DeleteProfile) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(1U, GetAllProfiles(0).size());

  RemoveProfile(1, GetAllProfiles(1)[0]->guid());
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(0U, GetAllProfiles(0).size());
}

// TestScribe ID - 426760.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, MergeProfiles) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  AddProfile(1, CreateAutofillProfile(PROFILE_MARION));
  AddProfile(1, CreateAutofillProfile(PROFILE_FRASIER));
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(3U, GetAllProfiles(0).size());
}

// TestScribe ID - 426756.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, UpdateFields) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(1U, GetAllProfiles(0).size());

  UpdateProfile(0, GetAllProfiles(0)[0]->guid(), AutoFillType(NAME_FIRST),
      ASCIIToUTF16("Lisa"));
  UpdateProfile(0, GetAllProfiles(0)[0]->guid(), AutoFillType(EMAIL_ADDRESS),
      ASCIIToUTF16("grrrl@TV.com"));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(1U, GetAllProfiles(0).size());
}

// TestScribe ID - 432634.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, ConflictingFields) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddProfile(0, CreateAutofillProfile(PROFILE_HOMER));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(1U, GetAllProfiles(0).size());
  UpdateProfile(0, GetAllProfiles(0)[0]->guid(), AutoFillType(NAME_FIRST),
      ASCIIToUTF16("Lisa"));
  UpdateProfile(1, GetAllProfiles(1)[0]->guid(), AutoFillType(NAME_FIRST),
      ASCIIToUTF16("Bart"));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(ProfilesMatch(0,1));
  ASSERT_EQ(1U, GetAllProfiles(0).size());
}
