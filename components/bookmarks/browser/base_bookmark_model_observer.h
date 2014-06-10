// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BASE_BOOKMARK_MODEL_OBSERVER_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BASE_BOOKMARK_MODEL_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"

// Base class for a BookmarkModelObserver implementation. All mutations of the
// model funnel into the method BookmarkModelChanged.
class BaseBookmarkModelObserver : public BookmarkModelObserver {
 public:
  BaseBookmarkModelObserver() {}

  virtual void BookmarkModelChanged() = 0;

  // BookmarkModelObserver:
  virtual void BookmarkModelLoaded(BookmarkModel* model,
                                   bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) OVERRIDE;
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node,
                                   const std::set<GURL>& removed_urls) OVERRIDE;
  virtual void BookmarkAllUserNodesRemoved(
      BookmarkModel* model,
      const std::set<GURL>& removed_urls) OVERRIDE;
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) OVERRIDE;

 protected:
  virtual ~BaseBookmarkModelObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseBookmarkModelObserver);
};

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BASE_BOOKMARK_MODEL_OBSERVER_H_
