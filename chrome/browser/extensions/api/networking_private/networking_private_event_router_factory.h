// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_EVENT_ROUTER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class NetworkingPrivateEventRouter;

// This is a factory class used by the BrowserContextDependencyManager
// to instantiate the networking event router per profile (since the extension
// event router is per profile).
class NetworkingPrivateEventRouterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the NetworkingPrivateEventRouter for |profile|, creating it if
  // it is not yet created.
  static NetworkingPrivateEventRouter* GetForProfile(
      content::BrowserContext* context);

  // Returns the NetworkingPrivateEventRouterFactory instance.
  static NetworkingPrivateEventRouterFactory* GetInstance();

 protected:
  // BrowserContextKeyedBaseFactory overrides:
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<NetworkingPrivateEventRouterFactory>;

  NetworkingPrivateEventRouterFactory();
  virtual ~NetworkingPrivateEventRouterFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateEventRouterFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_EVENT_ROUTER_FACTORY_H_
