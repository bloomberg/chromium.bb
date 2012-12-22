// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_FACTORY_H_

#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"

class ChromeGeolocationPermissionContext;
class Profile;

class ChromeGeolocationPermissionContextFactory {
 public:
  ChromeGeolocationPermissionContextFactory() {}
  ~ChromeGeolocationPermissionContextFactory() {}
  static ChromeGeolocationPermissionContext* Create(Profile* profile);
  static void RegisterUserPrefs(PrefServiceSyncable* user_prefs);

 private:

  DISALLOW_COPY_AND_ASSIGN(ChromeGeolocationPermissionContextFactory);
};

#endif  // CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_FACTORY_H_
