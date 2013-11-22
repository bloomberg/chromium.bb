// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_SERVICE_CLIENT_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_SERVICE_CLIENT_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class Profile;

namespace extensions {

class NetworkingPrivateServiceClient;

class NetworkingPrivateServiceClientFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static NetworkingPrivateServiceClient* GetForProfile(Profile* profile);

  static NetworkingPrivateServiceClientFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<NetworkingPrivateServiceClientFactory>;

  NetworkingPrivateServiceClientFactory();
  virtual ~NetworkingPrivateServiceClientFactory();

  // BrowserContextKeyedServiceFactory:
  virtual BrowserContextKeyedService*
      BuildServiceInstanceFor(content::BrowserContext* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateServiceClientFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_SERVICE_CLIENT_FACTORY_H_
