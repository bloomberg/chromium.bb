// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_
#define CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_

#include "base/macros.h"
#include "components/history/core/browser/history_client.h"
#include "components/history/core/browser/top_sites_observer.h"

class HistoryService;
class Profile;

namespace bookmarks {
class BookmarkModel;
}

namespace history {
class TopSites;
}

// This class implements history::HistoryClient to abstract operations that
// depend on Chrome environment.
class ChromeHistoryClient : public history::HistoryClient,
                            public history::TopSitesObserver {
 public:
  explicit ChromeHistoryClient(bookmarks::BookmarkModel* bookmark_model,
                               Profile* profile,
                               history::TopSites* top_sites);
  ~ChromeHistoryClient() override;

  // history::HistoryClient:
  void BlockUntilBookmarksLoaded() override;
  bool IsBookmarked(const GURL& url) override;
  void GetBookmarks(std::vector<history::URLAndTitle>* bookmarks) override;
  void NotifyProfileError(sql::InitStatus init_status) override;
  bool ShouldReportDatabaseError() override;

  // KeyedService:
  void Shutdown() override;

  // TopSitesObserver:
  void TopSitesLoaded(history::TopSites* top_sites) override;
  void TopSitesChanged(history::TopSites* top_sites) override;

 private:
  // The BookmarkModel, this should outlive ChromeHistoryClient.
  bookmarks::BookmarkModel* bookmark_model_;
  Profile* profile_;
  // The TopSites object is owned by the Profile (see
  // chrome/browser/profiles/profile_impl.h)
  // and lazily constructed by the  getter.
  // ChromeHistoryClient is a KeyedService linked to the Profile lifetime by the
  // ChromeHistoryClientFactory (which is a BrowserContextKeyedServiceFactory).
  // Before the Profile is destroyed, all the KeyedService Shutdown methods are
  // called, and the Profile is fully constructed before any of the KeyedService
  // can  be constructed. The TopSites does not use the HistoryService nor the
  // HistoryClient during construction (it uses it later, but supports getting
  // an NULL  pointer).
  history::TopSites* top_sites_;

  DISALLOW_COPY_AND_ASSIGN(ChromeHistoryClient);
};

#endif  // CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_
