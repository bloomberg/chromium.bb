// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FAVICON_CACHE_H_
#define COMPONENTS_OMNIBOX_BROWSER_FAVICON_CACHE_H_

#include <list>
#include <map>

#include "base/callback_forward.h"
#include "base/containers/mru_cache.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"

namespace favicon {
class FaviconService;
}

namespace gfx {
class Image;
}

class GURL;

typedef base::OnceCallback<void(const gfx::Image& favicon)>
    FaviconFetchedCallback;

// This caches favicons by both page URL and icon URL. We cache a small number
// of them so we can synchronously deliver them to the UI to prevent flicker as
// the user types.
//
// It also stores and times out null results from when we cannot fetch a favicon
// from the history database. Null results timeout after a while.
class FaviconCache : public history::HistoryServiceObserver {
 public:
  FaviconCache(favicon::FaviconService* favicon_service,
               history::HistoryService* history_service);
  ~FaviconCache() override;

  // These methods fetch favicons by the |page_url| or |icon_url| respectively.
  // If the correct favicon is already cached, these methods return the image
  // synchronously.
  //
  // If the correct favicon is not cached, we return an empty gfx::Image and
  // forward the request to FaviconService. |on_favicon_fetched| is stored in a
  // pending callback list, and subsequent identical requests are added to the
  // same pending list without issuing duplicate calls to FaviconService.
  //
  // If FaviconService responds with a non-empty image, we fulfill all the
  // matching |on_favicon_fetched| callbacks in the pending list, and cache the
  // result so that future matching requests can be fulfilled synchronously.
  //
  // If FaviconService responds with an empty image (because the correct favicon
  // isn't in our database), we simply erase all the pending callbacks, and also
  // cache the result.
  //
  // Therefore, |on_favicon_fetched| may or may not be called asynchrously
  // later, but will never be called with an empty result. It will also never
  // be called synchronously.
  gfx::Image GetFaviconForPageUrl(const GURL& page_url,
                                  FaviconFetchedCallback on_favicon_fetched);
  gfx::Image GetFaviconForIconUrl(const GURL& icon_url,
                                  FaviconFetchedCallback on_favicon_fetched);

 private:
  FRIEND_TEST_ALL_PREFIXES(FaviconCacheTest, ClearIconsWithHistoryDeletions);
  FRIEND_TEST_ALL_PREFIXES(FaviconCacheTest, ExpireNullFaviconsByHistory);
  FRIEND_TEST_ALL_PREFIXES(FaviconCacheTest, ExpireNullFaviconsByTime);

  enum class RequestType {
    BY_PAGE_URL,
    BY_ICON_URL,
  };

  struct Request {
    RequestType type;
    GURL url;

    // This operator is defined to support using Request as a key of std::map.
    bool operator<(const Request& rhs) const;
  };

  // Chosen arbitrarily. Declared in the class for testing.
  static const int kEmptyFaviconCacheLifetimeInSeconds;

  // Internal method backing GetFaviconForPageUrl and GetFaviconForIconUrl.
  gfx::Image GetFaviconInternal(const Request& request,
                                FaviconFetchedCallback on_favicon_fetched);

  // This is the callback passed to the underyling FaviconService. When this
  // is called, all the pending requests that match |request| will be called.
  void OnFaviconFetched(const Request& request,
                        const favicon_base::FaviconImageResult& result);

  void AgeOutOldCachedEmptyFavicons();

  // Virtual for testing.
  virtual base::TimeTicks GetTimeNow();

  // history::HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    ui::PageTransition transition,
                    const history::URLRow& row,
                    const history::RedirectList& redirects,
                    base::Time visit_time) override;
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // Non-owning pointer to a KeyedService.
  favicon::FaviconService* favicon_service_;

  ScopedObserver<history::HistoryService, FaviconCache> history_observer_;

  base::CancelableTaskTracker task_tracker_;
  std::map<Request, std::list<FaviconFetchedCallback>> pending_requests_;

  base::MRUCache<Request, gfx::Image> mru_cache_;

  // Keep responses with empty favicons in a separate list, to prevent a
  // response with an empty favicon from ever evicting an existing favicon.
  // The value is used to age out entries that are too old.
  base::MRUCache<Request, base::TimeTicks> responses_without_favicons_;

  base::WeakPtrFactory<FaviconCache> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FaviconCache);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAVICON_CACHE_H_
