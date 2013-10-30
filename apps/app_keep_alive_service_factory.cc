// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_keep_alive_service_factory.h"

#include "apps/app_keep_alive_service.h"
#include "apps/app_lifetime_monitor_factory.h"
#include "apps/shell_window_registry.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace apps {

// static
AppKeepAliveServiceFactory* AppKeepAliveServiceFactory::GetInstance() {
  return Singleton<AppKeepAliveServiceFactory>::get();
}

AppKeepAliveServiceFactory::AppKeepAliveServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AppKeepAliveService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AppLifetimeMonitorFactory::GetInstance());
}

AppKeepAliveServiceFactory::~AppKeepAliveServiceFactory() {}

BrowserContextKeyedService* AppKeepAliveServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppKeepAliveService(context);
}

bool AppKeepAliveServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* AppKeepAliveServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool AppKeepAliveServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace apps
