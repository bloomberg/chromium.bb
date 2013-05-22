// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/discovery/suggested_links_registry_factory.h"

#include "chrome/browser/extensions/api/discovery/suggested_links_registry.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace extensions {

// static
SuggestedLinksRegistry* SuggestedLinksRegistryFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SuggestedLinksRegistry*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SuggestedLinksRegistryFactory* SuggestedLinksRegistryFactory::GetInstance() {
  return Singleton<SuggestedLinksRegistryFactory>::get();
}

bool SuggestedLinksRegistryFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

SuggestedLinksRegistryFactory::SuggestedLinksRegistryFactory()
    : BrowserContextKeyedServiceFactory(
        "SuggestedLinksRegistry",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

SuggestedLinksRegistryFactory::~SuggestedLinksRegistryFactory() {
}

BrowserContextKeyedService*
SuggestedLinksRegistryFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new SuggestedLinksRegistry();
}

content::BrowserContext* SuggestedLinksRegistryFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace extensions
