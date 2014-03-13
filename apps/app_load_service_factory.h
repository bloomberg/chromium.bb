// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_LOAD_SERVICE_FACTORY_H_
#define APPS_APP_LOAD_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps {

class AppLoadService;

class AppLoadServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppLoadService* GetForProfile(Profile* profile);

  static AppLoadServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<AppLoadServiceFactory>;

  AppLoadServiceFactory();
  virtual ~AppLoadServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
};

}  // namespace apps

#endif  // APPS_APP_LOAD_SERVICE_FACTORY_H_
