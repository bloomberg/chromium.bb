// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_CACHE_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_CACHE_H_

#include <map>
#include <utility>

#include "base/memory/ref_counted.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/url_utils.h"

class GURL;

namespace history {

// TopSiteCache caches thumbnails for visited pages. Retrieving thumbnails from
// a given input URL is a two-stage process:
//
//   input URL --(map 1)--> canonical URL --(map 2)--> image.
//
// (map 1) searches input URL in |canonical_urls_|. canonical URL is
// assigned to the resulting value if found; else input URL.
//
// (map 2) simply looks up canonical URL in |images_|.
//
// TopSiteCache also provides GetCanonicalURLsIteratorForPrefix(), which is an
// alternative implementation of (map 1) that does the following:
// - if canonical URL is a key in |canonical_urls_|, return the value.
// - else if canonical URL is a "URL prefix" (see comment in url_utils.h) of
//   some key in |canonical_urls_|, return the value corresponding to the key.
// - else return input URL.

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

  // Returns the thumbnail as an Image for the specified url. This adds an entry
  // for |url| if one has not yet been added.
  Images* GetImage(const GURL& url);

  // Fetches the thumbnail for the specified url. Returns true if there is a
  // thumbnail for the specified url. It is possible for a URL to be in TopSites
  // but not have an thumbnail.
  bool GetPageThumbnail(const GURL& url,
                        scoped_refptr<base::RefCountedMemory>* bytes) const;

  // Fetches the thumbnail score for the specified url. Returns true if
  // there is a thumbnail score for the specified url.
  bool GetPageThumbnailScore(const GURL& url, ThumbnailScore* score) const;

  // Returns the canonical URL for |url|.
  const GURL& GetCanonicalURL(const GURL& url) const;

  // Returns the canonical URL for |url_prefix| that matches by prefix.
  // If multiple matches exist, returns the canonical URL for the first
  // matching entry under lexicographical order.
  const GURL& GetCanonicalURLForPrefix(const GURL& url_prefix) const;

  // Returns true if |url| is known.
  bool IsKnownURL(const GURL& url) const;

  // Returns the index into |top_sites_| for |url|.
  size_t GetURLIndex(const GURL& url) const;

 private:
  // The entries in CanonicalURLs, see CanonicalURLs for details. The second
  // argument gives the index of the URL into MostVisitedURLs redirects.
  typedef std::pair<MostVisitedURL*, size_t> CanonicalURLEntry;

  // Comparator used for CanonicalURLs.
  class CanonicalURLComparator {
   public:
    bool operator()(const CanonicalURLEntry& e1,
                    const CanonicalURLEntry& e2) const {
      return CanonicalURLStringCompare(e1.first->redirects[e1.second].spec(),
                                       e2.first->redirects[e2.second].spec());
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

  // Returns the iterator into |canonical_urls_| for the |url|.
  CanonicalURLs::const_iterator GetCanonicalURLsIterator(const GURL& url) const;

  // Returns the first iterator into |canonical_urls_| for which |prefix_url|
  // is a URL prefix. Returns |canonical_urls_.end()| if no match is found.
  CanonicalURLs::const_iterator GetCanonicalURLsIteratorForPrefix(
      const GURL& prefix_url) const;

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
