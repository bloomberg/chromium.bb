// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_load_service_factory.h"

#include "apps/app_load_service.h"
#include "apps/shell_window_registry.h"
#include "chrome/browser/extensions/extension_prefs_factory.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace apps {

// static
AppLoadService* AppLoadServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AppLoadService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

AppLoadServiceFactory* AppLoadServiceFactory::GetInstance() {
  return Singleton<AppLoadServiceFactory>::get();
}

AppLoadServiceFactory::AppLoadServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "AppLoadService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
  DependsOn(ShellWindowRegistry::Factory::GetInstance());
}

AppLoadServiceFactory::~AppLoadServiceFactory() {
}

BrowserContextKeyedService* AppLoadServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new AppLoadService(static_cast<Profile*>(profile));
}

bool AppLoadServiceFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

bool AppLoadServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* AppLoadServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace apps
