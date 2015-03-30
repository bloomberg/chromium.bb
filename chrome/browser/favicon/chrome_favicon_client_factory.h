// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_CHROME_FAVICON_CLIENT_FACTORY_H_
#define CHROME_BROWSER_FAVICON_CHROME_FAVICON_CLIENT_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

template <typename T>
struct DefaultSingletonTraits;

class Profile;

namespace favicon {
class FaviconClient;
}

// Singleton that owns all ChromeFaviconClients and associates them with
// Profiles.
class ChromeFaviconClientFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of favicon::FaviconClient associated with |profile|
  // (creating one if none exists).
  static favicon::FaviconClient* GetForProfile(Profile* profile);

  // Returns an instance of the factory singleton.
  static ChromeFaviconClientFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ChromeFaviconClientFactory>;

  ChromeFaviconClientFactory();
  ~ChromeFaviconClientFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_FAVICON_CHROME_FAVICON_CLIENT_FACTORY_H_
