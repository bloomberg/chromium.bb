// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_LIFETIME_MONITOR_FACTORY_H_
#define APPS_APP_LIFETIME_MONITOR_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps {

class AppLifetimeMonitor;

// Singleton that owns all AppLifetimeMonitors and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated AppLifetimeMonitor.
class AppLifetimeMonitorFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppLifetimeMonitor* GetForProfile(Profile* profile);

  static AppLifetimeMonitorFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<AppLifetimeMonitorFactory>;

  AppLifetimeMonitorFactory();
  virtual ~AppLifetimeMonitorFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
    content::BrowserContext* context) const OVERRIDE;
};

}  // namespace apps

#endif  // APPS_APP_LIFETIME_MONITOR_FACTORY_H_
