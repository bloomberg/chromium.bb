// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/test/live_sync/live_autofill_sync_test.h"

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, Client1HasData) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AutofillKeys keys;
  keys.insert(AutofillKey("name0", "value0"));
  keys.insert(AutofillKey("name0", "value1"));
  keys.insert(AutofillKey("name1", "value2"));
  keys.insert(AutofillKey(WideToUTF16(L"Sigur R\u00F3s"),
                          WideToUTF16(L"\u00C1g\u00E6tis byrjun")));
  AddFormFieldsToWebData(GetWebDataService(0), keys);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  AutofillKeys wd1_keys;
  GetAllAutofillKeys(GetWebDataService(1), &wd1_keys);

  // Note: In this test, name0-value0 and name0-value1 were both added in the
  // same transaction on Client0. However, only the first entry with name0 is
  // added, due to changes made in r55781. See crbug.com/51727.
  AutofillKeys expected_keys;
  expected_keys.insert(AutofillKey("name0", "value0"));
  expected_keys.insert(AutofillKey("name1", "value2"));
  expected_keys.insert(AutofillKey(WideToUTF16(L"Sigur R\u00F3s"),
                                   WideToUTF16(L"\u00C1g\u00E6tis byrjun")));
  EXPECT_EQ(expected_keys, wd1_keys);
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, BothHaveData) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AutofillKeys keys1;
  keys1.insert(AutofillKey("name0", "value0"));
  keys1.insert(AutofillKey("name1", "value2"));
  AddFormFieldsToWebData(GetWebDataService(0), keys1);

  AutofillKeys keys2;
  keys2.insert(AutofillKey("name0", "value1"));
  keys2.insert(AutofillKey("name2", "value3"));
  keys2.insert(AutofillKey("name3", "value3"));
  AddFormFieldsToWebData(GetWebDataService(1), keys2);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  // Note: In this test, name0-value0 and name0-value1 were added in separate
  // transactions -- one on Client0 and the other on Client1. Therefore, we
  // expect to see both pairs once sync completes and the lists are merged.
  AutofillKeys expected_keys;
  expected_keys.insert(AutofillKey("name0", "value0"));
  expected_keys.insert(AutofillKey("name0", "value1"));
  expected_keys.insert(AutofillKey("name1", "value2"));
  expected_keys.insert(AutofillKey("name2", "value3"));
  expected_keys.insert(AutofillKey("name3", "value3"));

  AutofillKeys wd0_keys;
  GetAllAutofillKeys(GetWebDataService(0), &wd0_keys);
  EXPECT_EQ(expected_keys, wd0_keys);

  AutofillKeys wd1_keys;
  GetAllAutofillKeys(GetWebDataService(1), &wd1_keys);
  EXPECT_EQ(expected_keys, wd1_keys);
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, Steady) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Client0 adds a key.
  AutofillKeys add_one_key;
  add_one_key.insert(AutofillKey("name0", "value0"));
  AddFormFieldsToWebData(GetWebDataService(0), add_one_key);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  AutofillKeys expected_keys;
  expected_keys.insert(AutofillKey("name0", "value0"));

  AutofillKeys keys;
  GetAllAutofillKeys(GetWebDataService(0), &keys);
  EXPECT_EQ(expected_keys, keys);
  keys.clear();
  GetAllAutofillKeys(GetWebDataService(1), &keys);
  EXPECT_EQ(expected_keys, keys);

  // Client1 adds a key.
  add_one_key.clear();
  add_one_key.insert(AutofillKey("name1", "value1"));
  AddFormFieldsToWebData(GetWebDataService(1), add_one_key);
  EXPECT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));

  expected_keys.insert(AutofillKey("name1", "value1"));
  keys.clear();
  GetAllAutofillKeys(GetWebDataService(0), &keys);
  EXPECT_EQ(expected_keys, keys);
  keys.clear();
  GetAllAutofillKeys(GetWebDataService(1), &keys);
  EXPECT_EQ(expected_keys, keys);

  // Client0 adds a key with the same name.
  add_one_key.clear();
  add_one_key.insert(AutofillKey("name1", "value2"));
  AddFormFieldsToWebData(GetWebDataService(0), add_one_key);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  expected_keys.insert(AutofillKey("name1", "value2"));
  keys.clear();
  GetAllAutofillKeys(GetWebDataService(0), &keys);
  EXPECT_EQ(expected_keys, keys);
  keys.clear();
  GetAllAutofillKeys(GetWebDataService(1), &keys);
  EXPECT_EQ(expected_keys, keys);

  // Client1 removes a key.
  RemoveKeyFromWebData(GetWebDataService(1), AutofillKey("name1", "value1"));
  EXPECT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));

  expected_keys.erase(AutofillKey("name1", "value1"));
  keys.clear();
  GetAllAutofillKeys(GetWebDataService(0), &keys);
  EXPECT_EQ(expected_keys, keys);
  keys.clear();
  GetAllAutofillKeys(GetWebDataService(1), &keys);
  EXPECT_EQ(expected_keys, keys);

  // Client0 removes the rest.
  RemoveKeyFromWebData(GetWebDataService(0), AutofillKey("name0", "value0"));
  RemoveKeyFromWebData(GetWebDataService(0), AutofillKey("name1", "value2"));
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  keys.clear();
  GetAllAutofillKeys(GetWebDataService(0), &keys);
  EXPECT_EQ(0U, keys.size());
  keys.clear();
  GetAllAutofillKeys(GetWebDataService(1), &keys);
  EXPECT_EQ(0U, keys.size());
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, ProfileClient1HasData) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AutoFillProfiles expected_profiles;
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_MARION, expected_profiles[0]);
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_HOMER, expected_profiles[1]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[0]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[1]);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));
  STLDeleteElements(&expected_profiles);
}

