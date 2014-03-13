// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_restore_service_factory.h"

#include "apps/app_lifetime_monitor_factory.h"
#include "apps/app_restore_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace apps {

// static
AppRestoreService* AppRestoreServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AppRestoreService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

AppRestoreServiceFactory* AppRestoreServiceFactory::GetInstance() {
  return Singleton<AppRestoreServiceFactory>::get();
}

AppRestoreServiceFactory::AppRestoreServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "AppRestoreService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AppLifetimeMonitorFactory::GetInstance());
}

AppRestoreServiceFactory::~AppRestoreServiceFactory() {
}

KeyedService* AppRestoreServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new AppRestoreService(static_cast<Profile*>(profile));
}

bool AppRestoreServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace apps
