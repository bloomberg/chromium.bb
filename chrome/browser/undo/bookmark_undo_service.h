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
class BookmarkUndoService : public BaseBookmarkModelObserver,
                            public KeyedService,
                            public BookmarkRenumberObserver {
 public:
  explicit BookmarkUndoService(Profile* profile);
  ~BookmarkUndoService() override;

  UndoManager* undo_manager() { return &undo_manager_; }

 private:
  // BaseBookmarkModelObserver:
  void BookmarkModelChanged() override {}
  void BookmarkModelLoaded(BookmarkModel* model, bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(BookmarkModel* model) override;
  void BookmarkNodeMoved(BookmarkModel* model,
                         const BookmarkNode* old_parent,
                         int old_index,
                         const BookmarkNode* new_parent,
                         int new_index) override;
  void BookmarkNodeAdded(BookmarkModel* model,
                         const BookmarkNode* parent,
                         int index) override;
  void OnWillRemoveBookmarks(BookmarkModel* model,
                             const BookmarkNode* parent,
                             int old_index,
                             const BookmarkNode* node) override;
  void OnWillRemoveAllUserBookmarks(BookmarkModel* model) override;
  void OnWillChangeBookmarkNode(BookmarkModel* model,
                                const BookmarkNode* node) override;
  void OnWillReorderBookmarkNode(BookmarkModel* model,
                                 const BookmarkNode* node) override;
  void GroupedBookmarkChangesBeginning(BookmarkModel* model) override;
  void GroupedBookmarkChangesEnded(BookmarkModel* model) override;

  // BookmarkRenumberObserver:
  void OnBookmarkRenumbered(int64 old_id, int64 new_id) override;

  Profile* profile_;
  UndoManager undo_manager_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkUndoService);
};

#endif  // CHROME_BROWSER_UNDO_BOOKMARK_UNDO_SERVICE_H_
