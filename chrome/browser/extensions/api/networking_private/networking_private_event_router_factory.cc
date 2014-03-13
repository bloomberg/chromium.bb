// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_event_router_factory.h"

#include "chrome/browser/extensions/api/networking_private/networking_private_event_router.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_service_client_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
NetworkingPrivateEventRouter*
NetworkingPrivateEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<NetworkingPrivateEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
NetworkingPrivateEventRouterFactory*
NetworkingPrivateEventRouterFactory::GetInstance() {
  return Singleton<NetworkingPrivateEventRouterFactory>::get();
}

NetworkingPrivateEventRouterFactory::NetworkingPrivateEventRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "NetworkingPrivateEventRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
#if !defined(OS_CHROMEOS)
  DependsOn(NetworkingPrivateServiceClientFactory::GetInstance());
#endif
}

NetworkingPrivateEventRouterFactory::~NetworkingPrivateEventRouterFactory() {
}

KeyedService* NetworkingPrivateEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return NetworkingPrivateEventRouter::Create(
      Profile::FromBrowserContext(context));
}

content::BrowserContext*
NetworkingPrivateEventRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

bool NetworkingPrivateEventRouterFactory::
ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool NetworkingPrivateEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