// TestScribe ID - 426761.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, ConflictLabels) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AutoFillProfiles profiles1;
  profiles1.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_HOMER, profiles1[0]);

  AutoFillProfiles profiles2;
  profiles2.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_MARION, profiles2[0]);
  profiles2[0]->set_label(ASCIIToUTF16("Shipping"));

  AddProfile(GetPersonalDataManager(0), *profiles1[0]);
  AddProfile(GetPersonalDataManager(1), *profiles2[0]);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  // Since client1 associates first, client2's "Shipping" profile will
  // be overwritten by the one stored in the cloud by profile1.
  EXPECT_TRUE(CompareAutoFillProfiles(profiles1,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(profiles1,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));
  STLDeleteElements(&profiles1);
  STLDeleteElements(&profiles2);
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest,
                       ProfileSameLabelOnClient1) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AutoFillProfiles expected_profiles;
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_HOMER, expected_profiles[0]);
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_HOMER, expected_profiles[1]);

  AddProfile(GetPersonalDataManager(0), *expected_profiles[0]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[1]);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  // One of the duplicate profiles will have its label renamed to
  // "Shipping2".
  expected_profiles[0]->set_label(ASCIIToUTF16("Shipping2"));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));
  STLDeleteElements(&expected_profiles);
}

IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, ProfileSteady) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Client0 adds a profile.
  AutoFillProfiles expected_profiles;
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_HOMER, expected_profiles[0]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[0]);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));

  // Client1 adds a profile.
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_MARION, expected_profiles[1]);
  AddProfile(GetPersonalDataManager(1), *expected_profiles[1]);
  EXPECT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));

  // Client0 adds a conflicting profile.
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_MARION, expected_profiles[2]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[2]);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // The conflicting profile's label will be made unique.
  expected_profiles[2]->set_label(ASCIIToUTF16("Billing2"));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));

  // Client1 removes a profile.
  delete expected_profiles.front();
  expected_profiles.erase(expected_profiles.begin());
  RemoveProfile(GetPersonalDataManager(1), ASCIIToUTF16("Shipping"));
  EXPECT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));

  // Client0 updates a profile.
  expected_profiles[0]->SetInfo(AutoFillType(NAME_FIRST),
                                ASCIIToUTF16("Bart"));
  UpdateProfile(GetPersonalDataManager(0),
                ASCIIToUTF16("Billing"),
                AutoFillType(NAME_FIRST),
                ASCIIToUTF16("Bart"));
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));

  // Client1 removes everything.
  STLDeleteElements(&expected_profiles);
  RemoveProfile(GetPersonalDataManager(1), ASCIIToUTF16("Billing"));
  RemoveProfile(GetPersonalDataManager(1), ASCIIToUTF16("Billing2"));
  EXPECT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));
}

// TestScribe ID - 426757.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, AddEmptyProfile) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AutoFillProfiles expected_profiles;
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_NULL, expected_profiles[0]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[0]);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));
  STLDeleteElements(&expected_profiles);
}

// TestScribe ID - 422839.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, AddProfile) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AutoFillProfiles expected_profiles;
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_HOMER, expected_profiles[0]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[0]);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));
  STLDeleteElements(&expected_profiles);
}

