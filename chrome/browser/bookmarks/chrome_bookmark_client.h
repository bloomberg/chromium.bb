// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_
#define CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_

#include <set>
#include <vector>

#include "base/deferred_sequenced_task_runner.h"
#include "base/macros.h"
#include "components/bookmarks/browser/bookmark_client.h"

class GURL;
class Profile;

namespace base {
class ListValue;
}

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
class BookmarkPermanentNode;
class ManagedBookmarkService;
}

class ChromeBookmarkClient : public bookmarks::BookmarkClient {
 public:
  ChromeBookmarkClient(
      Profile* profile,
      bookmarks::ManagedBookmarkService* managed_bookmark_service);
  ~ChromeBookmarkClient() override;

  // bookmarks::BookmarkClient:
  void Init(bookmarks::BookmarkModel* model) override;
  bool PreferTouchIcon() override;
  base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::IconType type,
      const favicon_base::FaviconImageCallback& callback,
      base::CancelableTaskTracker* tracker) override;
  bool SupportsTypedCountForNodes() override;
  void GetTypedCountForNodes(
      const NodeSet& nodes,
      NodeTypedCountPairs* node_typed_count_pairs) override;
  bool IsPermanentNodeVisible(
      const bookmarks::BookmarkPermanentNode* node) override;
  void RecordAction(const base::UserMetricsAction& action) override;
  bookmarks::LoadExtraCallback GetLoadExtraNodesCallback() override;
  bool CanSetPermanentNodeTitle(
      const bookmarks::BookmarkNode* permanent_node) override;
  bool CanSyncNode(const bookmarks::BookmarkNode* node) override;
  bool CanBeEditedByUser(const bookmarks::BookmarkNode* node) override;

 private:
  // Pointer to the associated Profile. Must outlive ChromeBookmarkClient.
  Profile* profile_;

  // Pointer to the ManagedBookmarkService responsible for bookmark policy. May
  // be null during testing.
  bookmarks::ManagedBookmarkService* managed_bookmark_service_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBookmarkClient);
};

#endif  // CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_
