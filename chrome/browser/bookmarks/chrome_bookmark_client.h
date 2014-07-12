// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_
#define CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_

#include <vector>

#include "base/deferred_sequenced_task_runner.h"
#include "base/macros.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/policy/core/browser/managed_bookmarks_tracker.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class BookmarkModel;
class Profile;

class ChromeBookmarkClient : public bookmarks::BookmarkClient,
                             public content::NotificationObserver,
                             public BaseBookmarkModelObserver {
 public:
  explicit ChromeBookmarkClient(Profile* profile);
  virtual ~ChromeBookmarkClient();

  void Init(BookmarkModel* model);

  // KeyedService:
  virtual void Shutdown() OVERRIDE;

  // Returns the managed_node.
  const BookmarkNode* managed_node() { return managed_node_; }

  // Returns true if the given node belongs to the managed bookmarks tree.
  bool IsDescendantOfManagedNode(const BookmarkNode* node);

  // Returns true if there is at least one managed node in the |list|.
  bool HasDescendantsOfManagedNode(
      const std::vector<const BookmarkNode*>& list);

  // bookmarks::BookmarkClient:
  virtual bool PreferTouchIcon() OVERRIDE;
  virtual base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::IconType type,
      const favicon_base::FaviconImageCallback& callback,
      base::CancelableTaskTracker* tracker) OVERRIDE;
  virtual bool SupportsTypedCountForNodes() OVERRIDE;
  virtual void GetTypedCountForNodes(
      const NodeSet& nodes,
      NodeTypedCountPairs* node_typed_count_pairs) OVERRIDE;
  virtual bool IsPermanentNodeVisible(
      const BookmarkPermanentNode* node) OVERRIDE;
  virtual void RecordAction(const base::UserMetricsAction& action) OVERRIDE;
  virtual bookmarks::LoadExtraCallback GetLoadExtraNodesCallback() OVERRIDE;
  virtual bool CanSetPermanentNodeTitle(
      const BookmarkNode* permanent_node) OVERRIDE;
  virtual bool CanSyncNode(const BookmarkNode* node) OVERRIDE;
  virtual bool CanBeEditedByUser(const BookmarkNode* node) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node,
                                   const std::set<GURL>& removed_urls) OVERRIDE;
  virtual void BookmarkAllUserNodesRemoved(
      BookmarkModel* model,
      const std::set<GURL>& removed_urls) OVERRIDE;
  virtual void BookmarkModelLoaded(BookmarkModel* model,
                                   bool ids_reassigned) OVERRIDE;

  // Helper for GetLoadExtraNodesCallback().
  static bookmarks::BookmarkPermanentNodeList LoadExtraNodes(
      scoped_ptr<BookmarkPermanentNode> managed_node,
      scoped_ptr<base::ListValue> initial_managed_bookmarks,
      int64* next_node_id);

  // Returns the management domain that configured the managed bookmarks,
  // or an empty string.
  std::string GetManagedBookmarksDomain();

  Profile* profile_;

  content::NotificationRegistrar registrar_;

  // Pointer to the BookmarkModel. Will be non-NULL from the call to Init to
  // the call to Shutdown. Must be valid for the whole interval.
  BookmarkModel* model_;

  scoped_ptr<policy::ManagedBookmarksTracker> managed_bookmarks_tracker_;
  BookmarkPermanentNode* managed_node_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBookmarkClient);
};

#endif  // CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_
