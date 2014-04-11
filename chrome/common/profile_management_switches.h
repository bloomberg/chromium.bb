// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These are functions to access various profile-management flags but with
// possible overrides from Experiements.  This is done inside chrome/common
// because it is accessed by files through the chrome/ directory tree.

#ifndef CHROME_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_
#define CHROME_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_

namespace switches {

// Enables the web-based sign in flow on Chrome desktop.
bool IsEnableWebBasedSignin();

// Enables using GAIA information to populate profile name and icon.
bool IsGoogleProfileInfo();

// Shows the new avatar menu.
bool IsNewAvatarMenu();

// Use new profile management system, including profile sign-out and new
// choosers.
bool IsNewProfileManagement();

// Checks whether the flag for fast user switching is enabled.
bool IsFastUserSwitching();

}  // namespace switches

#endif  // CHROME_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_
