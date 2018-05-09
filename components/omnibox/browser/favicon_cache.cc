// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/favicon_cache.h"

#include <tuple>

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

// static
const int FaviconCache::kEmptyFaviconCacheLifetimeInSeconds = 60;

bool FaviconCache::Request::operator<(const Request& rhs) const {
  // Compare |type| first, and if equal, compare |url|.
  return std::tie(type, url) < std::tie(rhs.type, rhs.url);
}

FaviconCache::FaviconCache(favicon::FaviconService* favicon_service,
                           history::HistoryService* history_service)
    : favicon_service_(favicon_service),
      history_observer_(this),
      mru_cache_(GetFaviconCacheSize()),
      responses_without_favicons_(GetFaviconCacheSize()),
      weak_factory_(this) {
  if (history_service)
    history_observer_.Add(history_service);
}

FaviconCache::~FaviconCache() {}

gfx::Image FaviconCache::GetFaviconForPageUrl(
    const GURL& page_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return GetFaviconInternal({RequestType::BY_PAGE_URL, page_url},
                            std::move(on_favicon_fetched));
}

gfx::Image FaviconCache::GetFaviconForIconUrl(
    const GURL& icon_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return GetFaviconInternal({RequestType::BY_ICON_URL, icon_url},
                            std::move(on_favicon_fetched));
}

gfx::Image FaviconCache::GetFaviconInternal(
    const Request& request,
    FaviconFetchedCallback on_favicon_fetched) {
  if (!favicon_service_)
    return gfx::Image();

  if (request.url.is_empty() || !request.url.is_valid())
    return gfx::Image();

  // Early exit if we have a cached favicon ready.
  auto cache_iterator = mru_cache_.Get(request);
  if (cache_iterator != mru_cache_.end())
    return cache_iterator->second;

  // Early exit if we've already established that we don't have the favicon.
  AgeOutOldCachedEmptyFavicons();
  if (responses_without_favicons_.Peek(request) !=
      responses_without_favicons_.end()) {
    return gfx::Image();
  }

  // We have an outstanding request for this page. Add one more waiting callback
  // and return an empty gfx::Image.
  auto it = pending_requests_.find(request);
  if (it != pending_requests_.end()) {
    it->second.push_back(std::move(on_favicon_fetched));
    return gfx::Image();
  }

  if (request.type == RequestType::BY_PAGE_URL) {
    favicon_service_->GetFaviconImageForPageURL(
        request.url,
        base::BindRepeating(&FaviconCache::OnFaviconFetched,
                            weak_factory_.GetWeakPtr(), request),
        &task_tracker_);
  } else if (request.type == RequestType::BY_ICON_URL) {
    favicon_service_->GetFaviconImage(
        request.url,
        base::BindRepeating(&FaviconCache::OnFaviconFetched,
                            weak_factory_.GetWeakPtr(), request),
        &task_tracker_);
  } else {
    NOTREACHED();
  }

  pending_requests_[request].push_back(std::move(on_favicon_fetched));

  return gfx::Image();
}

void FaviconCache::OnFaviconFetched(
    const Request& request,
    const favicon_base::FaviconImageResult& result) {
  if (result.image.IsEmpty()) {
    responses_without_favicons_.Put(request, GetTimeNow());
    pending_requests_.erase(request);
    return;
  }

  mru_cache_.Put(request, result.image);

  auto it = pending_requests_.find(request);
  DCHECK(it != pending_requests_.end());
  for (auto& callback : it->second) {
    std::move(callback).Run(result.image);
  }
  pending_requests_.erase(it);
}

void FaviconCache::OnURLVisited(history::HistoryService* history_service,
                                ui::PageTransition transition,
                                const history::URLRow& row,
                                const history::RedirectList& redirects,
                                base::Time visit_time) {
  auto it =
      responses_without_favicons_.Peek({RequestType::BY_PAGE_URL, row.url()});
  if (it != responses_without_favicons_.end())
    responses_without_favicons_.Erase(it);
}

void FaviconCache::AgeOutOldCachedEmptyFavicons() {
  // TODO(tommycli): This code is based on chrome_browser_net::TimedCache.
  // Investigate combining.
  base::TimeDelta max_duration =
      base::TimeDelta::FromSeconds(kEmptyFaviconCacheLifetimeInSeconds);
  base::TimeTicks age_cutoff = GetTimeNow() - max_duration;
  auto eldest = responses_without_favicons_.rbegin();
  while (eldest != responses_without_favicons_.rend() &&
         eldest->second <= age_cutoff) {
    eldest = responses_without_favicons_.Erase(eldest);
  }
}

base::TimeTicks FaviconCache::GetTimeNow() {
  return base::TimeTicks::Now();
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
    auto it = mru_cache_.Peek({RequestType::BY_PAGE_URL, row.url()});
    if (it != mru_cache_.end())
      mru_cache_.Erase(it);
  }
}
