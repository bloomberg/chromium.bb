// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BOOKMARK_LOAD_OBSERVER_H_
#define CHROME_TEST_BOOKMARK_LOAD_OBSERVER_H_
#pragma once

#include "chrome/browser/bookmarks/bookmark_model.h"

// BookmarkLoadObserver is used when blocking until the BookmarkModel
// finishes loading. As soon as the BookmarkModel finishes loading the message
// loop is quit.
class BookmarkLoadObserver : public BookmarkModelObserver {
 public:
  BookmarkLoadObserver();
  virtual ~BookmarkLoadObserver();
  virtual void Loaded(BookmarkModel* model);

  virtual void BookmarkNodeMoved(BookmarkModel* model,
    const BookmarkNode* old_parent,
    int old_index,
    const BookmarkNode* new_parent,
    int new_index) {}
  virtual void BookmarkNodeAdded(BookmarkModel* model,
    const BookmarkNode* parent,
    int index) {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
    const BookmarkNode* parent,
    int old_index,
    const BookmarkNode* node) {}
  virtual void BookmarkNodeChanged(BookmarkModel* model,
    const BookmarkNode* node) {}
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
    const BookmarkNode* node) {}
  virtual void BookmarkNodeFaviconLoaded(BookmarkModel* model,
    const BookmarkNode* node) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkLoadObserver);
};

#endif  // CHROME_TEST_BOOKMARK_LOAD_OBSERVER_H_
