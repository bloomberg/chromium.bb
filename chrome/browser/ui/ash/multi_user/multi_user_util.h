// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_UTIL_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_UTIL_H_

#include <string>

class Profile;

namespace multi_user_util {

// Get the user id from a given profile.
std::string GetUserIDFromProfile(Profile* profile);

// Get the user id from an email address.
std::string GetUserIDFromEmail(const std::string& email);

// Get a profile for a given user id.
Profile* GetProfileFromUserID(const std::string& user_id);

// Check if the given profile is from the currently active user.
bool IsProfileFromActiveUser(Profile* profile);

}  // namespace multi_user_util

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_UTIL_H_
