// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/favicon_cache.h"

#include "base/containers/mru_cache.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/autocomplete_result.h"

namespace {

size_t GetFaviconCacheSize() {
  // Set cache size to twice the number of maximum results to avoid favicon
  // refetches as the user types. Favicon fetches are uncached and can hit disk.
  return 2 * AutocompleteResult::GetMaxMatches();
}

}  // namespace

FaviconCache::FaviconCache(favicon::FaviconService* favicon_service,
                           history::HistoryService* history_service)
    : favicon_service_(favicon_service),
      history_observer_(this),
      mru_cache_(GetFaviconCacheSize()),
      weak_factory_(this) {
  if (history_service)
    history_observer_.Add(history_service);
}

FaviconCache::~FaviconCache() {}

gfx::Image FaviconCache::GetFaviconForPageUrl(
    const GURL& page_url,
    FaviconFetchedCallback on_favicon_fetched) {
  auto cache_iterator = mru_cache_.Get(page_url);
  if (cache_iterator != mru_cache_.end())
    return cache_iterator->second;

  // We don't have the favicon in the cache. We kick off the request and return
  // an empty gfx::Image.
  if (!favicon_service_)
    return gfx::Image();

  // We have an outstanding request for this page. Add one more waiting callback
  // and return an empty gfx::Image.
  auto it = pending_requests_.find(page_url);
  if (it != pending_requests_.end()) {
    it->second.push_back(std::move(on_favicon_fetched));
    return gfx::Image();
  }

  // TODO(tommycli): Investigate using the version of this method that specifies
  // the desired size.
  favicon_service_->GetFaviconImageForPageURL(
      page_url,
      base::BindRepeating(&FaviconCache::OnFaviconFetched,
                          weak_factory_.GetWeakPtr(), page_url),
      &task_tracker_);
  pending_requests_[page_url].push_back(std::move(on_favicon_fetched));

  return gfx::Image();
}

void FaviconCache::OnFaviconFetched(
    const GURL& page_url,
    const favicon_base::FaviconImageResult& result) {
  // TODO(tommycli): Also cache null image results with a reasonable expiry.
  if (result.image.IsEmpty())
    return;

  mru_cache_.Put(page_url, result.image);

  auto it = pending_requests_.find(page_url);
  DCHECK(it != pending_requests_.end());
  for (auto& callback : it->second) {
    std::move(callback).Run(result.image);
  }
  pending_requests_.erase(it);
}

void FaviconCache::OnURLsDeleted(history::HistoryService* history_service,
                                 bool all_history,
                                 bool expired,
                                 const history::URLRows& deleted_rows,
                                 const std::set<GURL>& favicon_urls) {
  // We only care about actual user (or sync) deletions.
  if (expired)
    return;

  if (all_history) {
    mru_cache_.Clear();
    return;
  }

  for (const history::URLRow& row : deleted_rows) {
    auto it = mru_cache_.Peek(row.url());
    if (it != mru_cache_.end())
      mru_cache_.Erase(it);
  }
}
