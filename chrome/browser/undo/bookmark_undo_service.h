// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNDO_BOOKMARK_UNDO_SERVICE_H_
#define CHROME_BROWSER_UNDO_BOOKMARK_UNDO_SERVICE_H_

#include <map>

#include "chrome/browser/undo/bookmark_renumber_observer.h"
#include "chrome/browser/undo/undo_manager.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
namespace {
class BookmarkUndoOperation;
}  // namespace

// BookmarkUndoService --------------------------------------------------------

// BookmarkUndoService is owned by the profile, and is responsible for observing
// BookmarkModel changes in order to provide an undo for those changes.
class BookmarkUndoService : public bookmarks::BaseBookmarkModelObserver,
                            public KeyedService,
                            public BookmarkRenumberObserver {
 public:
  explicit BookmarkUndoService(Profile* profile);
  ~BookmarkUndoService() override;

  UndoManager* undo_manager() { return &undo_manager_; }

 private:
  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override {}
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         int old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         int new_index) override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         int index) override;
  void OnWillRemoveBookmarks(bookmarks::BookmarkModel* model,
                             const bookmarks::BookmarkNode* parent,
                             int old_index,
                             const bookmarks::BookmarkNode* node) override;
  void OnWillRemoveAllUserBookmarks(bookmarks::BookmarkModel* model) override;
  void OnWillChangeBookmarkNode(bookmarks::BookmarkModel* model,
                                const bookmarks::BookmarkNode* node) override;
  void OnWillReorderBookmarkNode(bookmarks::BookmarkModel* model,
                                 const bookmarks::BookmarkNode* node) override;
  void GroupedBookmarkChangesBeginning(
      bookmarks::BookmarkModel* model) override;
  void GroupedBookmarkChangesEnded(bookmarks::BookmarkModel* model) override;

  // BookmarkRenumberObserver:
  void OnBookmarkRenumbered(int64 old_id, int64 new_id) override;

  Profile* profile_;
  UndoManager undo_manager_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkUndoService);
};

#endif  // CHROME_BROWSER_UNDO_BOOKMARK_UNDO_SERVICE_H_
