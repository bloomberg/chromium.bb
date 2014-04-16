// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_CORE_BROWSER_BOOKMARK_TITLE_MATCH_H_
#define COMPONENTS_BOOKMARKS_CORE_BROWSER_BOOKMARK_TITLE_MATCH_H_

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

#endif  // COMPONENTS_BOOKMARKS_CORE_BROWSER_BOOKMARK_TITLE_MATCH_H_
