// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_factory_chromeos.h"

#include "chrome/browser/extensions/api/networking_private/networking_private_chromeos.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_delegate.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

// static
NetworkingPrivateDelegate* NetworkingPrivateDelegate::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return NetworkingPrivateChromeOSFactory::GetForBrowserContext(
      browser_context);
}

// static
NetworkingPrivateChromeOS*
NetworkingPrivateChromeOSFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<NetworkingPrivateChromeOS*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
NetworkingPrivateChromeOSFactory*
NetworkingPrivateChromeOSFactory::GetInstance() {
  return Singleton<NetworkingPrivateChromeOSFactory>::get();
}

NetworkingPrivateChromeOSFactory::NetworkingPrivateChromeOSFactory()
    : BrowserContextKeyedServiceFactory(
          "NetworkingPrivateChromeOS",
          BrowserContextDependencyManager::GetInstance()) {
}

NetworkingPrivateChromeOSFactory::~NetworkingPrivateChromeOSFactory() {
}

KeyedService* NetworkingPrivateChromeOSFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return new NetworkingPrivateChromeOS(browser_context);
}

bool NetworkingPrivateChromeOSFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return false;
}

bool NetworkingPrivateChromeOSFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace extensions
