// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/ref_counted_memory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/thumbnail_score.h"
#include "googleurl/src/gurl.h"

class SkBitmap;
struct ThumbnailScore;

namespace history {

class TopSitesBackend;
class TopSitesTest;

// Stores the data for the top "most visited" sites. This includes a cache of
// the most visited data from history, as well as the corresponding thumbnails
// of those sites.
//
// This class IS threadsafe. It is designed to be used from the UI thread of
// the browser (where history requests must be kicked off and received from)
// and from the I/O thread (where new tab page requests come in). Handling the
// new tab page requests on the I/O thread without proxying to the UI thread is
// a nontrivial performance win, especially when the browser is starting and
// the UI thread is busy.
class TopSites : public base::RefCountedThreadSafe<TopSites> {
 public:
  TopSites();
  ~TopSites();

  // Initializes this component, returning true on success.
  bool Init();

  // Sets the given thumbnail for the given URL. Returns true if the thumbnail
  // was updated. False means either the URL wasn't known to us, or we felt
  // that our current thumbnail was superior to the given one.
  bool SetPageThumbnail(const GURL& url,
                        const SkBitmap& thumbnail,
                        const ThumbnailScore& score);

  // TODO(brettw): write this.
  // bool GetPageThumbnail(const GURL& url, RefCountedBytes** data) const;

 private:
  friend class TopSitesTest;

  struct Images {
    scoped_refptr<RefCountedBytes> thumbnail;
    ThumbnailScore thumbnail_score;

    // TODO(brettw): this will eventually store the favicon.
    // scoped_refptr<RefCountedBytes> favicon;
  };

  // Saves the set of the top URLs visited by this user. The 0th item is the
  // most popular.
  //
  // DANGER! This will clear all data from the input argument.
  void StoreMostVisited(std::vector<MostVisitedURL>* most_visited);

  // Saves the given set of redirects. The redirects are in order of the
  // given vector, so [0] -> [1] -> [2].
  void StoreRedirectChain(const RedirectList& redirects,
                          size_t destination);

  // Each item in the most visited view can redirect elsewhere. This returns
  // the canonical URL one identifying the site if the given URL does appear
  // in the "top sites" list.
  //
  // If the given URL is not in the top sites, this will return an empty GURL.
  GURL GetCanonicalURL(const GURL& url) const;

  // Finds the given URL in the redirect chain for the given TopSite, and
  // returns the distance from the destination in hops that the given URL is.
  // The URL is assumed to be in the list. The destination is 0.
  static int GetRedirectDistanceForURL(const MostVisitedURL& most_visited,
                                       const GURL& url);

  // Generates the diff of things that happened between "old" and "new."
  //
  // The URLs that are in "new" but not "old" will be have their index into
  // "new" put in |added_urls|. The URLs that are in "old" but not "new" will
  // have their index into "old" put into |deleted_urls|.
  //
  // URLs appearing in both old and new lists but having different indices will
  // have their index into "new" be put into |moved_urls|.
  static void DiffMostVisited(const std::vector<MostVisitedURL>& old_list,
                              const std::vector<MostVisitedURL>& new_list,
                              std::vector<size_t>* added_urls,
                              std::vector<size_t>* deleted_urls,
                              std::vector<size_t>* moved_urls);

  mutable Lock lock_;

  // The cached version of the top sites. The 0th item in this vector is the
  // #1 site.
  std::vector<MostVisitedURL> top_sites_;

  // The images corresponding to the top_sites. This is indexed by the URL of
  // the top site, so this doesn't have to be shuffled around when the ordering
  // changes of the top sites. Some top_sites_ entries may not have images.
  std::map<GURL, Images> top_images_;

  // Generated from the redirects to and from the most visited pages, this
  // maps the redirects to the index into top_sites_ that contains it.
  std::map<GURL, size_t> canonical_urls_;

  // TODO(brettw): use the blacklist.
  // std::set<GURL> blacklist_;

  DISALLOW_COPY_AND_ASSIGN(TopSites);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_H_

