// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NETWORK_TIME_NETWORK_TIME_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NETWORK_TIME_NETWORK_TIME_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class BrowserContextKeyedService;
class NetworkTimeService;

// Singleton that owns all NetworkTimeService and associates them with
// Profiles.
class NetworkTimeServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static NetworkTimeServiceFactory* GetInstance();
  static NetworkTimeService* GetForProfile(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<NetworkTimeServiceFactory>;

  NetworkTimeServiceFactory();
  virtual ~NetworkTimeServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NetworkTimeServiceFactory);
};

#endif  // CHROME_BROWSER_NETWORK_TIME_NETWORK_TIME_SERVICE_FACTORY_H_
