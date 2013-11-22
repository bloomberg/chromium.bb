// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_service_client_factory.h"

#include "chrome/browser/extensions/api/networking_private/networking_private_service_client.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

// static
NetworkingPrivateServiceClient*
  NetworkingPrivateServiceClientFactory::GetForProfile(Profile* profile) {
  return static_cast<NetworkingPrivateServiceClient*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NetworkingPrivateServiceClientFactory*
    NetworkingPrivateServiceClientFactory::GetInstance() {
  return Singleton<NetworkingPrivateServiceClientFactory>::get();
}

NetworkingPrivateServiceClientFactory::NetworkingPrivateServiceClientFactory()
    : BrowserContextKeyedServiceFactory(
        "NetworkingPrivateServiceClient",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

NetworkingPrivateServiceClientFactory
    ::~NetworkingPrivateServiceClientFactory() {
}

BrowserContextKeyedService*
    NetworkingPrivateServiceClientFactory::BuildServiceInstanceFor(
        content::BrowserContext* profile) const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return new NetworkingPrivateServiceClient(
      wifi::WiFiService::Create(),
      NetworkingPrivateServiceClient::CryptoVerify::Create());
}

bool NetworkingPrivateServiceClientFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return false;
}

bool NetworkingPrivateServiceClientFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace extensions
