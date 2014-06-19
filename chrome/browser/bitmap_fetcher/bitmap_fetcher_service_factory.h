// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class BitmapFetcherService;

class BitmapFetcherServiceFactory : BrowserContextKeyedServiceFactory {
 public:
  // TODO(groby): Maybe make this GetForProfile?
  static BitmapFetcherService* GetForBrowserContext(
      content::BrowserContext* context);
  static BitmapFetcherServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<BitmapFetcherServiceFactory>;

  BitmapFetcherServiceFactory();
  virtual ~BitmapFetcherServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(BitmapFetcherServiceFactory);
};

#endif  // CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_SERVICE_FACTORY_H_
