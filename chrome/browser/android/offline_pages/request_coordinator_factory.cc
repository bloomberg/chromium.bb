// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/android/offline_pages/background_scheduler_bridge.h"
#include "chrome/browser/android/offline_pages/downloads/offline_page_notification_bridge.h"
#include "chrome/browser/android/offline_pages/prerendering_offliner_factory.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_factory.h"
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
#include "components/offline_pages/downloads/download_notifying_observer.h"
#include "content/public/browser/browser_thread.h"
#include "net/nqe/network_quality_estimator.h"

#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/android/offline_pages/evaluation/evaluation_test_scheduler.h"
#endif

namespace offline_pages {

namespace {
const bool kPreferUntriedRequest = false;
const bool kPreferEarlierRequest = true;
const bool kPreferRetryCountOverRecency = false;
const int kMaxStartedTries = 4;
const int kMaxCompletedTries = 1;
const int kImmediateRequestExpirationTimeInSeconds = 3600;
}

RequestCoordinatorFactory::RequestCoordinatorFactory()
    : BrowserContextKeyedServiceFactory(
          "OfflineRequestCoordinator",
          BrowserContextDependencyManager::GetInstance()) {}

#if !defined(OFFICIAL_BUILD)
// static
std::unique_ptr<KeyedService> RequestCoordinatorFactory::GetTestingFactory(
    content::BrowserContext* context) {
  // Create a new OfflinerPolicy with a larger background processing
  // budget (3600 sec). Other values are the same with default ones.
  std::unique_ptr<OfflinerPolicy> policy(new OfflinerPolicy(
      kPreferUntriedRequest, kPreferEarlierRequest,
      kPreferRetryCountOverRecency, kMaxStartedTries, kMaxCompletedTries,
      kImmediateRequestExpirationTimeInSeconds));
  std::unique_ptr<OfflinerFactory> prerenderer_offliner(
      new PrerenderingOfflinerFactory(context));

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      content::BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
          content::BrowserThread::GetBlockingPool()->GetSequenceToken());
  Profile* profile = Profile::FromBrowserContext(context);
  base::FilePath queue_store_path =
      profile->GetPath().Append(chrome::kOfflinePageRequestQueueDirname);

  std::unique_ptr<RequestQueueStoreSQL> queue_store(
      new RequestQueueStoreSQL(background_task_runner, queue_store_path));
  std::unique_ptr<RequestQueue> queue(new RequestQueue(std::move(queue_store)));
  std::unique_ptr<android::EvaluationTestScheduler> scheduler(
      new android::EvaluationTestScheduler());
  net::NetworkQualityEstimator::NetworkQualityProvider*
      network_quality_estimator =
          UINetworkQualityEstimatorServiceFactory::GetForProfile(profile);
  // TODO(fgorski): Something needs to keep the handle to the Notification
  // dispatcher.
  std::unique_ptr<RequestCoordinator> request_coordinator =
      base::MakeUnique<RequestCoordinator>(
          std::move(policy), std::move(prerenderer_offliner), std::move(queue),
          std::move(scheduler), network_quality_estimator);
  request_coordinator->SetImmediateScheduleCallbackForTest(
      base::Bind(&android::EvaluationTestScheduler::ImmediateScheduleCallback,
                 base::Unretained(scheduler.get())));

  DownloadNotifyingObserver::CreateAndStartObserving(
      request_coordinator.get(),
      base::MakeUnique<android::OfflinePageNotificationBridge>());

  return std::move(request_coordinator);
}
#endif

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
  Profile* profile = Profile::FromBrowserContext(context);
  base::FilePath queue_store_path =
      profile->GetPath().Append(chrome::kOfflinePageRequestQueueDirname);

  std::unique_ptr<RequestQueueStoreSQL> queue_store(
      new RequestQueueStoreSQL(background_task_runner, queue_store_path));
  std::unique_ptr<RequestQueue> queue(new RequestQueue(std::move(queue_store)));
  std::unique_ptr<Scheduler>
      scheduler(new android::BackgroundSchedulerBridge());
  net::NetworkQualityEstimator::NetworkQualityProvider*
      network_quality_estimator =
          UINetworkQualityEstimatorServiceFactory::GetForProfile(profile);
  // TODO(fgorski): Something needs to keep the handle to the Notification
  // dispatcher.
  RequestCoordinator* request_coordinator = new RequestCoordinator(
      std::move(policy), std::move(prerenderer_offliner), std::move(queue),
      std::move(scheduler), network_quality_estimator);

  DownloadNotifyingObserver::CreateAndStartObserving(
      request_coordinator,
      base::MakeUnique<android::OfflinePageNotificationBridge>());

  return request_coordinator;
}

content::BrowserContext* RequestCoordinatorFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // TODO(petewil): Make sure we support incognito properly.
  return context;
}

}  // namespace offline_pages
