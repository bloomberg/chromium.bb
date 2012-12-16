// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmark_api_helpers.h"

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_constants.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace keys = bookmark_api_constants;
using api::bookmarks::BookmarkTreeNode;

namespace bookmark_api_helpers {

class ExtensionBookmarksTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    model_.reset(new BookmarkModel(NULL));
    model_->AddURL(model_->other_node(), 0, ASCIIToUTF16("Digg"),
                     GURL("http://www.reddit.com"));
    model_->AddURL(model_->other_node(), 0, ASCIIToUTF16("News"),
                     GURL("http://www.foxnews.com"));
    folder_ = model_->AddFolder(
        model_->other_node(), 0, ASCIIToUTF16("outer folder"));
    model_->AddFolder(folder_, 0, ASCIIToUTF16("inner folder 1"));
    model_->AddFolder(folder_, 0, ASCIIToUTF16("inner folder 2"));
    model_->AddURL(folder_, 0, ASCIIToUTF16("Digg"), GURL("http://reddit.com"));
    model_->AddURL(folder_, 0, ASCIIToUTF16("CNet"), GURL("http://cnet.com"));
  }

  scoped_ptr<BookmarkModel> model_;
  const BookmarkNode* folder_;
};
TEST_F(ExtensionBookmarksTest, GetFullTreeFromRoot) {
  scoped_ptr<BookmarkTreeNode> tree(
      GetBookmarkTreeNode(model_->other_node(),
                          true,    // Recurse.
                          false));  // Not only folders.
  ASSERT_EQ(3U, tree->children->size());
}

TEST_F(ExtensionBookmarksTest, GetFoldersOnlyFromRoot) {
  scoped_ptr<BookmarkTreeNode> tree(
      GetBookmarkTreeNode(model_->other_node(),
                          true,   // Recurse.
                          true));  // Only folders.
  ASSERT_EQ(1U, tree->children->size());
}

TEST_F(ExtensionBookmarksTest, GetSubtree) {
  scoped_ptr<BookmarkTreeNode> tree(
      GetBookmarkTreeNode(folder_,
                          true,    // Recurse.
                          false));  // Not only folders.
  ASSERT_EQ(4U, tree->children->size());
  linked_ptr<BookmarkTreeNode> digg = tree->children->at(1);
  ASSERT_TRUE(digg.get());
  ASSERT_EQ("Digg", digg->title);
}

TEST_F(ExtensionBookmarksTest, GetSubtreeFoldersOnly) {
  scoped_ptr<BookmarkTreeNode> tree(
      GetBookmarkTreeNode(folder_,
                          true,   // Recurse.
                          true));  // Only folders.
  ASSERT_EQ(2U, tree->children->size());
  linked_ptr<BookmarkTreeNode> inner_folder = tree->children->at(1);
  ASSERT_TRUE(inner_folder.get());
  ASSERT_EQ("inner folder 1", inner_folder->title);
}

}  // namespace bookmark_api_helpers
}  // namespace extensions
