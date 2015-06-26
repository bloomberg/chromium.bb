// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_
#define CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_

#include <set>
#include <vector>

#include "base/deferred_sequenced_task_runner.h"
#include "base/macros.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
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
class ManagedBookmarksTracker;
}

class ChromeBookmarkClient : public bookmarks::BookmarkClient,
                             public bookmarks::BaseBookmarkModelObserver {
 public:
  explicit ChromeBookmarkClient(Profile* profile);
  ~ChromeBookmarkClient() override;

  void Init(bookmarks::BookmarkModel* model);

  // KeyedService:
  void Shutdown() override;

  // The top-level managed bookmarks folder, defined by an enterprise policy.
  const bookmarks::BookmarkNode* managed_node() { return managed_node_; }
  // The top-level supervised bookmarks folder, defined by the custodian of a
  // supervised user.
  const bookmarks::BookmarkNode* supervised_node() { return supervised_node_; }

  // bookmarks::BookmarkClient:
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
  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;

  // Helper for GetLoadExtraNodesCallback().
  static bookmarks::BookmarkPermanentNodeList LoadExtraNodes(
      scoped_ptr<bookmarks::BookmarkPermanentNode> managed_node,
      scoped_ptr<base::ListValue> initial_managed_bookmarks,
      scoped_ptr<bookmarks::BookmarkPermanentNode> supervised_node,
      scoped_ptr<base::ListValue> initial_supervised_bookmarks,
      int64* next_node_id);

  // Returns the management domain that configured the managed bookmarks,
  // or an empty string.
  std::string GetManagedBookmarksDomain();

  Profile* profile_;

  // Pointer to the BookmarkModel. Will be non-NULL from the call to Init to
  // the call to Shutdown. Must be valid for the whole interval.
  bookmarks::BookmarkModel* model_;

  // Managed bookmarks are defined by an enterprise policy.
  scoped_ptr<bookmarks::ManagedBookmarksTracker> managed_bookmarks_tracker_;
  // The top-level managed bookmarks folder.
  bookmarks::BookmarkPermanentNode* managed_node_;

  // Supervised bookmarks are defined by the custodian of a supervised user.
  scoped_ptr<bookmarks::ManagedBookmarksTracker> supervised_bookmarks_tracker_;
  // The top-level supervised bookmarks folder.
  bookmarks::BookmarkPermanentNode* supervised_node_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBookmarkClient);
};

#endif  // CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_
