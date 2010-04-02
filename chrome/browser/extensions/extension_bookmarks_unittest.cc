// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/extensions/extension_bookmark_helpers.h"
#include "chrome/browser/extensions/extension_bookmarks_module_constants.h"
namespace keys = extension_bookmarks_module_constants;

class ExtensionBookmarksTest : public testing::Test {
 public:
  virtual void SetUp() {
    model_.reset(new BookmarkModel(NULL));
    model_->AddURL(model_->other_node(), 0, L"Digg",
                     GURL("http://www.reddit.com"));
    model_->AddURL(model_->other_node(), 0, L"News",
                     GURL("http://www.foxnews.com"));
    folder =
        model_->AddGroup(model_->other_node(), 0, L"outer folder");
    model_->AddGroup(folder, 0, L"inner folder 1");
    model_->AddGroup(folder, 0, L"inner folder 2");
    model_->AddURL(folder, 0, L"Digg", GURL("http://reddit.com"));
    model_->AddURL(folder, 0, L"CNet", GURL("http://cnet.com"));
  }
  
  scoped_ptr<BookmarkModel> model_;
  const BookmarkNode* folder;
};
TEST_F(ExtensionBookmarksTest, GetFullTreeFromRoot) {
  DictionaryValue* tree =
      extension_bookmark_helpers::GetNodeDictionary(model_->other_node(),
                                                    true, // recurse
                                                    false); // not only folders
  ListValue* children;
  tree->GetList(keys::kChildrenKey, &children);
  ASSERT_EQ(3U, children->GetSize());
}

TEST_F(ExtensionBookmarksTest, GetFoldersOnlyFromRoot) {
  DictionaryValue* tree =
      extension_bookmark_helpers::GetNodeDictionary(model_->other_node(),
                                                    true, // recurse
                                                    true); // only folders
  ListValue* children;
  tree->GetList(keys::kChildrenKey, &children);
  ASSERT_EQ(1U, children->GetSize());
}

TEST_F(ExtensionBookmarksTest, GetSubtree) {
  DictionaryValue* tree =
      extension_bookmark_helpers::GetNodeDictionary(folder,
                                                    true, // recurse
                                                    false); // not only folders
  ListValue* children;
  tree->GetList(keys::kChildrenKey, &children);
  ASSERT_EQ(4U, children->GetSize());
  DictionaryValue* digg;
  children->GetDictionary(1,&digg);
  std::string title;
  digg->GetString(keys::kTitleKey, &title);
  ASSERT_EQ("Digg", title);
}

TEST_F(ExtensionBookmarksTest, GetSubtreeFoldersOnly) {
  DictionaryValue* tree =
      extension_bookmark_helpers::GetNodeDictionary(folder,
                                                    true, // recurse
                                                    true); // only folders
  ListValue* children;
  tree->GetList(keys::kChildrenKey, &children);
  ASSERT_EQ(2U, children->GetSize());
  DictionaryValue* inner_folder;
  children->GetDictionary(1,&inner_folder);
  std::string title;
  inner_folder->GetString(keys::kTitleKey, &title);
  ASSERT_EQ("inner folder 1", title);
}
