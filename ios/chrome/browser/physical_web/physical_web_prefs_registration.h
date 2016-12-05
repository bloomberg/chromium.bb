// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHYSICAL_WEB_PHYSICAL_WEB_PREFS_REGISTRATION_H_
#define IOS_CHROME_BROWSER_PHYSICAL_WEB_PHYSICAL_WEB_PREFS_REGISTRATION_H_

class PrefRegistrySimple;
namespace user_prefs {
class PrefRegistrySyncable;
}

// Registers browser state prefs needed by the Physical Web.
void RegisterPhysicalWebBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry);

// Registers local state prefs needed by the Physical Web.
void RegisterPhysicalWebLocalStatePrefs(PrefRegistrySimple* registry);

#endif  // IOS_CHROME_BROWSER_PHYSICAL_WEB_PHYSICAL_WEB_PREFS_REGISTRATION_H_
