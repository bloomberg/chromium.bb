// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_extension_helpers.h"

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_extension_api_constants.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = bookmark_extension_api_constants;

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
  scoped_ptr<DictionaryValue> tree(
      bookmark_extension_helpers::GetNodeDictionary(
          model_->other_node(),
          true,    // Recurse.
          false));  // Not only folders.
  ListValue* children;
  tree->GetList(keys::kChildrenKey, &children);
  ASSERT_EQ(3U, children->GetSize());
}

TEST_F(ExtensionBookmarksTest, GetFoldersOnlyFromRoot) {
  scoped_ptr<DictionaryValue> tree(
      bookmark_extension_helpers::GetNodeDictionary(
          model_->other_node(),
          true,   // Recurse.
          true));  // Only folders.
  ListValue* children;
  tree->GetList(keys::kChildrenKey, &children);
  ASSERT_EQ(1U, children->GetSize());
}

TEST_F(ExtensionBookmarksTest, GetSubtree) {
  scoped_ptr<DictionaryValue> tree(
      bookmark_extension_helpers::GetNodeDictionary(
          folder_,
          true,    // Recurse.
          false));  // Not only folders.
  ListValue* children;
  tree->GetList(keys::kChildrenKey, &children);
  ASSERT_EQ(4U, children->GetSize());
  DictionaryValue* digg;
  ASSERT_TRUE(children->GetDictionary(1, &digg));
  std::string title;
  digg->GetString(keys::kTitleKey, &title);
  ASSERT_EQ("Digg", title);
}

TEST_F(ExtensionBookmarksTest, GetSubtreeFoldersOnly) {
  scoped_ptr<DictionaryValue> tree(
      bookmark_extension_helpers::GetNodeDictionary(
          folder_,
          true,   // Recurse.
          true));  // Only folders.
  ListValue* children;
  tree->GetList(keys::kChildrenKey, &children);
  ASSERT_EQ(2U, children->GetSize());
  DictionaryValue* inner_folder;
  ASSERT_TRUE(children->GetDictionary(1, &inner_folder));
  std::string title;
  inner_folder->GetString(keys::kTitleKey, &title);
  ASSERT_EQ("inner folder 1", title);
}
