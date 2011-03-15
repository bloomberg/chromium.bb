// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"

void BaseBookmarkModelObserver::Loaded(BookmarkModel* model) {
}

void BaseBookmarkModelObserver::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeMoved(
    BookmarkModel* model,
    const BookmarkNode* old_parent,
    int old_index,
    const BookmarkNode* new_parent,
    int new_index) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeAdded(BookmarkModel* model,
                                                  const BookmarkNode* parent,
                                                  int index) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeRemoved(BookmarkModel* model,
                                                    const BookmarkNode* parent,
                                                    int old_index,
                                                    const BookmarkNode* node) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeChanged(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  BookmarkModelChanged();
}

void BaseBookmarkModelObserver::BookmarkNodeFaviconLoaded(
    BookmarkModel* model,
    const BookmarkNode* node) {
}

void BaseBookmarkModelObserver::BookmarkNodeChildrenReordered(
    BookmarkModel* model,
    const BookmarkNode* node) {
  BookmarkModelChanged();
}
