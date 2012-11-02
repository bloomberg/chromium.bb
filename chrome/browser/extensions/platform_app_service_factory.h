// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/platform_app_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class PlatformAppServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static PlatformAppService* GetForProfile(Profile* profile);

  static PlatformAppServiceFactory* GetInstance();
 private:
  friend struct DefaultSingletonTraits<PlatformAppServiceFactory>;

  PlatformAppServiceFactory();
  virtual ~PlatformAppServiceFactory();

  // ProfileKeyedServiceFactory
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceHasOwnInstanceInIncognito() const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_SERVICE_FACTORY_H_
