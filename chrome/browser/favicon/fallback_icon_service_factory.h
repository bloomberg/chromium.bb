// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FALLBACK_ICON_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FAVICON_FALLBACK_ICON_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace favicon {
class FallbackIconService;
}

// Singleton that owns all FallbackIconService and associates them with
// BrowserContext instances.
class FallbackIconServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static favicon::FallbackIconService* GetForBrowserContext(
      content::BrowserContext* context);

  static FallbackIconServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<FallbackIconServiceFactory>;

  FallbackIconServiceFactory();
  ~FallbackIconServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(FallbackIconServiceFactory);
};

#endif  // CHROME_BROWSER_FAVICON_FALLBACK_ICON_SERVICE_FACTORY_H_
