// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_UI_PREFS_H_
#define CHROME_BROWSER_UI_BROWSER_UI_PREFS_H_

#include <string>

class PrefRegistrySimple;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chrome {

void RegisterBrowserPrefs(PrefRegistrySimple* registry);
void RegisterBrowserUserPrefs(user_prefs::PrefRegistrySyncable* registry);

// Create a preference dictionary for the provided application name, in the
// given user profile. This is done only once per application name / per
// session / per user profile.
void RegisterAppPrefs(const std::string& app_name, Profile* profile);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_UI_PREFS_H_
