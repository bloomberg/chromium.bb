// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_KEEP_ALIVE_SERVICE_FACTORY_H_
#define APPS_APP_KEEP_ALIVE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

namespace apps {

class AppKeepAliveService;

class AppKeepAliveServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppKeepAliveServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<AppKeepAliveServiceFactory>;

  AppKeepAliveServiceFactory();
  virtual ~AppKeepAliveServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

}  // namespace apps

#endif  // APPS_APP_KEEP_ALIVE_SERVICE_FACTORY_H_
