// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_CONTROLLER_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_CONTROLLER_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

// Stub class for Easy Unlock features.
class EasyUnlockController {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_CONTROLLER_H_
