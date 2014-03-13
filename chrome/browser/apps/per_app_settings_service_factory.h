// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PER_APP_SETTINGS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_APPS_PER_APP_SETTINGS_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class PerAppSettingsService;

template <typename T> struct DefaultSingletonTraits;
class PerAppSettingsServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PerAppSettingsServiceFactory* GetInstance();
  static PerAppSettingsService* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  friend struct DefaultSingletonTraits<PerAppSettingsServiceFactory>;

  PerAppSettingsServiceFactory();
  virtual ~PerAppSettingsServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
};

#endif  // CHROME_BROWSER_APPS_PER_APP_SETTINGS_SERVICE_FACTORY_H_
