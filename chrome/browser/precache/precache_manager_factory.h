// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRECACHE_PRECACHE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_PRECACHE_PRECACHE_MANAGER_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace precache {

class PrecacheManager;

class PrecacheManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PrecacheManager* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static PrecacheManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PrecacheManagerFactory>;

  PrecacheManagerFactory();
  ~PrecacheManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;

  DISALLOW_COPY_AND_ASSIGN(PrecacheManagerFactory);
};

}  // namespace precache

#endif  // CHROME_BROWSER_PRECACHE_PRECACHE_MANAGER_FACTORY_H_
