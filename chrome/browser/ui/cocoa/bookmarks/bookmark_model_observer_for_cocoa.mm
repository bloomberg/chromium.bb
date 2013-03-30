// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/bookmarks/bookmark_model_observer_for_cocoa.h"

BookmarkModelObserverForCocoa::BookmarkModelObserverForCocoa(
    const BookmarkNode* node,
    BookmarkModel* model,
    NSObject* object,
    SEL selector) {
  DCHECK(model);
  node_ = node;
  model_ = model;
  object_ = object;
  selector_ = selector;
  model_->AddObserver(this);
}

BookmarkModelObserverForCocoa::~BookmarkModelObserverForCocoa() {
  model_->RemoveObserver(this);
}

void BookmarkModelObserverForCocoa::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  Notify();
}

void BookmarkModelObserverForCocoa::BookmarkNodeMoved(
    BookmarkModel* model,
    const BookmarkNode* old_parent,
    int old_index,
    const BookmarkNode* new_parent,
    int new_index) {
  // Editors often have a tree of parents, so movement of folders
  // must cause a cancel.
  Notify();
}

void BookmarkModelObserverForCocoa::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int old_index,
    const BookmarkNode* node) {
  // See comment in BookmarkNodeMoved.
  Notify();
}

void BookmarkModelObserverForCocoa::BookmarkAllNodesRemoved(
    BookmarkModel* model) {
  Notify();
}

void BookmarkModelObserverForCocoa::BookmarkNodeChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
  if ((node_ == node) || (!node_))
    Notify();
}

void BookmarkModelObserverForCocoa::ExtensiveBookmarkChangesBeginning(
    BookmarkModel* model) {
  // Be conservative.
  Notify();
}

void BookmarkModelObserverForCocoa::Notify() {
  [object_ performSelector:selector_ withObject:nil];
}
