// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites_cache.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"

namespace history {

TopSitesCache::TopSitesCache() {
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

void TopSitesCache::SetPageThumbnail(const GURL& url,
                                     RefCountedBytes* thumbnail,
                                     const ThumbnailScore& score) {
  Images& img = images_[GetCanonicalURL(url)];
  img.thumbnail = thumbnail;
  img.thumbnail_score = score;
}

Images* TopSitesCache::GetImage(const GURL& url) {
  return &images_[GetCanonicalURL(url)];
}

bool TopSitesCache::GetPageThumbnail(const GURL& url,
                                     scoped_refptr<RefCountedBytes>* bytes) {
  std::map<GURL, Images>::const_iterator found =
      images_.find(GetCanonicalURL(url));
  if (found != images_.end()) {
    *bytes = found->second.thumbnail.get();
    return true;
  }
  return false;
}

bool TopSitesCache::GetPageThumbnailScore(const GURL& url,
                                          ThumbnailScore* score) {
  std::map<GURL, Images>::const_iterator found =
      images_.find(GetCanonicalURL(url));
  if (found != images_.end()) {
    *score = found->second.thumbnail_score;
    return true;
  }
  return false;
}

GURL TopSitesCache::GetCanonicalURL(const GURL& url) {
  CanonicalURLs::iterator i = TopSitesCache::GetCanonicalURLsIterator(url);
  return i == canonical_urls_.end() ? url : i->first.first->url;
}

bool TopSitesCache::IsKnownURL(const GURL& url) {
  return GetCanonicalURLsIterator(url) != canonical_urls_.end();
}

size_t TopSitesCache::GetURLIndex(const GURL& url) {
  DCHECK(IsKnownURL(url));
  return GetCanonicalURLsIterator(url)->second;
}

void TopSitesCache::RemoveUnreferencedThumbnails() {
  for (URLToImagesMap::iterator i = images_.begin(); i != images_.end(); ) {
    if (IsKnownURL(i->first)) {
      ++i;
    } else {
      URLToImagesMap::iterator next_i = i;
      ++next_i;
      images_.erase(i);
      i = next_i;
    }
  }
}

void TopSitesCache::GenerateCanonicalURLs() {
  canonical_urls_.clear();
  for (size_t i = 0; i < top_sites_.size(); i++)
    StoreRedirectChain(top_sites_[i].redirects, i);
}

void TopSitesCache::StoreRedirectChain(const RedirectList& redirects,
                                       size_t destination) {
  // redirects is empty if the user pinned a site and there are not enough top
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

TopSitesCache::CanonicalURLs::iterator TopSitesCache::GetCanonicalURLsIterator(
    const GURL& url) {
  MostVisitedURL most_visited_url;
  most_visited_url.redirects.push_back(url);
  CanonicalURLEntry entry;
  entry.first = &most_visited_url;
  entry.second = 0u;
  return canonical_urls_.find(entry);
}

}  // namespace history
