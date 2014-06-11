// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/bookmarks/bookmark_model_observer_for_cocoa.h"

BookmarkModelObserverForCocoa::BookmarkModelObserverForCocoa(
    BookmarkModel* model,
    ChangeCallback callback) {
  DCHECK(model);
  callback_.reset(Block_copy(callback));
  model_ = model;
  model_->AddObserver(this);
}

BookmarkModelObserverForCocoa::~BookmarkModelObserverForCocoa() {
  model_->RemoveObserver(this);
}

void BookmarkModelObserverForCocoa::StartObservingNode(
    const BookmarkNode* node) {
  nodes_.insert(node);
}

void BookmarkModelObserverForCocoa::StopObservingNode(
    const BookmarkNode* node) {
  nodes_.erase(node);
}

void BookmarkModelObserverForCocoa::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  Notify(YES);
}

void BookmarkModelObserverForCocoa::BookmarkNodeMoved(
    BookmarkModel* model,
    const BookmarkNode* old_parent,
    int old_index,
    const BookmarkNode* new_parent,
    int new_index) {
  // Editors often have a tree of parents, so movement of folders
  // must cause a cancel.
  Notify(NO);
}

void BookmarkModelObserverForCocoa::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  // See comment in BookmarkNodeMoved.
  Notify(YES);
}

void BookmarkModelObserverForCocoa::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  Notify(YES);
}

void BookmarkModelObserverForCocoa::BookmarkNodeChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
  if (!nodes_.size() || nodes_.find(node) != nodes_.end())
    Notify(NO);
}

void BookmarkModelObserverForCocoa::Notify(BOOL deleted) {
  callback_.get()(deleted);
}
