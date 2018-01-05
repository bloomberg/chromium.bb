// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/favicon_cache.h"

#include "base/containers/mru_cache.h"
#include "components/favicon/core/favicon_service.h"
#include "components/omnibox/browser/autocomplete_result.h"

namespace {

size_t GetFaviconCacheSize() {
  // Set cache size to twice the number of maximum results to avoid favicon
  // refetches as the user types. Favicon fetches are uncached and can hit disk.
  return 2 * AutocompleteResult::GetMaxMatches();
}

}  // namespace

FaviconCache::FaviconCache(favicon::FaviconService* favicon_service)
    : favicon_service_(favicon_service),
      mru_cache_(GetFaviconCacheSize()),
      weak_factory_(this) {}

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

  // TODO(tommycli): Investigate using the version of this method that specifies
  // the desired size.
  favicon_service_->GetFaviconImageForPageURL(
      page_url,
      base::BindRepeating(&FaviconCache::OnFaviconFetched,
                          weak_factory_.GetWeakPtr(), page_url,
                          base::Passed(std::move(on_favicon_fetched))),
      &task_tracker_);

  return gfx::Image();
}

void FaviconCache::OnFaviconFetched(
    const GURL& page_url,
    FaviconFetchedCallback on_favicon_fetched,
    const favicon_base::FaviconImageResult& result) {
  if (result.image.IsEmpty())
    return;

  mru_cache_.Put(page_url, result.image);

  std::move(on_favicon_fetched).Run(result.image);
}