// TestScribe ID - 425335.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, AddMultipleProfiles) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AutoFillProfiles expected_profiles;
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_MARION, expected_profiles[0]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[0]);

  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_HOMER, expected_profiles[1]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[1]);

  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_NULL, expected_profiles[2]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[2]);

  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_FRASIER, expected_profiles[3]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[3]);

  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  EXPECT_TRUE(CompareAutoFillProfiles(
      GetAllAutoFillProfiles(GetPersonalDataManager(1)),
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  STLDeleteElements(&expected_profiles);
}

// TestScribe ID - 426758.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, AddUnicodeProfile) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  AutofillKeys expected_keys;
  const string16 kUnicodeString = WideToUTF16(L"\u00CE\u00F1\u0163\u00E9");
  expected_keys.insert(AutofillKey(kUnicodeString, kUnicodeString));
  AddFormFieldsToWebData(GetWebDataService(0), expected_keys);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  AutofillKeys synced_keys;
  GetAllAutofillKeys(GetWebDataService(1), &synced_keys);

  EXPECT_EQ(expected_keys, synced_keys);
}

// TestScribe ID - 425337.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, DeleteProfile) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AutoFillProfiles expected_profiles;
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_MARION, expected_profiles[0]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[0]);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));

  delete expected_profiles.front();
  expected_profiles.erase(expected_profiles.begin());
  RemoveProfile(GetPersonalDataManager(1), ASCIIToUTF16("Billing"));
  EXPECT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));
}

// TestScribe ID - 426760.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, MergeProfiles) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AutoFillProfiles expected_profiles;
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_HOMER, expected_profiles[0]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[0]);

  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_NULL, expected_profiles[1]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[1]);

  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_MARION, expected_profiles[2]);
  AddProfile(GetPersonalDataManager(1), *expected_profiles[2]);

  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_FRASIER, expected_profiles[3]);
  AddProfile(GetPersonalDataManager(1), *expected_profiles[3]);
  EXPECT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));
  STLDeleteElements(&expected_profiles);
}

// TestScribe ID - 426756.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, UpdateFields) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AutoFillProfiles expected_profiles;
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_HOMER, expected_profiles[0]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[0]);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));

  expected_profiles[0]->SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Lisa"));
  expected_profiles[0]->SetInfo(
                AutoFillType(EMAIL_ADDRESS), ASCIIToUTF16("grrrl@TV.com"));
  UpdateProfile(GetPersonalDataManager(0), ASCIIToUTF16("Billing"),
                AutoFillType(NAME_FIRST), ASCIIToUTF16("Lisa"));
  UpdateProfile(GetPersonalDataManager(0), ASCIIToUTF16("Billing"),
                AutoFillType(EMAIL_ADDRESS), ASCIIToUTF16("grrrl@TV.com"));
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  EXPECT_TRUE(CompareAutoFillProfiles(
      GetAllAutoFillProfiles(GetPersonalDataManager(0)),
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));
  STLDeleteElements(&expected_profiles);
}

// TestScribe ID - 426755.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, UpdateLabel) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AutoFillProfiles profiles0;
  profiles0.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_HOMER, profiles0[0]);

  AutoFillProfiles profiles1;
  profiles1.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_MARION, profiles1[0]);
  profiles1[0]->set_label(ASCIIToUTF16("Shipping"));

  AddProfile(GetPersonalDataManager(0), *profiles0[0]);
  AddProfile(GetPersonalDataManager(1), *profiles1[0]);

  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  EXPECT_TRUE(CompareAutoFillProfiles(profiles0,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(profiles0,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));
  STLDeleteElements(&profiles0);
  STLDeleteElements(&profiles1);
}

// TestScribe ID - 432634.
IN_PROC_BROWSER_TEST_F(TwoClientLiveAutofillSyncTest, ConflictFields) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AutoFillProfiles expected_profiles;
  expected_profiles.push_back(new AutoFillProfile(string16(), 0));
  FillProfile(PROFILE_HOMER, expected_profiles[0]);
  AddProfile(GetPersonalDataManager(0), *expected_profiles[0]);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(0))));
  EXPECT_TRUE(CompareAutoFillProfiles(expected_profiles,
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));

  UpdateProfile(GetPersonalDataManager(0), ASCIIToUTF16("Billing"),
                AutoFillType(NAME_FIRST), ASCIIToUTF16("Lisa"));
  UpdateProfile(GetPersonalDataManager(1), ASCIIToUTF16("Billing"),
                AutoFillType(NAME_FIRST), ASCIIToUTF16("Bart"));

  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  EXPECT_TRUE(CompareAutoFillProfiles(
      GetAllAutoFillProfiles(GetPersonalDataManager(0)),
      GetAllAutoFillProfiles(GetPersonalDataManager(1))));
  STLDeleteElements(&expected_profiles);
}
