// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_
#define CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_

#include "base/macros.h"
#include "components/history/core/browser/history_client.h"

class HistoryService;
class Profile;

namespace bookmarks {
class BookmarkModel;
}

// This class implements history::HistoryClient to abstract operations that
// depend on Chrome environment.
class ChromeHistoryClient : public history::HistoryClient {
 public:
  explicit ChromeHistoryClient(bookmarks::BookmarkModel* bookmark_model);
  ~ChromeHistoryClient() override;

  // history::HistoryClient:
  void BlockUntilBookmarksLoaded() override;
  bool IsBookmarked(const GURL& url) override;
  void GetBookmarks(std::vector<history::URLAndTitle>* bookmarks) override;
  bool CanAddURL(const GURL& url) override;
  void NotifyProfileError(sql::InitStatus init_status) override;
  bool ShouldReportDatabaseError() override;
#if defined(OS_ANDROID)
  void OnHistoryBackendInitialized(
      history::HistoryBackend* history_backend,
      history::HistoryDatabase* history_database,
      history::ThumbnailDatabase* thumbnail_database,
      const base::FilePath& history_dir) override;
  void OnHistoryBackendDestroyed(history::HistoryBackend* history_backend,
                                 const base::FilePath& history_dir) override;
#endif

  // KeyedService:
  void Shutdown() override;

 private:
  // The BookmarkModel, this should outlive ChromeHistoryClient.
  bookmarks::BookmarkModel* bookmark_model_;

  DISALLOW_COPY_AND_ASSIGN(ChromeHistoryClient);
};

#endif  // CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_
