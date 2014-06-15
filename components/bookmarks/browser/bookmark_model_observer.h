// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_OBSERVER_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_OBSERVER_H_

#include <set>

class BookmarkModel;
class BookmarkNode;
class GURL;

// Observer for the BookmarkModel.
class BookmarkModelObserver {
 public:
  // Invoked when the model has finished loading. |ids_reassigned| mirrors
  // that of BookmarkLoadDetails::ids_reassigned. See it for details.
  virtual void BookmarkModelLoaded(BookmarkModel* model,
                                   bool ids_reassigned) = 0;

  // Invoked from the destructor of the BookmarkModel.
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) {}

  // Invoked when a node has moved.
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) = 0;

  // Invoked when a node has been added.
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) = 0;

  // Invoked before a node is removed.
  // |parent| the parent of the node that will be removed.
  // |old_index| the index of the node about to be removed in |parent|.
  // |node| is the node to be removed.
  virtual void OnWillRemoveBookmarks(BookmarkModel* model,
                                     const BookmarkNode* parent,
                                     int old_index,
                                     const BookmarkNode* node) {}

  // Invoked when a node has been removed, the item may still be starred though.
  // |parent| the parent of the node that was removed.
  // |old_index| the index of the removed node in |parent| before it was
  // removed.
  // |node| is the node that was removed.
  // |removed_urls| is populated with the urls which no longer have any
  // bookmarks associated with them.
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node,
                                   const std::set<GURL>& removed_urls) = 0;

  // Invoked before the title or url of a node is changed.
  virtual void OnWillChangeBookmarkNode(BookmarkModel* model,
                                        const BookmarkNode* node) {}

  // Invoked when the title or url of a node changes.
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) = 0;

  // Invoked before the metainfo of a node is changed.
  virtual void OnWillChangeBookmarkMetaInfo(BookmarkModel* model,
                                            const BookmarkNode* node) {}

  // Invoked when the metainfo on a node changes.
  virtual void BookmarkMetaInfoChanged(BookmarkModel* model,
                                       const BookmarkNode* node) {}

  // Invoked when a favicon has been loaded or changed.
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) = 0;

  // Invoked before the direct children of |node| have been reordered in some
  // way, such as sorted.
  virtual void OnWillReorderBookmarkNode(BookmarkModel* model,
                                         const BookmarkNode* node) {}

  // Invoked when the children (just direct children, not descendants) of
  // |node| have been reordered in some way, such as sorted.
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) = 0;

  // Invoked before an extensive set of model changes is about to begin.
  // This tells UI intensive observers to wait until the updates finish to
  // update themselves.
  // These methods should only be used for imports and sync.
  // Observers should still respond to BookmarkNodeRemoved immediately,
  // to avoid holding onto stale node pointers.
  virtual void ExtensiveBookmarkChangesBeginning(BookmarkModel* model) {}

  // Invoked after an extensive set of model changes has ended.
  // This tells observers to update themselves if they were waiting for the
  // update to finish.
  virtual void ExtensiveBookmarkChangesEnded(BookmarkModel* model) {}

  // Invoked before all non-permanent bookmark nodes that are editable by
  // the user are removed.
  virtual void OnWillRemoveAllUserBookmarks(BookmarkModel* model) {}

  // Invoked when all non-permanent bookmark nodes that are editable by the
  // user have been removed.
  // |removed_urls| is populated with the urls which no longer have any
  // bookmarks associated with them.
  virtual void BookmarkAllUserNodesRemoved(
      BookmarkModel* model,
      const std::set<GURL>& removed_urls) = 0;

  // Invoked before a set of model changes that is initiated by a single user
  // action. For example, this is called a single time when pasting from the
  // clipboard before each pasted bookmark is added to the bookmark model.
  virtual void GroupedBookmarkChangesBeginning(BookmarkModel* model) {}

  // Invoked after a set of model changes triggered by a single user action has
  // ended.
  virtual void GroupedBookmarkChangesEnded(BookmarkModel* model) {}

 protected:
  virtual ~BookmarkModelObserver() {}
};

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_OBSERVER_H_
