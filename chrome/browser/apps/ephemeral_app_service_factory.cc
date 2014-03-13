// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/ephemeral_app_service_factory.h"

#include "chrome/browser/apps/ephemeral_app_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

using extensions::ExtensionsBrowserClient;

// static
EphemeralAppService*
EphemeralAppServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<EphemeralAppService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
EphemeralAppServiceFactory* EphemeralAppServiceFactory::GetInstance() {
  return Singleton<EphemeralAppServiceFactory>::get();
}

EphemeralAppServiceFactory::EphemeralAppServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "EphemeralAppService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

EphemeralAppServiceFactory::~EphemeralAppServiceFactory() {
}

KeyedService* EphemeralAppServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new EphemeralAppService(Profile::FromBrowserContext(context));
}

content::BrowserContext* EphemeralAppServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

bool EphemeralAppServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool EphemeralAppServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
