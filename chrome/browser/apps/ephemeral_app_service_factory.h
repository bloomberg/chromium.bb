// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_EPHEMERAL_APP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_APPS_EPHEMERAL_APP_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class EphemeralAppService;
class Profile;

class EphemeralAppServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static EphemeralAppService* GetForProfile(Profile* profile);

  static EphemeralAppServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<EphemeralAppServiceFactory>;

  EphemeralAppServiceFactory();
  ~EphemeralAppServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_APPS_EPHEMERAL_APP_SERVICE_FACTORY_H_
