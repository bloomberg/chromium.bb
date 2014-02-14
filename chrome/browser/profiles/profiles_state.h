// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILES_STATE_H_
#define CHROME_BROWSER_PROFILES_PROFILES_STATE_H_

#include <vector>
#include "base/strings/string16.h"

class Browser;
class PrefRegistrySimple;
class Profile;
namespace base { class FilePath; }

namespace profiles {

// Checks if multiple profiles is enabled.
bool IsMultipleProfilesEnabled();

// Returns the path to the default profile directory, based on the given
// user data directory.
base::FilePath GetDefaultProfileDir(const base::FilePath& user_data_dir);

// Register multi-profile related preferences in Local State.
void RegisterPrefs(PrefRegistrySimple* registry);

// Returns the display name of the active on-the-record profile (or guest).
base::string16 GetActiveProfileDisplayName(Browser* browser);

// Update the name of |profile| to |new_profile_name|. This updates the
// profile preferences, which triggers an update in the ProfileInfoCache.
void UpdateProfileName(Profile* profile,
                       const base::string16& new_profile_name);

// Returns the list of secondary accounts for a specific |profile|, which is
// all the email addresses associated with the profile that are not equal to
// the |primary_account|.
std::vector<std::string> GetSecondaryAccountsForProfile(
    Profile* profile,
    const std::string& primary_account);

// Returns whether the |browser|'s profile is a non-incognito or guest profile.
// The distinction is needed because guest profiles are implemented as
// incognito profiles.
bool IsRegularOrGuestSession(Browser* browser);

}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILES_STATE_H_
