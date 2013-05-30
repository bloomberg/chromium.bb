// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_TITLE_MATCH_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_TITLE_MATCH_H_

#include <stddef.h>

#include <utility>
#include <vector>

class BookmarkNode;

struct BookmarkTitleMatch {
  // Each MatchPosition is the [begin, end) positions of a match within a
  // string.
  typedef std::pair<size_t, size_t> MatchPosition;
  typedef std::vector<MatchPosition> MatchPositions;

  BookmarkTitleMatch();
  ~BookmarkTitleMatch();

  // The matching node of a query.
  const BookmarkNode* node;

  // Location of the matching words in the title of the node.
  MatchPositions match_positions;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_TITLE_MATCH_H_
