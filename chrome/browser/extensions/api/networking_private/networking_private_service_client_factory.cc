// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_service_client_factory.h"

#include "chrome/browser/extensions/api/networking_private/networking_private_delegate.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_service_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

// static
NetworkingPrivateDelegate* NetworkingPrivateDelegate::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return NetworkingPrivateServiceClientFactory::GetForBrowserContext(
      browser_context);
}

// static
NetworkingPrivateServiceClient*
NetworkingPrivateServiceClientFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<NetworkingPrivateServiceClient*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
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
}

NetworkingPrivateServiceClientFactory
    ::~NetworkingPrivateServiceClientFactory() {
}

KeyedService* NetworkingPrivateServiceClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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
