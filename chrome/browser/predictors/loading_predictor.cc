// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor.h"

#include <algorithm>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/loading_stats_collector.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"

namespace predictors {

namespace {

const base::TimeDelta kMinDelayBetweenPreresolveRequests =
    base::TimeDelta::FromSeconds(60);
const base::TimeDelta kMinDelayBetweenPreconnectRequests =
    base::TimeDelta::FromSeconds(10);

// Returns true iff |prediction| is not empty.
bool AddInitialUrlToPreconnectPrediction(const GURL& initial_url,
                                         PreconnectPrediction* prediction) {
  GURL initial_origin = initial_url.GetOrigin();
  // Open minimum 2 sockets to the main frame host to speed up the loading if a
  // main page has a redirect to the same host. This is because there can be a
  // race between reading the server redirect response and sending a new request
  // while the connection is still in use.
  static const int kMinSockets = 2;

  if (!prediction->requests.empty() &&
      prediction->requests.front().origin == initial_origin) {
    prediction->requests.front().num_sockets =
        std::max(prediction->requests.front().num_sockets, kMinSockets);
  } else if (initial_origin.is_valid() &&
             initial_origin.SchemeIsHTTPOrHTTPS()) {
    prediction->requests.emplace(prediction->requests.begin(), initial_origin,
                                 kMinSockets);
  }

  return !prediction->requests.empty();
}

}  // namespace

LoadingPredictor::LoadingPredictor(const LoadingPredictorConfig& config,
                                   Profile* profile)
    : config_(config),
      profile_(profile),
      resource_prefetch_predictor_(
          std::make_unique<ResourcePrefetchPredictor>(config, profile)),
      stats_collector_(std::make_unique<LoadingStatsCollector>(
          resource_prefetch_predictor_.get(),
          config)),
      loading_data_collector_(std::make_unique<LoadingDataCollector>(
          resource_prefetch_predictor_.get(),
          stats_collector_.get(),
          config)),
      observer_(nullptr),
      weak_factory_(this) {}

LoadingPredictor::~LoadingPredictor() {
  DCHECK(shutdown_);
}

void LoadingPredictor::PrepareForPageLoad(const GURL& url,
                                          HintOrigin origin,
                                          bool preconnectable) {
  if (shutdown_)
    return;

  if (origin == HintOrigin::OMNIBOX) {
    // Omnibox hints are lightweight and need a special treatment.
    HandleOmniboxHint(url, preconnectable);
    return;
  }

  if (active_hints_.find(url) != active_hints_.end())
    return;

  bool has_prefetch_prediction = false;
  bool has_preconnect_prediction = false;
  bool hint_activated = false;

  {
    ResourcePrefetchPredictor::Prediction prediction;
    has_prefetch_prediction =
        resource_prefetch_predictor_->GetPrefetchData(url, &prediction);
    if (has_prefetch_prediction &&
        config_.IsPrefetchingEnabledForOrigin(profile_, origin)) {
      MaybeAddPrefetch(url, prediction.subresource_urls, origin);
      hint_activated = true;
    }
  }

  if (!hint_activated) {
    PreconnectPrediction prediction;
    has_preconnect_prediction =
        resource_prefetch_predictor_->PredictPreconnectOrigins(url,
                                                               &prediction);
    // Try to preconnect to the |url| even if the predictor has no
    // prediction.
    has_preconnect_prediction =
        AddInitialUrlToPreconnectPrediction(url, &prediction);
    if (has_preconnect_prediction &&
        config_.IsPreconnectEnabledForOrigin(profile_, origin)) {
      MaybeAddPreconnect(url, std::move(prediction.requests), origin);
      hint_activated = true;
    }
  }

  if (has_prefetch_prediction || has_preconnect_prediction) {
    // To report hint durations and deduplicate hints to the same url.
    active_hints_.emplace(url, base::TimeTicks::Now());
  }
}

void LoadingPredictor::CancelPageLoadHint(const GURL& url) {
  if (shutdown_)
    return;

  CancelActiveHint(active_hints_.find(url));
}

void LoadingPredictor::StartInitialization() {
  if (shutdown_)
    return;

  resource_prefetch_predictor_->StartInitialization();
}

LoadingDataCollector* LoadingPredictor::loading_data_collector() {
  return loading_data_collector_.get();
}

ResourcePrefetchPredictor* LoadingPredictor::resource_prefetch_predictor() {
  return resource_prefetch_predictor_.get();
}

PreconnectManager* LoadingPredictor::preconnect_manager() {
  if (shutdown_)
    return nullptr;

  if (!preconnect_manager_ &&
      config_.IsPreconnectEnabledForSomeOrigin(profile_)) {
    preconnect_manager_ = std::make_unique<PreconnectManager>(
        GetWeakPtr(), profile_->GetRequestContext());
  }

  return preconnect_manager_.get();
}

void LoadingPredictor::Shutdown() {
  DCHECK(!shutdown_);
  resource_prefetch_predictor_->Shutdown();

  std::vector<std::unique_ptr<ResourcePrefetcher>> prefetchers;
  for (auto& kv : prefetches_)
    prefetchers.push_back(std::move(kv.second.first));

  // |prefetchers| and |preconnect_manager_| must be destroyed on the IO thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce([](std::vector<std::unique_ptr<ResourcePrefetcher>>,
                        std::unique_ptr<PreconnectManager>) {},
                     std::move(prefetchers), std::move(preconnect_manager_)));
  shutdown_ = true;
}

