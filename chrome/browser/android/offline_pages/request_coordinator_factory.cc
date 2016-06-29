// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/android/offline_pages/background_scheduler_bridge.h"
#include "chrome/browser/android/offline_pages/prerendering_offliner_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/background/offliner_factory.h"
#include "components/offline_pages/background/offliner_policy.h"
#include "components/offline_pages/background/request_coordinator.h"
#include "components/offline_pages/background/request_queue.h"
#include "components/offline_pages/background/request_queue_store.h"
#include "components/offline_pages/background/request_queue_store_sql.h"
#include "components/offline_pages/background/scheduler.h"
#include "content/public/browser/browser_thread.h"

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

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      content::BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
          content::BrowserThread::GetBlockingPool()->GetSequenceToken());
  base::FilePath queue_store_path =
      Profile::FromBrowserContext(context)->GetPath().Append(
          chrome::kOfflinePageRequestQueueDirname);

  std::unique_ptr<RequestQueueStoreSQL> queue_store(
      new RequestQueueStoreSQL(background_task_runner, queue_store_path));
  std::unique_ptr<RequestQueue> queue(new RequestQueue(std::move(queue_store)));
  std::unique_ptr<Scheduler>
      scheduler(new android::BackgroundSchedulerBridge());

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
