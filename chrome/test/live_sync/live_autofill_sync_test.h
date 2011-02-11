// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_AUTOFILL_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_AUTOFILL_SYNC_TEST_H_
#pragma once

#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/test/live_sync/live_sync_test.h"

class AutofillEntry;
class AutofillKey;
class AutoFillProfile;
class WebDataService;

class LiveAutofillSyncTest : public LiveSyncTest {
 public:
  enum ProfileType {
    PROFILE_MARION,
    PROFILE_HOMER,
    PROFILE_FRASIER,
    PROFILE_NULL
  };

  explicit LiveAutofillSyncTest(TestType test_type);
  virtual ~LiveAutofillSyncTest();

  // Used to access the web data service within a particular sync profile.
  WebDataService* GetWebDataService(int index) WARN_UNUSED_RESULT;

  // Used to access the personal data manager within a particular sync profile.
  PersonalDataManager* GetPersonalDataManager(int index) WARN_UNUSED_RESULT;

  // Adds the form fields in |keys| to the WebDataService of sync profile
  // |profile|.
  void AddKeys(int profile, const std::set<AutofillKey>& keys);
  // Removes the form field in |key| from the WebDataService of sync profile
  // |profile|.
  void RemoveKey(int profile, const AutofillKey& key);

  // Gets all the form fields in the WebDataService of sync profile |profile|.
  std::set<AutofillEntry> GetAllKeys(int profile) WARN_UNUSED_RESULT;

  // Compares the form fields in the WebDataServices of sync profiles
  // |profile_a| and |profile_b|. Returns true if they match.
  bool KeysMatch(int profile_a, int profile_b) WARN_UNUSED_RESULT;

  // Replaces the Autofill profiles in sync profile |profile| with
  // |autofill_profiles|.
  void SetProfiles(
      int profile, std::vector<AutoFillProfile>* autofill_profiles);

  // Adds the autofill profile |autofill_profile| to sync profile |profile|.
  void AddProfile(int profile, const AutoFillProfile& autofill_profile);

  // Removes the autofill profile with guid |guid| from sync profile
  // |profile|.
  void RemoveProfile(int profile, const std::string& guid);

  // Updates the autofill profile with guid |guid| in sync profile |profile|
  // to |type| and |value|.
  void UpdateProfile(int profile,
                     const std::string& guid,
                     const AutoFillType& type,
                     const string16& value);

  // Gets all the Autofill profiles in the PersonalDataManager of sync profile
  // |profile|.
  const std::vector<AutoFillProfile*>& GetAllProfiles(int profile)
      WARN_UNUSED_RESULT;

  // Compares the Autofill profiles in the PersonalDataManagers of sync profiles
  // |profile_a| and |profile_b|. Returns true if they match.
  bool ProfilesMatch(int profile_a, int profile_b) WARN_UNUSED_RESULT;

 private:
  DISALLOW_COPY_AND_ASSIGN(LiveAutofillSyncTest);
};

AutoFillProfile CreateAutofillProfile(LiveAutofillSyncTest::ProfileType type);

class TwoClientLiveAutofillSyncTest : public LiveAutofillSyncTest {
 public:
  TwoClientLiveAutofillSyncTest() : LiveAutofillSyncTest(TWO_CLIENT) {}
  ~TwoClientLiveAutofillSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientLiveAutofillSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_AUTOFILL_SYNC_TEST_H_