void LoadingPredictor::OnMainFrameRequest(const URLRequestSummary& summary) {
  DCHECK(summary.resource_type == content::RESOURCE_TYPE_MAIN_FRAME);
  if (shutdown_)
    return;

  const NavigationID& navigation_id = summary.navigation_id;
  CleanupAbandonedHintsAndNavigations(navigation_id);
  active_navigations_.emplace(navigation_id, navigation_id.main_frame_url);
  PrepareForPageLoad(navigation_id.main_frame_url, HintOrigin::NAVIGATION);
}

void LoadingPredictor::OnMainFrameRedirect(const URLRequestSummary& summary) {
  DCHECK(summary.resource_type == content::RESOURCE_TYPE_MAIN_FRAME);
  if (shutdown_)
    return;

  auto it = active_navigations_.find(summary.navigation_id);
  if (it != active_navigations_.end()) {
    if (summary.navigation_id.main_frame_url == summary.redirect_url)
      return;
    NavigationID navigation_id = summary.navigation_id;
    navigation_id.main_frame_url = summary.redirect_url;
    active_navigations_.emplace(navigation_id, it->second);
    active_navigations_.erase(it);
  }
}

void LoadingPredictor::OnMainFrameResponse(const URLRequestSummary& summary) {
  DCHECK(summary.resource_type == content::RESOURCE_TYPE_MAIN_FRAME);
  if (shutdown_)
    return;

  const NavigationID& navigation_id = summary.navigation_id;
  auto it = active_navigations_.find(navigation_id);
  if (it != active_navigations_.end()) {
    const GURL& initial_url = it->second;
    CancelPageLoadHint(initial_url);
    active_navigations_.erase(it);
  } else {
    CancelPageLoadHint(navigation_id.main_frame_url);
  }
}

void LoadingPredictor::SetObserverForTesting(TestLoadingObserver* observer) {
  observer_ = observer;
}

std::map<GURL, base::TimeTicks>::iterator LoadingPredictor::CancelActiveHint(
    std::map<GURL, base::TimeTicks>::iterator hint_it) {
  if (hint_it == active_hints_.end())
    return hint_it;

  const GURL& url = hint_it->first;
  MaybeRemovePrefetch(url);
  MaybeRemovePreconnect(url);
  UMA_HISTOGRAM_TIMES(
      internal::kResourcePrefetchPredictorPrefetchingDurationHistogram,
      base::TimeTicks::Now() - hint_it->second);
  return active_hints_.erase(hint_it);
}

void LoadingPredictor::CleanupAbandonedHintsAndNavigations(
    const NavigationID& navigation_id) {
  base::TimeTicks time_now = base::TimeTicks::Now();
  const base::TimeDelta max_navigation_age =
      base::TimeDelta::FromSeconds(config_.max_navigation_lifetime_seconds);

  // Hints.
  for (auto it = active_hints_.begin(); it != active_hints_.end();) {
    base::TimeDelta prefetch_age = time_now - it->second;
    if (prefetch_age > max_navigation_age) {
      // Will go to the last bucket in the duration reported in
      // CancelActiveHint() meaning that the duration was unlimited.
      it = CancelActiveHint(it);
    } else {
      ++it;
    }
  }

  // Navigations.
  for (auto it = active_navigations_.begin();
       it != active_navigations_.end();) {
    if ((it->first.tab_id == navigation_id.tab_id) ||
        (time_now - it->first.creation_time > max_navigation_age)) {
      const GURL& initial_url = it->second;
      CancelActiveHint(active_hints_.find(initial_url));
      it = active_navigations_.erase(it);
    } else {
      ++it;
    }
  }
}

