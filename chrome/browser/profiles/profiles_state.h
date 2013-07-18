// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILES_STATE_H_
#define CHROME_BROWSER_PROFILES_PROFILES_STATE_H_

class PrefRegistrySimple;
namespace base { class FilePath; }

namespace profiles {

// Checks if multiple profiles is enabled.
bool IsMultipleProfilesEnabled();

// Checks if new profile management is enabled.
bool IsNewProfileManagementEnabled();

// Returns the path to the default profile directory, based on the given
// user data directory.
base::FilePath GetDefaultProfileDir(const base::FilePath& user_data_dir);

// Returns the path to the preferences file given the user profile directory.
base::FilePath GetProfilePrefsPath(const base::FilePath& profile_dir);

// Register multi-profile related preferences in Local State.
void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILES_STATE_H_
