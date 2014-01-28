// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registry_factory.h"

#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"

using content::BrowserContext;

namespace extensions {

// static
ExtensionRegistry* ExtensionRegistryFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ExtensionRegistry*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionRegistryFactory* ExtensionRegistryFactory::GetInstance() {
  return Singleton<ExtensionRegistryFactory>::get();
}

ExtensionRegistryFactory::ExtensionRegistryFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionRegistry",
          BrowserContextDependencyManager::GetInstance()) {
  // No dependencies on other services.
}

ExtensionRegistryFactory::~ExtensionRegistryFactory() {}

BrowserContextKeyedService* ExtensionRegistryFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ExtensionRegistry;
}

BrowserContext* ExtensionRegistryFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Redirected in incognito.
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

}  // namespace extensions
