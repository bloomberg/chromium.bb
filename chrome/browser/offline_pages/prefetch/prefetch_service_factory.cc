// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/offline_pages/prefetch/offline_metrics_collector_impl.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_background_task_handler_impl.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_configuration_impl.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_importer_impl.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_instance_id_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_app_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#include "content/public/browser/browser_context.h"

namespace offline_pages {

PrefetchServiceFactory::PrefetchServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "OfflinePagePrefetchService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DownloadServiceFactory::GetInstance());
}

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
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);

  auto offline_metrics_collector =
      std::make_unique<OfflineMetricsCollectorImpl>(profile->GetPrefs());

  auto prefetch_dispatcher = std::make_unique<PrefetchDispatcherImpl>();

  auto prefetch_gcm_app_handler = std::make_unique<PrefetchGCMAppHandler>(
      std::make_unique<PrefetchInstanceIDProxy>(kPrefetchingOfflinePagesAppId,
                                                context));

  auto prefetch_network_request_factory =
      std::make_unique<PrefetchNetworkRequestFactoryImpl>(
          profile->GetRequestContext(), chrome::GetChannel(), GetUserAgent());

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()});
  base::FilePath store_path =
      profile->GetPath().Append(chrome::kOfflinePagePrefetchStoreDirname);
  auto prefetch_store =
      std::make_unique<PrefetchStore>(background_task_runner, store_path);

  auto suggested_articles_observer =
      std::make_unique<SuggestedArticlesObserver>();

  auto prefetch_downloader = std::make_unique<PrefetchDownloaderImpl>(
      DownloadServiceFactory::GetForBrowserContext(context),
      chrome::GetChannel());

  auto prefetch_importer = std::make_unique<PrefetchImporterImpl>(
      prefetch_dispatcher.get(), context, background_task_runner);

  auto prefetch_background_task_handler =
      std::make_unique<PrefetchBackgroundTaskHandlerImpl>(profile->GetPrefs());

  auto configuration =
      std::make_unique<PrefetchConfigurationImpl>(profile->GetPrefs());

  return new PrefetchServiceImpl(
      std::move(offline_metrics_collector), std::move(prefetch_dispatcher),
      std::move(prefetch_gcm_app_handler),
      std::move(prefetch_network_request_factory), std::move(prefetch_store),
      std::move(suggested_articles_observer), std::move(prefetch_downloader),
      std::move(prefetch_importer), std::move(prefetch_background_task_handler),
      std::move(configuration));
}

}  // namespace offline_pages
