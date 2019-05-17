// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_gcm_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_importer.h"
#include "components/offline_pages/core/prefetch/prefetch_network_request_factory.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/suggested_articles_observer.h"
#include "components/offline_pages/core/prefetch/suggestions_provider.h"
#include "components/offline_pages/core/prefetch/thumbnail_fetcher.h"

namespace offline_pages {

PrefetchServiceImpl::PrefetchServiceImpl(
    std::unique_ptr<OfflineMetricsCollector> offline_metrics_collector,
    std::unique_ptr<PrefetchDispatcher> dispatcher,
    std::unique_ptr<PrefetchNetworkRequestFactory> network_request_factory,
    OfflinePageModel* offline_page_model,
    std::unique_ptr<PrefetchStore> prefetch_store,
    std::unique_ptr<SuggestedArticlesObserver> suggested_articles_observer,
    std::unique_ptr<PrefetchDownloader> prefetch_downloader,
    std::unique_ptr<PrefetchImporter> prefetch_importer,
    std::unique_ptr<PrefetchBackgroundTaskHandler>
        prefetch_background_task_handler,
    std::unique_ptr<ThumbnailFetcher> thumbnail_fetcher,
    image_fetcher::ImageFetcher* image_fetcher)
    : offline_metrics_collector_(std::move(offline_metrics_collector)),
      prefetch_dispatcher_(std::move(dispatcher)),
      network_request_factory_(std::move(network_request_factory)),
      offline_page_model_(offline_page_model),
      prefetch_store_(std::move(prefetch_store)),
      prefetch_downloader_(std::move(prefetch_downloader)),
      prefetch_importer_(std::move(prefetch_importer)),
      prefetch_background_task_handler_(
          std::move(prefetch_background_task_handler)),
      suggested_articles_observer_(std::move(suggested_articles_observer)),
      thumbnail_fetcher_(std::move(thumbnail_fetcher)),
      image_fetcher_(image_fetcher),
      weak_ptr_factory_(this) {
  prefetch_dispatcher_->SetService(this);
  prefetch_downloader_->SetPrefetchService(this);
  if (suggested_articles_observer_)
    suggested_articles_observer_->SetPrefetchService(this);
}

PrefetchServiceImpl::~PrefetchServiceImpl() {
  // The dispatcher needs to be disposed first because it may need to
  // communicate with other members owned by the service at destruction time.
  prefetch_dispatcher_.reset();
}

void PrefetchServiceImpl::ForceRefreshSuggestions() {
  if (suggestions_provider_) {
    // Feed only.
    NewSuggestionsAvailable();
  } else {
    // Zine only.
    DCHECK(suggested_articles_observer_);
    suggested_articles_observer_->ConsumeSuggestions();
  }
}

void PrefetchServiceImpl::SetCachedGCMToken(const std::string& gcm_token) {
  // This method is passed a cached token that was stored in the job scheduler,
  // to be used until the PrefetchGCMHandler is created. In some cases, the
  // PrefetchGCMHandler could have been already created and a fresher token
  // requested before this function is called. Make sure to not override a
  // fresher token with a stale one.
  if (gcm_token_.empty())
    gcm_token_ = gcm_token;
}

const std::string& PrefetchServiceImpl::GetCachedGCMToken() const {
  DCHECK(!gcm_token_.empty()) << "No cached token is set, you should call "
                                 "PrefetchService::GetGCMToken instead";
  return gcm_token_;
}

void PrefetchServiceImpl::GetGCMToken(GCMTokenCallback callback) {
  DCHECK(prefetch_gcm_handler_);
  prefetch_gcm_handler_->GetGCMToken(base::AdaptCallbackForRepeating(
      base::BindOnce(&PrefetchServiceImpl::OnGCMTokenReceived, GetWeakPtr(),
                     std::move(callback))));
}

void PrefetchServiceImpl::OnGCMTokenReceived(
    GCMTokenCallback callback,
    const std::string& gcm_token,
    instance_id::InstanceID::Result result) {
  // TODO(dimich): Add UMA reporting on instance_id::InstanceID::Result.
  // Keep the cached token fresh
  gcm_token_ = gcm_token;
  std::move(callback).Run(gcm_token);
}

void PrefetchServiceImpl::SetContentSuggestionsService(
    ntp_snippets::ContentSuggestionsService* content_suggestions) {
  if (!suggested_articles_observer_) {
    // TODO(https://crbug.com/892265): When the Feed is enabled, we currently
    // need to ignore Zine. Eventually, Zine will be disabled when using Feed,
    // so this check will be unnecessary.
    return;
  }
  DCHECK(suggested_articles_observer_);
  DCHECK(!suggestions_provider_);
  DCHECK(thumbnail_fetcher_);
  DCHECK(!image_fetcher_);
  suggested_articles_observer_->SetContentSuggestionsServiceAndObserve(
      content_suggestions);
  thumbnail_fetcher_->SetContentSuggestionsService(content_suggestions);
  content_suggestions_ = content_suggestions;
}

void PrefetchServiceImpl::SetSuggestionProvider(
    SuggestionsProvider* suggestions_provider) {
  DCHECK(!suggested_articles_observer_);
  DCHECK(!thumbnail_fetcher_);
  DCHECK(image_fetcher_);
  suggestions_provider_ = suggestions_provider;
}

void PrefetchServiceImpl::SetEnabledByServer(PrefService* pref_service,
                                             bool enabled) {
  if (enabled == prefetch_prefs::IsEnabledByServer(pref_service))
    return;

  prefetch_prefs::SetEnabledByServer(pref_service, enabled);
  if (enabled)
    ForceRefreshSuggestions();
}

void PrefetchServiceImpl::NewSuggestionsAvailable() {
  DCHECK(suggestions_provider_);
  prefetch_dispatcher_->NewSuggestionsAvailable(suggestions_provider_);
}

void PrefetchServiceImpl::RemoveSuggestion(GURL url) {
  DCHECK(suggestions_provider_);
  prefetch_dispatcher_->RemoveSuggestion(std::move(url));
}

OfflineMetricsCollector* PrefetchServiceImpl::GetOfflineMetricsCollector() {
  return offline_metrics_collector_.get();
}

PrefetchDispatcher* PrefetchServiceImpl::GetPrefetchDispatcher() {
  return prefetch_dispatcher_.get();
}

PrefetchGCMHandler* PrefetchServiceImpl::GetPrefetchGCMHandler() {
  DCHECK(prefetch_gcm_handler_);
  return prefetch_gcm_handler_.get();
}

void PrefetchServiceImpl::SetPrefetchGCMHandler(
    std::unique_ptr<PrefetchGCMHandler> handler) {
  DCHECK(!prefetch_gcm_handler_);
  prefetch_gcm_handler_ = std::move(handler);
  prefetch_gcm_handler_->SetService(this);
}

PrefetchNetworkRequestFactory*
PrefetchServiceImpl::GetPrefetchNetworkRequestFactory() {
  return network_request_factory_.get();
}

OfflinePageModel* PrefetchServiceImpl::GetOfflinePageModel() {
  return offline_page_model_;
}

PrefetchStore* PrefetchServiceImpl::GetPrefetchStore() {
  return prefetch_store_.get();
}

SuggestedArticlesObserver*
PrefetchServiceImpl::GetSuggestedArticlesObserverForTesting() {
  return suggested_articles_observer_.get();
}

OfflineEventLogger* PrefetchServiceImpl::GetLogger() {
  return &logger_;
}

PrefetchDownloader* PrefetchServiceImpl::GetPrefetchDownloader() {
  return prefetch_downloader_.get();
}

PrefetchImporter* PrefetchServiceImpl::GetPrefetchImporter() {
  return prefetch_importer_.get();
}

PrefetchBackgroundTaskHandler*
PrefetchServiceImpl::GetPrefetchBackgroundTaskHandler() {
  return prefetch_background_task_handler_.get();
}

ThumbnailFetcher* PrefetchServiceImpl::GetThumbnailFetcher() {
  return thumbnail_fetcher_.get();
}

image_fetcher::ImageFetcher* PrefetchServiceImpl::GetImageFetcher() {
  return image_fetcher_;
}

base::WeakPtr<PrefetchServiceImpl> PrefetchServiceImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PrefetchServiceImpl::Shutdown() {
  prefetch_gcm_handler_.reset();
  suggested_articles_observer_.reset();
  prefetch_downloader_.reset();
}

}  // namespace offline_pages
