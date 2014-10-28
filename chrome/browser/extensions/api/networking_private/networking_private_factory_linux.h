// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_FACTORY_LINUX_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_FACTORY_LINUX_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace context {
class BrowserContext;
}

namespace extensions {

class NetworkingPrivateLinux;

class NetworkingPrivateLinuxFactory : public BrowserContextKeyedServiceFactory {
 public:
  static NetworkingPrivateLinux* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static NetworkingPrivateLinuxFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<NetworkingPrivateLinuxFactory>;

  NetworkingPrivateLinuxFactory();
  ~NetworkingPrivateLinuxFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateLinuxFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_FACTORY_LINUX_H_
