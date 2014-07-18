// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_SERVICE_CLIENT_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_SERVICE_CLIENT_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace context {
class BrowserContext;
}

namespace extensions {

class NetworkingPrivateServiceClient;

class NetworkingPrivateServiceClientFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static NetworkingPrivateServiceClient* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static NetworkingPrivateServiceClientFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<NetworkingPrivateServiceClientFactory>;

  NetworkingPrivateServiceClientFactory();
  virtual ~NetworkingPrivateServiceClientFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateServiceClientFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_SERVICE_CLIENT_FACTORY_H_
