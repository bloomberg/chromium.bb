// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_AUTOFILL_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_AUTOFILL_HELPER_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"

class AutofillEntry;
class AutofillKey;
class AutofillProfile;
class AutofillType;
class PersonalDataManager;
class WebDataService;

namespace autofill_helper {

enum ProfileType {
  PROFILE_MARION,
  PROFILE_HOMER,
  PROFILE_FRASIER,
  PROFILE_NULL
};

// Used to access the web data service within a particular sync profile.
scoped_refptr<WebDataService> GetWebDataService(int index) WARN_UNUSED_RESULT;

// Used to access the personal data manager within a particular sync profile.
PersonalDataManager* GetPersonalDataManager(int index) WARN_UNUSED_RESULT;

// Adds the form fields in |keys| to the WebDataService of sync profile
// |profile|.
void AddKeys(int profile, const std::set<AutofillKey>& keys);

// Removes the form field in |key| from the WebDataService of sync profile
// |profile|.
void RemoveKey(int profile, const AutofillKey& key);

// Removes all of the keys from the WebDataService of sync profile |profile|.
void RemoveKeys(int profile);

// Gets all the form fields in the WebDataService of sync profile |profile|.
std::set<AutofillEntry> GetAllKeys(int profile) WARN_UNUSED_RESULT;

// Compares the form fields in the WebDataServices of sync profiles
// |profile_a| and |profile_b|. Returns true if they match.
bool KeysMatch(int profile_a, int profile_b) WARN_UNUSED_RESULT;

// Replaces the Autofill profiles in sync profile |profile| with
// |autofill_profiles|.
void SetProfiles(int profile, std::vector<AutofillProfile>* autofill_profiles);

// Adds the autofill profile |autofill_profile| to sync profile |profile|.
void AddProfile(int profile, const AutofillProfile& autofill_profile);

// Removes the autofill profile with guid |guid| from sync profile
// |profile|.
void RemoveProfile(int profile, const std::string& guid);

// Updates the autofill profile with guid |guid| in sync profile |profile|
// to |type| and |value|.
void UpdateProfile(int profile,
                   const std::string& guid,
                   const AutofillType& type,
                   const string16& value);

// Gets all the Autofill profiles in the PersonalDataManager of sync profile
// |profile|.
const std::vector<AutofillProfile*>& GetAllProfiles(
    int profile) WARN_UNUSED_RESULT;

// Returns the number of autofill profiles contained by sync profile
// |profile|.
int GetProfileCount(int profile);

// Returns the number of autofill keys contained by sync profile |profile|.
int GetKeyCount(int profile);

// Compares the Autofill profiles in the PersonalDataManagers of sync profiles
// |profile_a| and |profile_b|. Returns true if they match.
bool ProfilesMatch(int profile_a, int profile_b) WARN_UNUSED_RESULT;

// Compares the autofill profiles for all sync profiles, and returns true if
// they all match.
bool AllProfilesMatch() WARN_UNUSED_RESULT;

// Creates a test autofill profile based on the persona specified in |type|.
AutofillProfile CreateAutofillProfile(ProfileType type);

}  // namespace autofill_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_AUTOFILL_HELPER_H_
