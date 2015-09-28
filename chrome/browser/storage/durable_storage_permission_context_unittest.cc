// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/durable_storage_permission_context.h"

#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkModel;

class BookmarksOriginTest : public ::testing::Test {
 protected:
  static std::vector<BookmarkModel::URLAndTitle> MakeBookmarks(
      const std::string urls[],
      const int array_size) {
    std::vector<BookmarkModel::URLAndTitle> bookmarks;
    for (int i = 0; i < array_size; ++i) {
      BookmarkModel::URLAndTitle bookmark;
      bookmark.url = GURL(urls[i]);
      EXPECT_TRUE(bookmark.url.is_valid());
      bookmarks.push_back(bookmark);
    }
    return bookmarks;
  }
};

TEST_F(BookmarksOriginTest, Exists) {
  std::string urls[] = {
    "http://www.google.com/",
    "https://dogs.com/somepage.html",
    "https://mail.google.com/mail/u/0/#inbox",
  };
  std::vector<BookmarkModel::URLAndTitle> bookmarks =
      MakeBookmarks(urls, arraysize(urls));
  GURL looking_for("https://dogs.com");
  EXPECT_TRUE(DurableStoragePermissionContext::IsOriginBookmarked(
      bookmarks, looking_for));
}

TEST_F(BookmarksOriginTest, DoesntExist) {
  std::string urls[] = {
    "http://www.google.com/",
    "https://www.google.com/",
  };
  std::vector<BookmarkModel::URLAndTitle> bookmarks =
      MakeBookmarks(urls, arraysize(urls));
  GURL looking_for("https://dogs.com");
  EXPECT_FALSE(DurableStoragePermissionContext::IsOriginBookmarked(
      bookmarks, looking_for));
}
