// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_
#define CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_

#include <set>
#include <vector>

#include "base/callback_list.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/macros.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/policy/core/browser/managed_bookmarks_tracker.h"

class GURL;
class HistoryServiceFactory;
class Profile;

namespace bookmarks {
class BookmarkModel;
}

namespace history {
class HistoryService;
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
  friend class HistoryServiceFactory;
  void SetHistoryService(history::HistoryService* history_service);

  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           int old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;
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

  // HistoryService associated to the Profile. Due to circular dependency, this
  // cannot be passed to the constructor, nor lazily fetched. Instead the value
  // is initialized from HistoryServiceFactory.
  history::HistoryService* history_service_;

  scoped_ptr<base::CallbackList<void(const std::set<GURL>&)>::Subscription>
      favicon_changed_subscription_;

  // Pointer to the BookmarkModel. Will be non-NULL from the call to Init to
  // the call to Shutdown. Must be valid for the whole interval.
  bookmarks::BookmarkModel* model_;

  // Managed bookmarks are defined by an enterprise policy.
  scoped_ptr<policy::ManagedBookmarksTracker> managed_bookmarks_tracker_;
  // The top-level managed bookmarks folder.
  bookmarks::BookmarkPermanentNode* managed_node_;

  // Supervised bookmarks are defined by the custodian of a supervised user.
  scoped_ptr<policy::ManagedBookmarksTracker> supervised_bookmarks_tracker_;
  // The top-level supervised bookmarks folder.
  bookmarks::BookmarkPermanentNode* supervised_node_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBookmarkClient);
};

#endif  // CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_
