// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CONTENT_PRECACHE_MANAGER_FACTORY_H_
#define COMPONENTS_PRECACHE_CONTENT_PRECACHE_MANAGER_FACTORY_H_

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
  friend struct DefaultSingletonTraits<PrecacheManagerFactory>;

  PrecacheManagerFactory();
  virtual ~PrecacheManagerFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(PrecacheManagerFactory);
};

}  // namespace precache

#endif  // COMPONENTS_PRECACHE_CONTENT_PRECACHE_MANAGER_FACTORY_H_
