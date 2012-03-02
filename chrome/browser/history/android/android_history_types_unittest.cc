// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/android_history_types.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace history {

TEST(AndroidHistoryTypesTest, TestGetBookmarkColumnID) {
  EXPECT_EQ(BookmarkRow::ID, BookmarkRow::GetBookmarkColumnID("_id"));
  EXPECT_EQ(BookmarkRow::URL, BookmarkRow::GetBookmarkColumnID("url"));
  EXPECT_EQ(BookmarkRow::TITLE, BookmarkRow::GetBookmarkColumnID("title"));
  EXPECT_EQ(BookmarkRow::CREATED, BookmarkRow::GetBookmarkColumnID("created"));
  EXPECT_EQ(BookmarkRow::LAST_VISIT_TIME,
            BookmarkRow::GetBookmarkColumnID("date"));
  EXPECT_EQ(BookmarkRow::VISIT_COUNT,
            BookmarkRow::GetBookmarkColumnID("visits"));
  EXPECT_EQ(BookmarkRow::FAVICON, BookmarkRow::GetBookmarkColumnID("favicon"));
  EXPECT_EQ(BookmarkRow::BOOKMARK,
            BookmarkRow::GetBookmarkColumnID("bookmark"));
  EXPECT_EQ(BookmarkRow::RAW_URL, BookmarkRow::GetBookmarkColumnID("raw_url"));
}

TEST(AndroidHistoryTypesTest, TestGetSearchColumnID) {
  EXPECT_EQ(SearchRow::ID, SearchRow::GetSearchColumnID("_id"));
  EXPECT_EQ(SearchRow::SEARCH_TERM, SearchRow::GetSearchColumnID("search"));
  EXPECT_EQ(SearchRow::SEARCH_TIME, SearchRow::GetSearchColumnID("date"));
}

}  // namespace history
