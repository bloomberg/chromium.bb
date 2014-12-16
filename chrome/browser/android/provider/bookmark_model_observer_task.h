// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PROVIDER_BOOKMARK_MODEL_OBSERVER_TASK_H_
#define CHROME_BROWSER_ANDROID_PROVIDER_BOOKMARK_MODEL_OBSERVER_TASK_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"

// Base class for synchronous tasks that involve the bookmark model.
// Ensures the model has been loaded before accessing it.
// Must not be created from the UI thread.
class BookmarkModelTask {
 public:
  explicit BookmarkModelTask(BookmarkModel* model);
  BookmarkModel* model() const;

 private:
  BookmarkModel* model_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelTask);
};

// Base class for bookmark model tasks that observe for model updates.
class BookmarkModelObserverTask : public BookmarkModelTask,
                                  public bookmarks::BookmarkModelObserver {
 public:
  explicit BookmarkModelObserverTask(BookmarkModel* bookmark_model);
  virtual ~BookmarkModelObserverTask();

  // bookmarks::BookmarkModelObserver:
  virtual void BookmarkModelLoaded(BookmarkModel* model,
                                   bool ids_reassigned) override;
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) override;
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) override;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node,
                                   const std::set<GURL>& removed_urls) override;
  virtual void BookmarkAllUserNodesRemoved(
      BookmarkModel* model,
      const std::set<GURL>& removed_urls) override;
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) override;
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) override;
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkModelObserverTask);
};

#endif  // CHROME_BROWSER_ANDROID_PROVIDER_BOOKMARK_MODEL_OBSERVER_TASK_H_
