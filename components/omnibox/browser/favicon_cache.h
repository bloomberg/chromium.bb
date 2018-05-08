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

// This caches favicons for pages. We cache a small number of them so we can
// synchronously deliver them to the UI to prevent flicker as the user types.
// It also stores and times out null results from when we cannot fetch a favicon
// from the history database.
class FaviconCache : public history::HistoryServiceObserver {
 public:
  FaviconCache(favicon::FaviconService* favicon_service,
               history::HistoryService* history_service);
  ~FaviconCache() override;

  gfx::Image GetFaviconForPageUrl(const GURL& page_url,
                                  FaviconFetchedCallback on_favicon_fetched);

 private:
  FRIEND_TEST_ALL_PREFIXES(FaviconCacheTest, ClearIconsWithHistoryDeletions);
  FRIEND_TEST_ALL_PREFIXES(FaviconCacheTest, ExpireNullFaviconsByHistory);
  FRIEND_TEST_ALL_PREFIXES(FaviconCacheTest, ExpireNullFaviconsByTime);

  // Chosen arbitrarily. Declared in the class for testing.
  static const int kEmptyFaviconCacheLifetimeInSeconds;

  void OnFaviconFetched(const GURL& page_url,
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
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // Non-owning pointer to a KeyedService.
  favicon::FaviconService* favicon_service_;

  ScopedObserver<history::HistoryService, FaviconCache> history_observer_;

  base::CancelableTaskTracker task_tracker_;
  std::map<GURL, std::list<FaviconFetchedCallback>> pending_requests_;

  base::MRUCache<GURL, gfx::Image> mru_cache_;

  // Keep pages with empty favicons in a separate list, to prevent a page with
  // an empty favicon from ever evicting an existing favicon. The value is used
  // to age out entries that are too old.
  base::MRUCache<GURL, base::TimeTicks> pages_without_favicons_;

  base::WeakPtrFactory<FaviconCache> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FaviconCache);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAVICON_CACHE_H_
