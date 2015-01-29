// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_FACTORY_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

class Profile;

namespace history {
class TopSites;
}

template <typename T>
struct DefaultSingletonTraits;

namespace history {
class TopSites;
}

class Profile;

// Used for creating and fetching a per-profile instance of the
// TopSites.
class TopSitesFactory : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  // Get the TopSites service for |profile|, creating one if needed.
  static scoped_refptr<history::TopSites> GetForProfile(Profile* profile);

  // Get the TopSites service for |profile|, but do not create it if it doesn't
  // exist.
  static scoped_refptr<history::TopSites> GetForProfileIfExists(
      Profile* profile);

  // Get the singleton instance of the factory.
  static TopSitesFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<TopSitesFactory>;

  TopSitesFactory();
  ~TopSitesFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory.
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(TopSitesFactory);
};

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_FACTORY_H_
