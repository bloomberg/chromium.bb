// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

template <typename T> struct DefaultSingletonTraits;
class EasyUnlockService;
class Profile;

// Singleton factory that builds and owns all EasyUnlockService.
class EasyUnlockServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static EasyUnlockServiceFactory* GetInstance();

  static EasyUnlockService* GetForProfile(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<EasyUnlockServiceFactory>;

  EasyUnlockServiceFactory();
  virtual ~EasyUnlockServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceFactory);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_FACTORY_H_
