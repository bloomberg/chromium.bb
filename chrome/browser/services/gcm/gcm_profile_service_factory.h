// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "components/gcm_driver/system_encryptor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace gcm {

class GCMProfileService;

// Singleton that owns all GCMProfileService and associates them with
// Profiles.
class GCMProfileServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static GCMProfileService* GetForProfile(Profile* profile);
  static GCMProfileServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<GCMProfileServiceFactory>;

  GCMProfileServiceFactory();
  virtual ~GCMProfileServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(GCMProfileServiceFactory);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_PROFILE_SERVICE_FACTORY_H_
