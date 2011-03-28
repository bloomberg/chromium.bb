// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_CACHE_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_CACHE_H_
#pragma once

#include <algorithm>
#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/history/history_types.h"

class RefCountedBytes;

namespace history {

// TopSitesCache caches the top sites and thumbnails for TopSites.
class TopSitesCache {
 public:
  TopSitesCache();
  ~TopSitesCache();

  // The top sites.
  void SetTopSites(const MostVisitedURLList& top_sites);
  const MostVisitedURLList& top_sites() const { return top_sites_; }

  // The thumbnails.
  void SetThumbnails(const URLToImagesMap& images);
  const URLToImagesMap& images() const { return images_; }

  // Set a thumbnail.
  void SetPageThumbnail(const GURL& url,
                        RefCountedBytes* thumbnail,
                        const ThumbnailScore& score);

  // Returns the thumbnail as an Image for the specified url. This adds an entry
  // for |url| if one has not yet been added.
  Images* GetImage(const GURL& url);

  // Fetches the thumbnail for the specified url. Returns true if there is a
  // thumbnail for the specified url.
  bool GetPageThumbnail(const GURL& url,
                        scoped_refptr<RefCountedBytes>* bytes);

  // Fetches the thumbnail score for the specified url. Returns true if
  // there is a thumbnail score for the specified url.
  bool GetPageThumbnailScore(const GURL& url, ThumbnailScore* score);

  // Returns the canonical URL for |url|.
  GURL GetCanonicalURL(const GURL& url);

  // Returns true if |url| is known.
  bool IsKnownURL(const GURL& url);

  // Returns the index into |top_sites_| for |url|.
  size_t GetURLIndex(const GURL& url);

  // Removes any thumbnails that are no longer referenced by the top sites.
  void RemoveUnreferencedThumbnails();

 private:
  // The entries in CanonicalURLs, see CanonicalURLs for details. The second
  // argument gives the index of the URL into MostVisitedURLs redirects.
  typedef std::pair<MostVisitedURL*, size_t> CanonicalURLEntry;

  // Comparator used for CanonicalURLs.
  class CanonicalURLComparator {
   public:
    bool operator()(const CanonicalURLEntry& e1,
                    const CanonicalURLEntry& e2) const {
      return e1.first->redirects[e1.second] < e2.first->redirects[e2.second];
    }
  };

  // This is used to map from redirect url to the MostVisitedURL the redirect is
  // from. Ideally this would be map<GURL, size_t> (second param indexing into
  // top_sites_), but this results in duplicating all redirect urls. As some
  // sites have a lot of redirects, we instead use the MostVisitedURL* and the
  // index of the redirect as the key, and the index into top_sites_ as the
  // value. This way we aren't duplicating GURLs. CanonicalURLComparator
  // enforces the ordering as if we were using GURLs.
  typedef std::map<CanonicalURLEntry, size_t,
                   CanonicalURLComparator> CanonicalURLs;

  // Generates the set of canonical urls from |top_sites_|.
  void GenerateCanonicalURLs();

  // Stores a set of redirects. This is used by GenerateCanonicalURLs.
  void StoreRedirectChain(const RedirectList& redirects, size_t destination);

  // Returns the iterator into canconical_urls_ for the specified url.
  CanonicalURLs::iterator GetCanonicalURLsIterator(const GURL& url);

  // The top sites.
  MostVisitedURLList top_sites_;

  // The images. These map from canonical url to image.
  URLToImagesMap images_;

  // Generated from the redirects to and from the most visited pages. See
  // description above typedef for details.
  CanonicalURLs canonical_urls_;

  DISALLOW_COPY_AND_ASSIGN(TopSitesCache);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_CACHE_H_
