// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_FACTORY_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_FACTORY_CHROMEOS_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace context {
class BrowserContext;
}

namespace extensions {

class NetworkingPrivateChromeOS;

class NetworkingPrivateChromeOSFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static NetworkingPrivateChromeOS* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static NetworkingPrivateChromeOSFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<NetworkingPrivateChromeOSFactory>;

  NetworkingPrivateChromeOSFactory();
  virtual ~NetworkingPrivateChromeOSFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateChromeOSFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_FACTORY_CHROMEOS_H_
