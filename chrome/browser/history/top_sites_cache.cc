// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites_cache.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"

namespace history {

TopSitesCache::CanonicalURLQuery::CanonicalURLQuery(const GURL& url) {
  most_visited_url_.redirects.push_back(url);
  entry_.first = &most_visited_url_;
  entry_.second = 0u;
}

TopSitesCache::CanonicalURLQuery::~CanonicalURLQuery() {
}

TopSitesCache::TopSitesCache() {
  clear_query_ref_.ClearQuery();
  clear_query_ref_.ClearRef();
  clear_path_query_ref_.ClearQuery();
  clear_path_query_ref_.ClearRef();
  clear_path_query_ref_.ClearPath();
}

TopSitesCache::~TopSitesCache() {
}

void TopSitesCache::SetTopSites(const MostVisitedURLList& top_sites) {
  top_sites_ = top_sites;
  GenerateCanonicalURLs();
}

void TopSitesCache::SetThumbnails(const URLToImagesMap& images) {
  images_ = images;
}

Images* TopSitesCache::GetImage(const GURL& url) {
  return &images_[GetCanonicalURL(url)];
}

bool TopSitesCache::GetPageThumbnail(
    const GURL& url,
    scoped_refptr<base::RefCountedMemory>* bytes) const {
  std::map<GURL, Images>::const_iterator found =
      images_.find(GetCanonicalURL(url));
  if (found != images_.end()) {
    base::RefCountedMemory* data = found->second.thumbnail.get();
    if (data) {
      *bytes = data;
      return true;
    }
  }
  return false;
}

bool TopSitesCache::GetPageThumbnailScore(const GURL& url,
                                          ThumbnailScore* score) const {
  std::map<GURL, Images>::const_iterator found =
      images_.find(GetCanonicalURL(url));
  if (found != images_.end()) {
    *score = found->second.thumbnail_score;
    return true;
  }
  return false;
}

const GURL& TopSitesCache::GetCanonicalURL(const GURL& url) const {
  CanonicalURLs::const_iterator it = GetCanonicalURLsIterator(url);
  return it == canonical_urls_.end() ? url : it->first.first->url;
}

GURL TopSitesCache::GetSpecializedCanonicalURL(const GURL& url) const {
  // Perform effective binary search for URL prefix search.
  CanonicalURLs::const_iterator it =
      canonical_urls_.upper_bound(CanonicalURLQuery(url).entry());

  // Check whether |prev_it| equals to |url|, ignoring "?query#ref". This
  // handles exact match and equality matches with earlier "?query#ref" part.
  if (it != canonical_urls_.begin()) {
    CanonicalURLs::const_iterator prev_it = it;
    --prev_it;
    if (url.ReplaceComponents(clear_query_ref_) ==
        GetURLFromIterator(prev_it).ReplaceComponents(clear_query_ref_)) {
      return prev_it->first.first->url;
    }
  }

  // Check whether |url| is a URL prefix of |it|. This handles strict
  // specialized match and equality matches with later "?query#ref".
  if (it != canonical_urls_.end()) {
    GURL compare_url(GetURLFromIterator(it));
    if (HaveSameSchemeHostAndPort(url, compare_url) &&
        IsPathPrefix(url.path(), compare_url.path())) {
      return it->first.first->url;
    }
  }

  return GURL::EmptyGURL();
}

GURL TopSitesCache::GetGeneralizedCanonicalURL(const GURL& url) const {
  CanonicalURLs::const_iterator it_hi =
      canonical_urls_.lower_bound(CanonicalURLQuery(url).entry());
  if (it_hi != canonical_urls_.end()) {
    // Test match ignoring "?query#ref". This also handles exact match.
    if (url.ReplaceComponents(clear_query_ref_) ==
        GetURLFromIterator(it_hi).ReplaceComponents(clear_query_ref_)) {
      return it_hi->first.first->url;
    }
  }
  // Everything on or after |it_hi| is irrelevant.

  GURL base_url(url.ReplaceComponents(clear_path_query_ref_));
  CanonicalURLs::const_iterator it_lo =
      canonical_urls_.lower_bound(CanonicalURLQuery(base_url).entry());
  if (it_lo == canonical_urls_.end())
    return GURL::EmptyGURL();
  GURL compare_url_lo(GetURLFromIterator(it_lo));
  if (!HaveSameSchemeHostAndPort(base_url, compare_url_lo) ||
      !IsPathPrefix(base_url.path(), compare_url_lo.path())) {
    return GURL::EmptyGURL();
  }
  // Everything before |it_lo| is irrelevant.

  // Search in [|it_lo|, |it_hi|) in reversed order. The first URL found that's
  // a prefix of |url| (ignoring "?query#ref") would be returned.
  for (CanonicalURLs::const_iterator it = it_hi; it != it_lo;) {
    --it;
    GURL compare_url(GetURLFromIterator(it));
    DCHECK(HaveSameSchemeHostAndPort(compare_url, url));
    if (IsPathPrefix(compare_url.path(), url.path()))
      return it->first.first->url;
  }

  return GURL::EmptyGURL();
}

bool TopSitesCache::IsKnownURL(const GURL& url) const {
  return GetCanonicalURLsIterator(url) != canonical_urls_.end();
}

size_t TopSitesCache::GetURLIndex(const GURL& url) const {
  DCHECK(IsKnownURL(url));
  return GetCanonicalURLsIterator(url)->second;
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
