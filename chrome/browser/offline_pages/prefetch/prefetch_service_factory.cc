// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/offline_pages/prefetch/offline_metrics_collector_impl.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_instance_id_proxy.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_app_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#include "content/public/browser/browser_context.h"

namespace offline_pages {

PrefetchServiceFactory::PrefetchServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "OfflinePagePrefetchService",
          BrowserContextDependencyManager::GetInstance()) {}
// static
PrefetchServiceFactory* PrefetchServiceFactory::GetInstance() {
  return base::Singleton<PrefetchServiceFactory>::get();
}

// static
PrefetchService* PrefetchServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PrefetchService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

KeyedService* PrefetchServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto prefetch_dispatcher = base::MakeUnique<PrefetchDispatcherImpl>();
  auto prefetch_gcm_app_handler = base::MakeUnique<PrefetchGCMAppHandler>(
      base::MakeUnique<PrefetchInstanceIDProxy>(kPrefetchingOfflinePagesAppId,
                                                context));
  auto offline_metrics_collector =
      base::MakeUnique<OfflineMetricsCollectorImpl>();
  auto suggested_articles_observer =
      base::MakeUnique<SuggestedArticlesObserver>();

  return new PrefetchServiceImpl(std::move(offline_metrics_collector),
                                 std::move(prefetch_dispatcher),
                                 std::move(prefetch_gcm_app_handler),
                                 std::move(suggested_articles_observer));
}

}  // namespace offline_pages
