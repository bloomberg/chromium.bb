// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetcher_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace predictors {

ResourcePrefetcherManager::ResourcePrefetcherManager(
    ResourcePrefetchPredictor* predictor,
    const ResourcePrefetchPredictorConfig& config,
    net::URLRequestContextGetter* context_getter)
    : predictor_(predictor),
      config_(config),
      context_getter_(context_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(predictor_);
  CHECK(context_getter_);
}

ResourcePrefetcherManager::~ResourcePrefetcherManager() {
  DCHECK(prefetcher_map_.empty())
      << "Did not call ShutdownOnUIThread or ShutdownOnIOThread. "
         " Will leak Prefetcher pointers.";
}

void ResourcePrefetcherManager::ShutdownOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  predictor_ = NULL;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&ResourcePrefetcherManager::ShutdownOnIOThread,
                 this));
}

void ResourcePrefetcherManager::ShutdownOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  prefetcher_map_.clear();
}

void ResourcePrefetcherManager::MaybeAddPrefetch(
    const GURL& main_frame_url,
    const std::vector<GURL>& urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Don't add a duplicate prefetch for the same host.
  std::string key = main_frame_url.host();
  if (base::ContainsKey(prefetcher_map_, key))
    return;

  auto prefetcher =
      base::MakeUnique<ResourcePrefetcher>(this, config_, main_frame_url, urls);
  prefetcher->Start();
  prefetcher_map_[key] = std::move(prefetcher);
}

void ResourcePrefetcherManager::MaybeRemovePrefetch(
    const GURL& main_frame_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::string key = main_frame_url.host();
  auto it = prefetcher_map_.find(key);
  if (it != prefetcher_map_.end() &&
      it->second->main_frame_url() == main_frame_url) {
    it->second->Stop();
  }
}

void ResourcePrefetcherManager::ResourcePrefetcherFinished(
    ResourcePrefetcher* resource_prefetcher,
    std::unique_ptr<ResourcePrefetcher::PrefetcherStats> stats) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const GURL& main_frame_url = resource_prefetcher->main_frame_url();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ResourcePrefetchPredictor::OnPrefetchingFinished,
                 base::Unretained(predictor_), main_frame_url,
                 base::Passed(std::move(stats))));

  const std::string key = main_frame_url.host();
  auto it = prefetcher_map_.find(key);
  DCHECK(it != prefetcher_map_.end());
  prefetcher_map_.erase(it);
}

net::URLRequestContext* ResourcePrefetcherManager::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  return context_getter_->GetURLRequestContext();
}

}  // namespace predictors
