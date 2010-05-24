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
#include "base/timer.h"
#include "base/ref_counted.h"
#include "base/ref_counted_memory.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/common/thumbnail_score.h"
#include "googleurl/src/gurl.h"

class SkBitmap;
class Profile;

namespace history {

class TopSitesBackend;
class TopSitesTest;

typedef std::vector<MostVisitedURL> MostVisitedURLList;

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
  explicit TopSites(Profile* profile);

  class MockHistoryService {
    // A mockup of a HistoryService used for testing TopSites.
   public:
    virtual HistoryService::Handle QueryMostVisitedURLs(
        int result_count, int days_back,
        CancelableRequestConsumerBase* consumer,
        HistoryService::QueryMostVisitedURLsCallback* callback) = 0;
    virtual ~MockHistoryService() {}
  };

  // Initializes TopSites.
  void Init();

  // Sets the given thumbnail for the given URL. Returns true if the thumbnail
  // was updated. False means either the URL wasn't known to us, or we felt
  // that our current thumbnail was superior to the given one.
  bool SetPageThumbnail(const GURL& url,
                        const SkBitmap& thumbnail,
                        const ThumbnailScore& score);

  // Returns a list of most visited URLs or an empty list if it's not
  // cached yet.
  MostVisitedURLList GetMostVisitedURLs();

  // Get a thumbnail for a given page. Returns true iff we have the thumbnail.
  bool GetPageThumbnail(const GURL& url, RefCountedBytes** data) const;

 private:
  friend class base::RefCountedThreadSafe<TopSites>;
  friend class TopSitesTest;
  friend class TopSitesTest_GetMostVisited_Test;

  ~TopSites();

  struct Images {
    scoped_refptr<RefCountedBytes> thumbnail;
    ThumbnailScore thumbnail_score;

    // TODO(brettw): this will eventually store the favicon.
    // scoped_refptr<RefCountedBytes> favicon;
  };

  void StartQueryForMostVisited();

  // Handler for the query response.
  void OnTopSitesAvailable(CancelableRequestProvider::Handle handle,
                           MostVisitedURLList data);

  // Saves the set of the top URLs visited by this user. The 0th item is the
  // most popular.
  //
  // DANGER! This will clear all data from the input argument.
  void StoreMostVisited(MostVisitedURLList* most_visited);

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
  static void DiffMostVisited(const MostVisitedURLList& old_list,
                              const MostVisitedURLList& new_list,
                              std::vector<size_t>* added_urls,
                              std::vector<size_t>* deleted_urls,
                              std::vector<size_t>* moved_urls);

  // For testing with a HistoryService mock.
  void SetMockHistoryService(MockHistoryService* mhs);

  Profile* profile_;
  // A mockup to use for testing. If NULL, use the real HistoryService
  // from the profile_. See SetMockHistoryService.
  MockHistoryService* mock_history_service_;
  CancelableRequestConsumerTSimple<PageUsageData*> cancelable_consumer_;
  mutable Lock lock_;

  // The cached version of the top sites. The 0th item in this vector is the
  // #1 site.
  MostVisitedURLList top_sites_;

  // The images corresponding to the top_sites. This is indexed by the URL of
  // the top site, so this doesn't have to be shuffled around when the ordering
  // changes of the top sites. Some top_sites_ entries may not have images.
  std::map<GURL, Images> top_images_;

  // Generated from the redirects to and from the most visited pages, this
  // maps the redirects to the index into top_sites_ that contains it.
  std::map<GURL, size_t> canonical_urls_;

  // Timer for updating TopSites data.
  base::OneShotTimer<TopSites> timer_;

  // TODO(brettw): use the blacklist.
  // std::set<GURL> blacklist_;

  DISALLOW_COPY_AND_ASSIGN(TopSites);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_H_
