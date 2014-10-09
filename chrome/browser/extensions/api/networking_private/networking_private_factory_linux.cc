// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_factory_linux.h"

#include "chrome/browser/extensions/api/networking_private/networking_private_delegate.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_linux.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

using content::BrowserContext;

// static
NetworkingPrivateDelegate* NetworkingPrivateDelegate::GetForBrowserContext(
    BrowserContext* browser_context) {
  return NetworkingPrivateLinuxFactory::GetForBrowserContext(browser_context);
}

// static
NetworkingPrivateLinux* NetworkingPrivateLinuxFactory::GetForBrowserContext(
    BrowserContext* browser_context) {
  return static_cast<NetworkingPrivateLinux*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 true /* create */));
}

// static
NetworkingPrivateLinuxFactory* NetworkingPrivateLinuxFactory::GetInstance() {
  return Singleton<NetworkingPrivateLinuxFactory>::get();
}

NetworkingPrivateLinuxFactory::NetworkingPrivateLinuxFactory()
    : BrowserContextKeyedServiceFactory(
          "NetworkingPrivateLinux",
          BrowserContextDependencyManager::GetInstance()) {
}

NetworkingPrivateLinuxFactory::~NetworkingPrivateLinuxFactory() {
}

KeyedService* NetworkingPrivateLinuxFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return new NetworkingPrivateLinux(browser_context);
}

bool NetworkingPrivateLinuxFactory::ServiceIsCreatedWithBrowserContext() const {
  return false;
}

bool NetworkingPrivateLinuxFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

}  // namespace extensions
