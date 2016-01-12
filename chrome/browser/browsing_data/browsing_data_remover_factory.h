// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_FACTORY_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

class BrowsingDataRemover;

class BrowsingDataRemoverFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the singleton instance of BrowsingDataRemoverFactory.
  static BrowsingDataRemoverFactory* GetInstance();

  // Returns the BrowsingDataRemover associated with |context|.
  static BrowsingDataRemover* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<BrowsingDataRemoverFactory>;

  BrowsingDataRemoverFactory();
  ~BrowsingDataRemoverFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverFactory);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_FACTORY_H_
