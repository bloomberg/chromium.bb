// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_TEST_BOOKMARK_CLIENT_H_
#define CHROME_BROWSER_BOOKMARKS_TEST_BOOKMARK_CLIENT_H_

#include "base/memory/scoped_ptr.h"
#include "components/bookmarks/core/browser/bookmark_client.h"

class BookmarkModel;

namespace test {

class TestBookmarkClient : public BookmarkClient {
 public:
  TestBookmarkClient() {}
  virtual ~TestBookmarkClient() {}

  // Create a BookmarkModel using this object as its client. The returned
  // BookmarkModel* is owned by the caller.
  scoped_ptr<BookmarkModel> CreateModel(bool index_urls);

  // BookmarkClient:
  virtual base::CancelableTaskTracker::TaskId GetFaviconImageForURL(
      const GURL& page_url,
      int icon_types,
      int desired_size_in_dip,
      const FaviconImageCallback& callback,
      base::CancelableTaskTracker* tracker) OVERRIDE;
  virtual bool SupportsTypedCountForNodes() OVERRIDE;
  virtual void GetTypedCountForNodes(
      const NodeSet& nodes,
      NodeTypedCountPairs* node_typed_count_pairs) OVERRIDE;
  virtual void RecordAction(const base::UserMetricsAction& action) OVERRIDE;
  virtual void NotifyHistoryAboutRemovedBookmarks(
      const std::set<GURL>& removed_bookmark_urls) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBookmarkClient);
};

}  // namespace test

#endif  // CHROME_BROWSER_BOOKMARKS_TEST_BOOKMARK_CLIENT_H_
