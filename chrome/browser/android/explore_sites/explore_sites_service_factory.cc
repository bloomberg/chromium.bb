// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_service_factory.h"

#include <memory>
#include <utility>

#include "base/memory/singleton.h"

#include "chrome/browser/android/explore_sites/explore_sites_service.h"
#include "chrome/browser/android/explore_sites/explore_sites_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace explore_sites {

ExploreSitesServiceFactory::ExploreSitesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ExploreSitesService",
          BrowserContextDependencyManager::GetInstance()) {}

// static
ExploreSitesServiceFactory* ExploreSitesServiceFactory::GetInstance() {
  return base::Singleton<ExploreSitesServiceFactory>::get();
}

// static
ExploreSitesService* ExploreSitesServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExploreSitesService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

KeyedService* ExploreSitesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ExploreSitesServiceImpl();
}

}  // namespace explore_sites
