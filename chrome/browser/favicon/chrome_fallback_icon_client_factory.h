// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_CHROME_FALLBACK_ICON_CLIENT_FACTORY_H_
#define CHROME_BROWSER_FAVICON_CHROME_FALLBACK_ICON_CLIENT_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace favicon {
class FallbackIconClient;
}

// Singleton that owns all ChromeFallbackIconClients and associates them with
// Profiles.
class ChromeFallbackIconClientFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of FallbackIconClient associated with this profile
  // (creating one if none exists).
  static favicon::FallbackIconClient* GetForBrowserContext(
      content::BrowserContext* context);

  // Returns an instance of the factory singleton.
  static ChromeFallbackIconClientFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ChromeFallbackIconClientFactory>;

  ChromeFallbackIconClientFactory();
  ~ChromeFallbackIconClientFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_FAVICON_CHROME_FALLBACK_ICON_CLIENT_FACTORY_H_
