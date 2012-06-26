// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_UI_PREFS_H_
#define CHROME_BROWSER_UI_BROWSER_UI_PREFS_H_
#pragma once

#include <string>

class PrefService;
class Profile;

namespace chrome {

// Sets the value of homepage related prefs to new values. Since we do not
// want to change these values for existing users, we can not change the
// default values under RegisterUserPrefs. Also if user already has an
// existing profile we do not want to override those preferences so we only
// set new values if they have not been set already. This method gets called
// during First Run.
void SetNewHomePagePrefs(PrefService* prefs);

void RegisterBrowserPrefs(PrefService* prefs);
void RegisterBrowserUserPrefs(PrefService* prefs);

// Create a preference dictionary for the provided application name, in the
// given user profile. This is done only once per application name / per
// session / per user profile.
void RegisterAppPrefs(const std::string& app_name, Profile* profile);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_UI_PREFS_H_
