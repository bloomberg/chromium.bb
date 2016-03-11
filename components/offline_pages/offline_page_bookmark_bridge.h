// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_BOOKMARK_BRIDGE_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_BOOKMARK_BRIDGE_H_

#include <set>

#include "components/bookmarks/browser/base_bookmark_model_observer.h"

namespace offline_pages {

class OfflinePageModel;

// A bridge that adapts the bookmark observer to the OfflinePageModel
class OfflinePageBookmarkBridge : public bookmarks::BaseBookmarkModelObserver {
 public:
  // constructor, does not take ownership of either pointer.
  OfflinePageBookmarkBridge(OfflinePageModel* model,
                            bookmarks::BookmarkModel* bookmark_model);

  // Overridden from bookmarks::BaseBookmarkModelObserver
  void BookmarkModelChanged() override;
  // Invoked from the destructor of the BookmarkModel.
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;

  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         int index) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           int old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;

 private:
  // Not owned.
  OfflinePageModel* offline_model_;
  bookmarks::BookmarkModel* bookmark_model_;
  DISALLOW_COPY_AND_ASSIGN(OfflinePageBookmarkBridge);
};
}  // namespace offline_pages
#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_BOOKMARK_BRIDGE_H_
