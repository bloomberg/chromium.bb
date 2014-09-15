// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetcher_manager.h"

#include "base/bind.h"
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
  STLDeleteContainerPairSecondPointers(prefetcher_map_.begin(),
                                       prefetcher_map_.end());
}

void ResourcePrefetcherManager::MaybeAddPrefetch(
    const NavigationID& navigation_id,
    PrefetchKeyType key_type,
    scoped_ptr<ResourcePrefetcher::RequestVector> requests) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Don't add a duplicate prefetch for the same host or URL.
  std::string key = key_type == PREFETCH_KEY_TYPE_HOST ?
      navigation_id.main_frame_url.host() : navigation_id.main_frame_url.spec();
  if (ContainsKey(prefetcher_map_, key))
    return;

  ResourcePrefetcher* prefetcher = new ResourcePrefetcher(
      this, config_, navigation_id, key_type, requests.Pass());
  prefetcher_map_.insert(std::make_pair(key, prefetcher));
  prefetcher->Start();
}

void ResourcePrefetcherManager::MaybeRemovePrefetch(
    const NavigationID& navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Look for a URL based prefetch first.
  PrefetcherMap::iterator it = prefetcher_map_.find(
      navigation_id.main_frame_url.spec());
  if (it != prefetcher_map_.end() &&
      it->second->navigation_id() == navigation_id) {
    it->second->Stop();
    return;
  }

  // No URL based prefetching, look for host based.
  it = prefetcher_map_.find(navigation_id.main_frame_url.host());
  if (it != prefetcher_map_.end() &&
      it->second->navigation_id() == navigation_id) {
    it->second->Stop();
  }
}

void ResourcePrefetcherManager::ResourcePrefetcherFinished(
    ResourcePrefetcher* resource_prefetcher,
    ResourcePrefetcher::RequestVector* requests) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // |predictor_| can only be accessed from the UI thread.
  scoped_ptr<ResourcePrefetcher::RequestVector> scoped_requests(requests);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ResourcePrefetcherManager::ResourcePrefetcherFinishedOnUI,
                 this,
                 resource_prefetcher->navigation_id(),
                 resource_prefetcher->key_type(),
                 base::Passed(&scoped_requests)));

  const GURL& main_frame_url =
      resource_prefetcher->navigation_id().main_frame_url;
  const std::string key =
      resource_prefetcher->key_type() == PREFETCH_KEY_TYPE_HOST ?
          main_frame_url.host() : main_frame_url.spec();
  PrefetcherMap::iterator it = prefetcher_map_.find(key);
  DCHECK(it != prefetcher_map_.end());
  delete it->second;
  prefetcher_map_.erase(it);
}

void ResourcePrefetcherManager::ResourcePrefetcherFinishedOnUI(
    const NavigationID& navigation_id,
    PrefetchKeyType key_type,
    scoped_ptr<ResourcePrefetcher::RequestVector> requests) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |predictor_| may have been set to NULL if the predictor is shutting down.
  if (predictor_) {
    predictor_->FinishedPrefetchForNavigation(navigation_id,
                                              key_type,
                                              requests.release());
  }
}

net::URLRequestContext* ResourcePrefetcherManager::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  return context_getter_->GetURLRequestContext();
}

}  // namespace predictors
