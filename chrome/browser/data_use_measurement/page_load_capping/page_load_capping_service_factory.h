// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_CAPPING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_CAPPING_SERVICE_FACTORY_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class PageLoadCappingService;

// LazyInstance that owns all PageLoadCappingServices and associates them with
// Profiles.
class PageLoadCappingServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Gets the PageLoadCappingService instance for |context|.
  static PageLoadCappingService* GetForBrowserContext(
      content::BrowserContext* context);

  // Gets the LazyInstance that owns all PreviewsServices.
  static PageLoadCappingServiceFactory* GetInstance();

 private:
  friend struct base::LazyInstanceTraitsBase<PageLoadCappingServiceFactory>;

  PageLoadCappingServiceFactory();
  ~PageLoadCappingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PageLoadCappingServiceFactory);
};

#endif  // CHROME_BROWSER_DATA_USE_MEASUREMENT_PAGE_LOAD_CAPPING_PAGE_LOAD_CAPPING_SERVICE_FACTORY_H_
