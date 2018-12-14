// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "components/history/core/browser/top_sites_cache.h"

namespace history {

TopSitesCache::CanonicalURLQuery::CanonicalURLQuery(const GURL& url) {
  most_visited_url_.redirects.push_back(url);
  entry_.first = &most_visited_url_;
  entry_.second = 0u;
}

TopSitesCache::CanonicalURLQuery::~CanonicalURLQuery() {
}

TopSitesCache::TopSitesCache() {
}

TopSitesCache::~TopSitesCache() {
}

void TopSitesCache::SetTopSites(const MostVisitedURLList& top_sites) {
  top_sites_ = top_sites;
  GenerateCanonicalURLs();
}

const GURL& TopSitesCache::GetCanonicalURL(const GURL& url) const {
  auto it = GetCanonicalURLsIterator(url);
  return it == canonical_urls_.end() ? url : it->first.first->url;
}

bool TopSitesCache::IsKnownURL(const GURL& url) const {
  return GetCanonicalURLsIterator(url) != canonical_urls_.end();
}

size_t TopSitesCache::GetURLIndex(const GURL& url) const {
  DCHECK(IsKnownURL(url));
  return GetCanonicalURLsIterator(url)->second;
}

size_t TopSitesCache::GetNumURLs() const {
  return top_sites_.size();
}

void TopSitesCache::GenerateCanonicalURLs() {
  canonical_urls_.clear();
  for (size_t i = 0; i < top_sites_.size(); i++)
    StoreRedirectChain(top_sites_[i].redirects, i);
}

void TopSitesCache::StoreRedirectChain(const RedirectList& redirects,
                                       size_t destination) {
  // |redirects| is empty if the user pinned a site and there are not enough top
  // sites before the pinned site.

  // Map all the redirected URLs to the destination.
  for (size_t i = 0; i < redirects.size(); i++) {
    // If this redirect is already known, don't replace it with a new one.
    if (!IsKnownURL(redirects[i])) {
      CanonicalURLEntry entry;
      entry.first = &(top_sites_[destination]);
      entry.second = i;
      canonical_urls_[entry] = destination;
    }
  }
}

TopSitesCache::CanonicalURLs::const_iterator
    TopSitesCache::GetCanonicalURLsIterator(const GURL& url) const {
  return canonical_urls_.find(CanonicalURLQuery(url).entry());
}

const GURL& TopSitesCache::GetURLFromIterator(
    CanonicalURLs::const_iterator it) const {
  DCHECK(it != canonical_urls_.end());
  return it->first.first->redirects[it->first.second];
}

}  // namespace history
