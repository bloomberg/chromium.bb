// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_FACTORY_H_

#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ChromeGeolocationPermissionContext;
class Profile;

class ChromeGeolocationPermissionContextFactory
    : public ProfileKeyedServiceFactory {
 public:
  static ChromeGeolocationPermissionContext* GetForProfile(Profile* profile);

  static ChromeGeolocationPermissionContextFactory* GetInstance();

 private:
  friend struct
      DefaultSingletonTraits<ChromeGeolocationPermissionContextFactory>;

  ChromeGeolocationPermissionContextFactory();
  virtual ~ChromeGeolocationPermissionContextFactory();

  // |ProfileKeyedBaseFactory| methods:
  virtual ProfileKeyedService*
      BuildServiceInstanceFor(Profile* profile) const OVERRIDE;
  virtual void RegisterUserPrefs(PrefServiceSyncable* user_prefs) OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromeGeolocationPermissionContextFactory);
};

#endif  // CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_FACTORY_H_
