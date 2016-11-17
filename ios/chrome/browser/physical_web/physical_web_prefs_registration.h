// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHYSICAL_WEB_PHYSICAL_WEB_PREFS_REGISTRATION_H_
#define IOS_CHROME_BROWSER_PHYSICAL_WEB_PHYSICAL_WEB_PREFS_REGISTRATION_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

// Registers the prefs needed by physical web.
void RegisterPhysicalWebBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry);

#endif  // IOS_CHROME_BROWSER_PHYSICAL_WEB_PHYSICAL_WEB_PREFS_REGISTRATION_H_
