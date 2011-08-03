// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_AUTOFILL_HELPER_H_
#define CHROME_TEST_LIVE_SYNC_AUTOFILL_HELPER_H_
#pragma once

#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/test/live_sync/sync_datatype_helper.h"

class AutofillEntry;
class AutofillKey;
class AutofillProfile;
class WebDataService;

class AutofillHelper : public SyncDatatypeHelper {
 public:
  enum ProfileType {
    PROFILE_MARION,
    PROFILE_HOMER,
    PROFILE_FRASIER,
    PROFILE_NULL
  };

  // Used to access the web data service within a particular sync profile.
  static WebDataService* GetWebDataService(int index) WARN_UNUSED_RESULT;

  // Used to access the personal data manager within a particular sync profile.
  static PersonalDataManager* GetPersonalDataManager(
      int index) WARN_UNUSED_RESULT;

  // Adds the form fields in |keys| to the WebDataService of sync profile
  // |profile|.
  static void AddKeys(int profile, const std::set<AutofillKey>& keys);

  // Removes the form field in |key| from the WebDataService of sync profile
  // |profile|.
  static void RemoveKey(int profile, const AutofillKey& key);

  // Gets all the form fields in the WebDataService of sync profile |profile|.
  static std::set<AutofillEntry> GetAllKeys(int profile) WARN_UNUSED_RESULT;

  // Compares the form fields in the WebDataServices of sync profiles
  // |profile_a| and |profile_b|. Returns true if they match.
  static bool KeysMatch(int profile_a, int profile_b) WARN_UNUSED_RESULT;

  // Replaces the Autofill profiles in sync profile |profile| with
  // |autofill_profiles|.
  static void SetProfiles(int profile,
                          std::vector<AutofillProfile>* autofill_profiles);

  // Adds the autofill profile |autofill_profile| to sync profile |profile|.
  static void AddProfile(int profile, const AutofillProfile& autofill_profile);

  // Removes the autofill profile with guid |guid| from sync profile
  // |profile|.
  static void RemoveProfile(int profile, const std::string& guid);

  // Updates the autofill profile with guid |guid| in sync profile |profile|
  // to |type| and |value|.
  static void UpdateProfile(int profile,
                            const std::string& guid,
                            const AutofillType& type,
                            const string16& value);

  // Gets all the Autofill profiles in the PersonalDataManager of sync profile
  // |profile|.
  static const std::vector<AutofillProfile*>& GetAllProfiles(
      int profile) WARN_UNUSED_RESULT;

  // Returns the number of autofill profiles contained by sync profile
  // |profile|.
  static int GetProfileCount(int profile);

  // Compares the Autofill profiles in the PersonalDataManagers of sync profiles
  // |profile_a| and |profile_b|. Returns true if they match.
  static bool ProfilesMatch(int profile_a, int profile_b) WARN_UNUSED_RESULT;

  // Compares the autofill profiles for all sync profiles, and returns true if
  // they all match.
  static bool AllProfilesMatch() WARN_UNUSED_RESULT;

  // Creates a test autofill profile based on the persona specified in |type|.
  static AutofillProfile CreateAutofillProfile(ProfileType type);
 protected:
  AutofillHelper();
  virtual ~AutofillHelper();

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillHelper);
};

#endif  // CHROME_TEST_LIVE_SYNC_AUTOFILL_HELPER_H_
