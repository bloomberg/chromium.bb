// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/prefetch/offline_metrics_collector_impl.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_background_task_handler_impl.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_instance_id_proxy.h"
#include "chrome/browser/offline_pages/prefetch/thumbnail_fetcher_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "components/feed/feed_feature_list.h"
#include "components/image_fetcher/core/cached_image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_app_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_importer_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory_impl.h"
#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace offline_pages {

namespace {

void OnProfileCreated(PrefetchServiceImpl* service, Profile* profile) {
  auto gcm_app_handler = std::make_unique<PrefetchGCMAppHandler>(
      std::make_unique<PrefetchInstanceIDProxy>(kPrefetchingOfflinePagesAppId,
                                                profile));
  service->SetPrefetchGCMHandler(std::move(gcm_app_handler));
  if (IsPrefetchingOfflinePagesEnabled()) {
    // Trigger an update of the cached GCM token. This needs to be post tasked
    // because otherwise leads to circular dependency between
    // PrefetchServiceFactory and GCMProfileServiceFactory. See
    // https://crbug.com/944952
    // Update is not a priority so make sure it happens after the critical
    // startup path.
    content::BrowserThread::PostAfterStartupTask(
        FROM_HERE, base::SequencedTaskRunnerHandle::Get(),
        base::BindOnce(&PrefetchServiceImpl::GetGCMToken, service->GetWeakPtr(),
                       base::DoNothing::Once<const std::string&>()));
  }
}

}  // namespace

PrefetchServiceFactory::PrefetchServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "OfflinePagePrefetchService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DownloadServiceFactory::GetInstance());
  DependsOn(OfflinePageModelFactory::GetInstance());
  DependsOn(ImageFetcherServiceFactory::GetInstance());
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
  const bool feed_enabled =
      base::FeatureList::IsEnabled(feed::kInterestFeedContentSuggestions);
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(context);
  DCHECK(offline_page_model);

  auto offline_metrics_collector =
      std::make_unique<OfflineMetricsCollectorImpl>(profile->GetPrefs());

  auto prefetch_dispatcher =
      std::make_unique<PrefetchDispatcherImpl>(profile->GetPrefs());

  auto prefetch_network_request_factory =
      std::make_unique<PrefetchNetworkRequestFactoryImpl>(
          profile->GetURLLoaderFactory(), chrome::GetChannel(), GetUserAgent(),
          profile->GetPrefs());

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()});
  base::FilePath store_path =
      profile->GetPath().Append(chrome::kOfflinePagePrefetchStoreDirname);
  auto prefetch_store =
      std::make_unique<PrefetchStore>(background_task_runner, store_path);

  // Zine/Feed
  // Conditional components for Zine. Not created when using Feed.
  std::unique_ptr<SuggestedArticlesObserver> suggested_articles_observer;
  std::unique_ptr<ThumbnailFetcherImpl> thumbnail_fetcher;
  // Conditional components for Feed. Not created when using Zine.
  image_fetcher::ImageFetcher* image_fetcher = nullptr;
  if (!feed_enabled) {
    suggested_articles_observer = std::make_unique<SuggestedArticlesObserver>();
    thumbnail_fetcher = std::make_unique<ThumbnailFetcherImpl>();
  } else {
    SimpleFactoryKey* simple_factory_key = profile->GetProfileKey();
    image_fetcher::ImageFetcherService* image_fetcher_service =
        ImageFetcherServiceFactory::GetForKey(simple_factory_key);
    DCHECK(image_fetcher_service);
    image_fetcher = image_fetcher_service->GetImageFetcher(
        image_fetcher::ImageFetcherConfig::kDiskCacheOnly);
  }

  auto prefetch_downloader = std::make_unique<PrefetchDownloaderImpl>(
      DownloadServiceFactory::GetForBrowserContext(context),
      chrome::GetChannel(), profile->GetPrefs());

  auto prefetch_importer = std::make_unique<PrefetchImporterImpl>(
      prefetch_dispatcher.get(), offline_page_model, background_task_runner);

  auto prefetch_background_task_handler =
      std::make_unique<PrefetchBackgroundTaskHandlerImpl>(profile->GetPrefs());

  auto* service = new PrefetchServiceImpl(
      std::move(offline_metrics_collector), std::move(prefetch_dispatcher),
      std::move(prefetch_network_request_factory), offline_page_model,
      std::move(prefetch_store), std::move(suggested_articles_observer),
      std::move(prefetch_downloader), std::move(prefetch_importer),
      std::move(prefetch_background_task_handler), std::move(thumbnail_fetcher),
      image_fetcher);

  auto callback = base::BindOnce(&OnProfileCreated, service);
  FullBrowserTransitionManager::Get()->RegisterCallbackOnProfileCreation(
      profile->GetProfileKey(), std::move(callback));

  return service;
}

}  // namespace offline_pages
