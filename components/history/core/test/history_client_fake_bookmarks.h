// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_TEST_HISTORY_CLIENT_FAKE_BOOKMARKS_H_
#define COMPONENTS_HISTORY_CORE_TEST_HISTORY_CLIENT_FAKE_BOOKMARKS_H_

#include <map>

#include "components/history/core/browser/history_client.h"

namespace history {

// The class HistoryClientFakeBookmarks implements HistoryClient faking the
// methods relating to bookmarks for unit testing.
class HistoryClientFakeBookmarks : public HistoryClient {
 public:
  HistoryClientFakeBookmarks();
  virtual ~HistoryClientFakeBookmarks();

  void ClearAllBookmarks();
  void AddBookmark(const GURL& url);
  void AddBookmarkWithTitle(const GURL& url, const base::string16& title);
  void DelBookmark(const GURL& url);

  // HistoryClient:
  virtual bool IsBookmarked(const GURL& url) OVERRIDE;
  virtual void GetBookmarks(std::vector<URLAndTitle>* bookmarks) OVERRIDE;

 private:
  std::map<GURL, base::string16> bookmarks_;

  DISALLOW_COPY_AND_ASSIGN(HistoryClientFakeBookmarks);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_TEST_HISTORY_CLIENT_FAKE_BOOKMARKS_H_