void LoadingPredictor::MaybeAddPrefetch(const GURL& url,
                                        const std::vector<GURL>& urls,
                                        HintOrigin origin) {
  std::string host = url.host();
  if (prefetches_.find(host) != prefetches_.end())
    return;

  auto prefetcher = std::make_unique<ResourcePrefetcher>(
      GetWeakPtr(), profile_->GetRequestContext(),
      config_.max_prefetches_inflight_per_navigation,
      config_.max_prefetches_inflight_per_host_per_navigation, url, urls);
  // base::Unretained(prefetcher.get()) is fine, as |prefetcher| is always
  // destructed on the IO thread, and the destruction task is posted from the
  // UI thread, after the current task. Since the IO thread is FIFO, then
  // ResourcePrefetcher::Start() will be called before ~ResourcePrefetcher.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ResourcePrefetcher::Start,
                     base::Unretained(prefetcher.get())));
  prefetches_.emplace(host, std::make_pair(std::move(prefetcher), false));
  if (observer_)
    observer_->OnPrefetchingStarted(url);
}

void LoadingPredictor::MaybeRemovePrefetch(const GURL& url) {
  std::string host = url.host();
  auto it = prefetches_.find(host);
  if (it == prefetches_.end())
    return;

  auto& prefetcher_and_deleted = it->second;
  if (prefetcher_and_deleted.second)
    return;

  // Avoid to stop the same prefetcher twice, which can happen for instance with
  // multiple prediction origins for the same |url|. Prefetches are
  // de-duplicated, not their cancellation.
  prefetcher_and_deleted.second = true;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ResourcePrefetcher::Stop,
                     base::Unretained(prefetcher_and_deleted.first.get())));
  if (observer_)
    observer_->OnPrefetchingStopped(url);
}

void LoadingPredictor::ResourcePrefetcherFinished(
    ResourcePrefetcher* prefetcher,
    std::unique_ptr<ResourcePrefetcher::PrefetcherStats> stats) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (shutdown_)
    return;

  std::string host = prefetcher->main_frame_url().host();
  auto it = prefetches_.find(host);

  DCHECK(it != prefetches_.end());
  DCHECK(it->second.first.get() == prefetcher);
  stats_collector_->RecordPrefetcherStats(std::move(stats));

  if (observer_)
    observer_->OnPrefetchingFinished(prefetcher->main_frame_url());

  // ResourcePrefetcher must be destroyed on the IO thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce([](std::unique_ptr<ResourcePrefetcher>) {},
                     std::move(it->second.first)));

  prefetches_.erase(it);
}

void LoadingPredictor::MaybeAddPreconnect(
    const GURL& url,
    std::vector<PreconnectRequest>&& requests,
    HintOrigin origin) {
  DCHECK(!shutdown_);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&PreconnectManager::Start,
                     base::Unretained(preconnect_manager()), url,
                     std::move(requests)));
}

void LoadingPredictor::MaybeRemovePreconnect(const GURL& url) {
  DCHECK(!shutdown_);
  if (!preconnect_manager_)
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&PreconnectManager::Stop,
                     base::Unretained(preconnect_manager_.get()), url));
}

void LoadingPredictor::HandleOmniboxHint(const GURL& url, bool preconnectable) {
  if (!url.is_valid() || !url.has_host() ||
      !config_.IsPreconnectEnabledForOrigin(profile_, HintOrigin::OMNIBOX)) {
    return;
  }

  GURL origin = url.GetOrigin();
  bool is_new_origin = origin != last_omnibox_origin_;
  last_omnibox_origin_ = origin;
  base::TimeTicks now = base::TimeTicks::Now();
  if (preconnectable) {
    if (is_new_origin || now - last_omnibox_preconnect_time_ >=
                             kMinDelayBetweenPreconnectRequests) {
      last_omnibox_preconnect_time_ = now;
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE,
          base::BindOnce(&PreconnectManager::StartPreconnectUrl,
                         base::Unretained(preconnect_manager()), url, true));
    }
    return;
  }

  if (is_new_origin || now - last_omnibox_preresolve_time_ >=
                           kMinDelayBetweenPreresolveRequests) {
    last_omnibox_preresolve_time_ = now;
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&PreconnectManager::StartPreresolveHost,
                       base::Unretained(preconnect_manager()), url));
  }
}

void LoadingPredictor::PreconnectFinished(
    std::unique_ptr<PreconnectStats> stats) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (shutdown_)
    return;

  DCHECK(stats);
  stats_collector_->RecordPreconnectStats(std::move(stats));
}

TestLoadingObserver::~TestLoadingObserver() {
  predictor_->SetObserverForTesting(nullptr);
}

TestLoadingObserver::TestLoadingObserver(LoadingPredictor* predictor)
    : predictor_(predictor) {
  predictor_->SetObserverForTesting(this);
}

}  // namespace predictors
