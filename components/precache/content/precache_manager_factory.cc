// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/content/precache_manager_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/precache/content/precache_manager.h"
#include "content/public/browser/browser_context.h"

namespace precache {

// static
PrecacheManager* PrecacheManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<PrecacheManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
PrecacheManagerFactory* PrecacheManagerFactory::GetInstance() {
  return Singleton<PrecacheManagerFactory>::get();
}

PrecacheManagerFactory::PrecacheManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PrecacheManager", BrowserContextDependencyManager::GetInstance()) {}

PrecacheManagerFactory::~PrecacheManagerFactory() {}

KeyedService* PrecacheManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new PrecacheManager(browser_context);
}

}  // namespace precache
