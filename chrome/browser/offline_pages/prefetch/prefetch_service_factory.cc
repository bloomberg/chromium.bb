// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/offline_pages/prefetch/offline_metrics_collector_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_app_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_in_memory_store.h"
#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"
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
  auto dispatcher_impl = base::MakeUnique<PrefetchDispatcherImpl>();
  auto offline_metrics_collector =
      base::MakeUnique<OfflineMetricsCollectorImpl>();
  auto gcm_handler = base::MakeUnique<PrefetchGCMAppHandler>();
  auto in_memory_store = base::MakeUnique<PrefetchInMemoryStore>();

  return new PrefetchServiceImpl(
      std::move(offline_metrics_collector), std::move(dispatcher_impl),
      std::move(gcm_handler), std::move(in_memory_store));
}

}  // namespace offline_pages
