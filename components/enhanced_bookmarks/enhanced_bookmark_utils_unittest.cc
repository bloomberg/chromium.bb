// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const GURL bookmark_url("http://example.com/index.html");

class EnhancedBookmarkUtilsTest : public testing::Test {
 public:
  EnhancedBookmarkUtilsTest() {}
  virtual ~EnhancedBookmarkUtilsTest() {}

 protected:
  DISALLOW_COPY_AND_ASSIGN(EnhancedBookmarkUtilsTest);

  // Adds a bookmark as the subnode at index 0 to other_node.
  // |name| should be ASCII encoded.
  // Returns the newly added bookmark.
  const BookmarkNode* AddBookmark(BookmarkModel* model, std::string name) {
    return model->AddURL(model->other_node(),
                         0,  // index.
                         base::ASCIIToUTF16(name),
                         bookmark_url);
  }
};

TEST_F(EnhancedBookmarkUtilsTest, TestBookmarkSearch) {
  test::TestBookmarkClient bookmark_client;
  scoped_ptr<BookmarkModel> bookmark_model(bookmark_client.CreateModel(false));
  const BookmarkNode* node1 = AddBookmark(bookmark_model.get(), "john hopkins");
  const BookmarkNode* node2 = AddBookmark(bookmark_model.get(), "JohN hopkins");
  const BookmarkNode* node3 = AddBookmark(bookmark_model.get(), "katy perry");
  const BookmarkNode* node4 = AddBookmark(bookmark_model.get(), "lil'john13");
  const BookmarkNode* node5 = AddBookmark(bookmark_model.get(), "jo hn");

  std::vector<const BookmarkNode*> result =
      enhanced_bookmark_utils::FindBookmarksWithQuery(bookmark_model.get(),
                                                      "john");
  ASSERT_EQ(result.size(), 3u);

  CHECK(std::find(result.begin(), result.end(), node1) != result.end());
  CHECK(std::find(result.begin(), result.end(), node2) != result.end());
  CHECK(std::find(result.begin(), result.end(), node4) != result.end());

  CHECK(std::find(result.begin(), result.end(), node3) == result.end());
  CHECK(std::find(result.begin(), result.end(), node5) == result.end());
};

}  // namespace
