// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_TAG_MODEL_OBSERVER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_TAG_MODEL_OBSERVER_H_

class BookmarkTagModel;
class BookmarkNode;

// Observer for the BookmarkTagModel.
class BookmarkTagModelObserver {
 public:
  // Invoked when the model has finished loading.
  virtual void Loaded(BookmarkTagModel* model) = 0;

  // Invoked from the destructor of the BookmarkTagModel.
  virtual void BookmarkTagModelBeingDeleted(BookmarkTagModel* model) {}

  // Invoked when a node has been added.
  virtual void BookmarkNodeAdded(BookmarkTagModel* model,
                                 const BookmarkNode* bookmark) = 0;

  // Invoked before a node is removed.
  // |node| is the node to be removed.
  virtual void OnWillRemoveBookmarks(BookmarkTagModel* model,
                                     const BookmarkNode* bookmark) {}

  // Invoked when a node has been removed, |node| is the node that was removed.
  virtual void BookmarkNodeRemoved(BookmarkTagModel* model,
                                   const BookmarkNode* bookmark) = 0;

  // Invoked before the title or url of a node is changed.
  virtual void OnWillChangeBookmarkNode(BookmarkTagModel* model,
                                        const BookmarkNode* bookmark) {}

  // Invoked when the title or url of a node changes.
  virtual void BookmarkNodeChanged(BookmarkTagModel* model,
                                   const BookmarkNode* bookmark) = 0;

  // Invoked before changing the tags of a node.
  virtual void OnWillChangeBookmarkTags(BookmarkTagModel* model,
                                        const BookmarkNode* bookmark) {}

  // Invoked when tags are changed on a bookmark.
  virtual void BookmarkTagsChanged(BookmarkTagModel* model,
                                   const BookmarkNode* bookmark) = 0;

  // Invoked when a favicon has been loaded or changed.
  virtual void BookmarkNodeFaviconChanged(BookmarkTagModel* model,
                                          const BookmarkNode* node) = 0;

  // Invoked before an extensive set of model changes is about to begin.
  // This tells UI intensive observers to wait until the updates finish to
  // update themselves.
  // These methods should only be used for imports and sync.
  // Observers should still respond to BookmarkNodeRemoved immediately,
  // to avoid holding onto stale node pointers.
  virtual void ExtensiveBookmarkChangesBeginning(BookmarkTagModel* model) {}

  // Invoked after an extensive set of model changes has ended.
  // This tells observers to update themselves if they were waiting for the
  // update to finish.
  virtual void ExtensiveBookmarkChangesEnded(BookmarkTagModel* model) {}

  // Invoked before all non-permanent bookmark nodes are removed.
  virtual void OnWillRemoveAllBookmarks(BookmarkTagModel* model) {}

  // Invoked when all non-permanent bookmark nodes have been removed.
  virtual void BookmarkAllNodesRemoved(BookmarkTagModel* model) = 0;

 protected:
  virtual ~BookmarkTagModelObserver() {}
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_TAG_MODEL_OBSERVER_H_
