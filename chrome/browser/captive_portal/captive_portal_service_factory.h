// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_SERVICE_FACTORY_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace captive_portal {

class CaptivePortalService;

// Singleton that owns all CaptivePortalServices and associates them with
// Profiles.  Listens for the Profile's destruction notification and cleans up
// the associated CaptivePortalService.  Incognito profiles have their own
// CaptivePortalService.
class CaptivePortalServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the CaptivePortalService for |profile|.
  static CaptivePortalService* GetForProfile(Profile* profile);

  static CaptivePortalServiceFactory* GetInstance();

 private:
  friend class CaptivePortalBrowserTest;
  friend class CaptivePortalServiceTest;
  friend struct DefaultSingletonTraits<CaptivePortalServiceFactory>;

  CaptivePortalServiceFactory();
  virtual ~CaptivePortalServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceHasOwnInstanceInIncognito() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalServiceFactory);
};

}  // namespace captive_portal

#endif  // CHROME_BROWSER_CAPTIVE_PORTAL_CAPTIVE_PORTAL_SERVICE_FACTORY_H_
