// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_model_observer.h"

void BookmarkModelObserver::OnWillRemoveAllUserBookmarks(BookmarkModel* model) {
  OnWillRemoveAllBookmarks(model);
}

void BookmarkModelObserver::BookmarkAllUserNodesRemoved(
      BookmarkModel* model,
      const std::set<GURL>& removed_urls) {
  BookmarkAllNodesRemoved(model, removed_urls);
}
