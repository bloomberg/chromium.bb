// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/android/offline_pages/background_scheduler_bridge.h"
#include "chrome/browser/android/offline_pages/prerendering_offliner_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_coordinator.h"
#include "components/offline_pages/background/request_queue.h"
#include "components/offline_pages/background/request_queue_in_memory_store.h"
#include "components/offline_pages/background/request_queue_store.h"
#include "components/offline_pages/background/scheduler.h"

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
  std::unique_ptr<OfflinerFactory> prerenderer_offliner(
      new PrerenderingOfflinerFactory(context));
  std::unique_ptr<RequestQueueInMemoryStore> store(
      new RequestQueueInMemoryStore());
  std::unique_ptr<RequestQueue> queue(new RequestQueue(std::move(store)));
  // TODO(petewil) Add support for server based offliner when it is ready.

  std::unique_ptr<Scheduler>
      scheduler(new android::BackgroundSchedulerBridge());
  // TODO(petewil) Add support for server based offliner when it is ready.

  return new RequestCoordinator(std::move(policy),
                                std::move(prerenderer_offliner),
                                std::move(queue),
                                std::move(scheduler));
}

content::BrowserContext* RequestCoordinatorFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // TODO(petewil): Make sure we support incognito properly.
  return context;
}

}  // namespace offline_pages
