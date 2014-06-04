// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/history_client_fake_bookmarks.h"

namespace history {

HistoryClientFakeBookmarks::HistoryClientFakeBookmarks() {
}

HistoryClientFakeBookmarks::~HistoryClientFakeBookmarks() {
}

void HistoryClientFakeBookmarks::ClearAllBookmarks() {
  bookmarks_.clear();
}

void HistoryClientFakeBookmarks::AddBookmark(const GURL& url) {
  AddBookmarkWithTitle(url, base::string16());
}

void HistoryClientFakeBookmarks::AddBookmarkWithTitle(
    const GURL& url,
    const base::string16& title) {
  bookmarks_.insert(std::make_pair(url, title));
}

void HistoryClientFakeBookmarks::DelBookmark(const GURL& url) {
  bookmarks_.erase(url);
}

bool HistoryClientFakeBookmarks::IsBookmarked(const GURL& url) {
  return bookmarks_.find(url) != bookmarks_.end();
}

void HistoryClientFakeBookmarks::GetBookmarks(
    std::vector<URLAndTitle>* bookmarks) {
  bookmarks->reserve(bookmarks->size() + bookmarks_.size());
  typedef std::map<GURL, base::string16>::const_iterator iterator;
  for (iterator i = bookmarks_.begin(); i != bookmarks_.end(); ++i) {
    URLAndTitle urlAndTitle = {i->first, i->second};
    bookmarks->push_back(urlAndTitle);
  }
}

}  // namespace history
