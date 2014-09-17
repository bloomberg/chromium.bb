// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_load_service_factory.h"

#include "apps/app_load_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

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
  DependsOn(extensions::AppWindowRegistry::Factory::GetInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

AppLoadServiceFactory::~AppLoadServiceFactory() {
}

KeyedService* AppLoadServiceFactory::BuildServiceInstanceFor(
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
  // Redirected in incognito.
  return extensions::ExtensionsBrowserClient::Get()->
      GetOriginalContext(context);
}

}  // namespace apps
