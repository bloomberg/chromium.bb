// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_
#define CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_

#include "base/macros.h"
#include "components/history/core/browser/history_client.h"

class BookmarkModel;

// This class implements history::HistoryClient to abstract operations that
// depend on Chrome environment.
class ChromeHistoryClient : public history::HistoryClient {
 public:
  explicit ChromeHistoryClient(BookmarkModel* bookmark_model);

  // history::HistoryClient:
  virtual void BlockUntilBookmarksLoaded() OVERRIDE;
  virtual bool IsBookmarked(const GURL& url) OVERRIDE;
  virtual void GetBookmarks(
      std::vector<history::URLAndTitle>* bookmarks) OVERRIDE;
  virtual void NotifyProfileError(sql::InitStatus init_status) OVERRIDE;
  virtual bool ShouldReportDatabaseError() OVERRIDE;

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

 private:
  // The BookmarkModel, this should outlive ChromeHistoryClient.
  BookmarkModel* bookmark_model_;

  DISALLOW_COPY_AND_ASSIGN(ChromeHistoryClient);
};

#endif  // CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_
