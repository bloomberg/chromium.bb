// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// C++ bridge class to send a selector to a Cocoa object when the
// bookmark model changes.  Some Cocoa objects edit the bookmark model
// and temporarily save a copy of the state (e.g. bookmark button
// editor).  As a fail-safe, these objects want an easy cancel if the
// model changes out from under them.  For example, if you have the
// bookmark button editor sheet open, then edit the bookmark in the
// bookmark manager, we'd want to simply cancel the editor.
//
// This class is conservative and may result in notifications which
// aren't strictly necessary.  For example, node removal only needs to
// cancel an edit if the removed node is a folder (editors often have
// a list of "new parents").  But, just to be sure, notification
// happens on any removal.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MODEL_OBSERVER_FOR_COCOA_H
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MODEL_OBSERVER_FOR_COCOA_H

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"

class BookmarkModelObserverForCocoa : public BookmarkModelObserver {
 public:
  // When |node| in |model| changes, send |selector| to |object|.
  // Assumes |selector| is a selector that takes one arg, like an
  // IBOutlet.  The arg passed is nil.
  // Many notifications happen independently of node
  // (e.g. BeingDeleted), so |node| can be nil.
  //
  // |object| is NOT retained, since the expected use case is for
  // ||object| to own the BookmarkModelObserverForCocoa and we don't
  // want a retain cycle.
  BookmarkModelObserverForCocoa(const BookmarkNode* node,
                                BookmarkModel* model,
                                NSObject* object,
                                SEL selector);
  virtual ~BookmarkModelObserverForCocoa();

  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkAllNodesRemoved(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void ExtensiveBookmarkChangesBeginning(BookmarkModel* model) OVERRIDE;

  // Some notifications we don't care about, but by being pure virtual
  // in the base class we must implement them.
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE {
  }
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE {
  }
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) OVERRIDE {
  }
  virtual void BookmarkNodeChildrenReordered(
      BookmarkModel* model,
      const BookmarkNode* node) OVERRIDE {
  }

  virtual void ExtensiveBookmarkChangesEnded(BookmarkModel* model) OVERRIDE {
  }

 private:
  const BookmarkNode* node_;  // Weak; owned by a BookmarkModel.
  BookmarkModel* model_;  // Weak; it is owned by a Profile.
  NSObject* object_; // Weak, like a delegate.
  SEL selector_;

  void Notify();

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelObserverForCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_MODEL_OBSERVER_FOR_COCOA_H
