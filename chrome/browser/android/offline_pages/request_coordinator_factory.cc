// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/android/offline_pages/prerendering_offliner_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_coordinator.h"

namespace offline_pages {

RequestCoordinatorFactory::RequestCoordinatorFactory()
    : BrowserContextKeyedServiceFactory(
          "OfflineRequestCoordinator",
          BrowserContextDependencyManager::GetInstance()) {}

// static
RequestCoordinatorFactory* RequestCoordinatorFactory::GetInstance() {
  return base::Singleton<RequestCoordinatorFactory>::get();
}

// static
RequestCoordinator* RequestCoordinatorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<RequestCoordinator*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

KeyedService* RequestCoordinatorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  std::unique_ptr<OfflinerPolicy> policy(new OfflinerPolicy());
  std::unique_ptr<OfflinerFactory> prerendererOffliner(
      new PrerenderingOfflinerFactory(context));
  // TODO(petewil) Add support for server based offliner when it is ready.

  return new RequestCoordinator(std::move(policy),
                                std::move(prerendererOffliner));
}

content::BrowserContext* RequestCoordinatorFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // TODO(petewil): Make sure we support incognito properly.
  return context;
}

}  // namespace offline_pages
