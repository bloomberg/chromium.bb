// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_lifetime_monitor_factory.h"

#include "apps/app_lifetime_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extensions_browser_client.h"

namespace apps {

// static
AppLifetimeMonitor* AppLifetimeMonitorFactory::GetForProfile(Profile* profile) {
  return static_cast<AppLifetimeMonitor*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

AppLifetimeMonitorFactory* AppLifetimeMonitorFactory::GetInstance() {
  return Singleton<AppLifetimeMonitorFactory>::get();
}

AppLifetimeMonitorFactory::AppLifetimeMonitorFactory()
    : BrowserContextKeyedServiceFactory(
        "AppLifetimeMonitor",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::AppWindowRegistry::Factory::GetInstance());
}

AppLifetimeMonitorFactory::~AppLifetimeMonitorFactory() {}

KeyedService* AppLifetimeMonitorFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new AppLifetimeMonitor(static_cast<Profile*>(profile));
}

bool AppLifetimeMonitorFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* AppLifetimeMonitorFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return extensions::ExtensionsBrowserClient::Get()->
      GetOriginalContext(context);
}

}  // namespace apps
