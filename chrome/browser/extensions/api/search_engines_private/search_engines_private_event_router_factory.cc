// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/search_engines_private/search_engines_private_event_router.h"
#include "chrome/browser/extensions/api/search_engines_private/search_engines_private_event_router_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
SearchEnginesPrivateEventRouter*
SearchEnginesPrivateEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<SearchEnginesPrivateEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SearchEnginesPrivateEventRouterFactory*
SearchEnginesPrivateEventRouterFactory::GetInstance() {
  return base::Singleton<SearchEnginesPrivateEventRouterFactory>::get();
}

SearchEnginesPrivateEventRouterFactory::SearchEnginesPrivateEventRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "SearchEnginesPrivateEventRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

SearchEnginesPrivateEventRouterFactory::
    ~SearchEnginesPrivateEventRouterFactory() {
}

KeyedService* SearchEnginesPrivateEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return SearchEnginesPrivateEventRouter::Create(context);
}

content::BrowserContext*
SearchEnginesPrivateEventRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

bool SearchEnginesPrivateEventRouterFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool SearchEnginesPrivateEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace extensions
