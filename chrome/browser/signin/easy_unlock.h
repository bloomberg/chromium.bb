// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;

namespace easy_unlock {

// Registers Easy Unlock profile preferences.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Launches Easy Unlock Setup app.
void LaunchEasyUnlockSetup(Profile* profile);

}  // namespace easy_unlock

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_H_
