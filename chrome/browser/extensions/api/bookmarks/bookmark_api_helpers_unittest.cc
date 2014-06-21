// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmark_api_helpers.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client_factory.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_constants.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace keys = bookmark_api_constants;
using api::bookmarks::BookmarkTreeNode;

namespace bookmark_api_helpers {

class ExtensionBookmarksTest : public testing::Test {
 public:
  ExtensionBookmarksTest()
      : client_(NULL), model_(NULL), node_(NULL), node2_(NULL), folder_(NULL) {}

  virtual void SetUp() OVERRIDE {
    profile_.CreateBookmarkModel(false);
    model_ = BookmarkModelFactory::GetForProfile(&profile_);
    client_ = ChromeBookmarkClientFactory::GetForProfile(&profile_);
    test::WaitForBookmarkModelToLoad(model_);

    node_ = model_->AddURL(model_->other_node(), 0, base::ASCIIToUTF16("Digg"),
                           GURL("http://www.reddit.com"));
    model_->SetNodeMetaInfo(node_, "some_key1", "some_value1");
    model_->SetNodeMetaInfo(node_, "some_key2", "some_value2");
    model_->AddURL(model_->other_node(), 0, base::ASCIIToUTF16("News"),
                   GURL("http://www.foxnews.com"));
    folder_ = model_->AddFolder(
        model_->other_node(), 0, base::ASCIIToUTF16("outer folder"));
    model_->SetNodeMetaInfo(folder_, "some_key1", "some_value1");
    model_->AddFolder(folder_, 0, base::ASCIIToUTF16("inner folder 1"));
    model_->AddFolder(folder_, 0, base::ASCIIToUTF16("inner folder 2"));
    node2_ = model_->AddURL(
        folder_, 0, base::ASCIIToUTF16("Digg"), GURL("http://reddit.com"));
    model_->SetNodeMetaInfo(node2_, "some_key2", "some_value2");
    model_->AddURL(
        folder_, 0, base::ASCIIToUTF16("CNet"), GURL("http://cnet.com"));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  ChromeBookmarkClient* client_;
  BookmarkModel* model_;
  const BookmarkNode* node_;
  const BookmarkNode* node2_;
  const BookmarkNode* folder_;
};

TEST_F(ExtensionBookmarksTest, GetFullTreeFromRoot) {
  scoped_ptr<BookmarkTreeNode> tree(
      GetBookmarkTreeNode(client_,
                          model_->other_node(),
                          true,    // Recurse.
                          false));  // Not only folders.
  ASSERT_EQ(3U, tree->children->size());
}

TEST_F(ExtensionBookmarksTest, GetFoldersOnlyFromRoot) {
  scoped_ptr<BookmarkTreeNode> tree(
      GetBookmarkTreeNode(client_,
                          model_->other_node(),
                          true,   // Recurse.
                          true));  // Only folders.
  ASSERT_EQ(1U, tree->children->size());
}

TEST_F(ExtensionBookmarksTest, GetSubtree) {
  scoped_ptr<BookmarkTreeNode> tree(
      GetBookmarkTreeNode(client_,
                          folder_,
                          true,    // Recurse.
                          false));  // Not only folders.
  ASSERT_EQ(4U, tree->children->size());
  linked_ptr<BookmarkTreeNode> digg = tree->children->at(1);
  ASSERT_TRUE(digg.get());
  ASSERT_EQ("Digg", digg->title);
}

TEST_F(ExtensionBookmarksTest, GetSubtreeFoldersOnly) {
  scoped_ptr<BookmarkTreeNode> tree(
      GetBookmarkTreeNode(client_,
                          folder_,
                          true,   // Recurse.
                          true));  // Only folders.
  ASSERT_EQ(2U, tree->children->size());
  linked_ptr<BookmarkTreeNode> inner_folder = tree->children->at(1);
  ASSERT_TRUE(inner_folder.get());
  ASSERT_EQ("inner folder 1", inner_folder->title);
}

TEST_F(ExtensionBookmarksTest, GetModifiableNode) {
  scoped_ptr<BookmarkTreeNode> tree(
      GetBookmarkTreeNode(client_,
                          node_,
                          false,    // Recurse.
                          false));  // Only folders.
  EXPECT_EQ("Digg", tree->title);
  ASSERT_TRUE(tree->url);
  EXPECT_EQ("http://www.reddit.com/", *tree->url);
  EXPECT_EQ(BookmarkTreeNode::UNMODIFIABLE_NONE, tree->unmodifiable);
}

TEST_F(ExtensionBookmarksTest, GetManagedNode) {
  const BookmarkNode* managed_bookmark =
      model_->AddURL(client_->managed_node(),
                     0,
                     base::ASCIIToUTF16("Chromium"),
                     GURL("http://www.chromium.org/"));
  scoped_ptr<BookmarkTreeNode> tree(
      GetBookmarkTreeNode(client_,
                          managed_bookmark,
                          false,    // Recurse.
                          false));  // Only folders.
  EXPECT_EQ("Chromium", tree->title);
  EXPECT_EQ("http://www.chromium.org/", *tree->url);
  EXPECT_EQ(BookmarkTreeNode::UNMODIFIABLE_MANAGED, tree->unmodifiable);
}

TEST_F(ExtensionBookmarksTest, RemoveNodeInvalidId) {
  int64 invalid_id = model_->next_node_id();
  std::string error;
  EXPECT_FALSE(RemoveNode(model_, client_, invalid_id, true, &error));
  EXPECT_EQ(keys::kNoNodeError, error);
}

TEST_F(ExtensionBookmarksTest, RemoveNodePermanent) {
  std::string error;
  EXPECT_FALSE(
      RemoveNode(model_, client_, model_->other_node()->id(), true, &error));
  EXPECT_EQ(keys::kModifySpecialError, error);
}

TEST_F(ExtensionBookmarksTest, RemoveNodeManaged) {
  const BookmarkNode* managed_bookmark =
      model_->AddURL(client_->managed_node(),
                     0,
                     base::ASCIIToUTF16("Chromium"),
                     GURL("http://www.chromium.org"));
  std::string error;
  EXPECT_FALSE(
      RemoveNode(model_, client_, managed_bookmark->id(), true, &error));
  EXPECT_EQ(keys::kModifyManagedError, error);
}

TEST_F(ExtensionBookmarksTest, RemoveNodeNotRecursive) {
  std::string error;
  EXPECT_FALSE(RemoveNode(model_, client_, folder_->id(), false, &error));
  EXPECT_EQ(keys::kFolderNotEmptyError, error);
}

TEST_F(ExtensionBookmarksTest, RemoveNodeRecursive) {
  EXPECT_EQ(3, model_->other_node()->child_count());
  std::string error;
  EXPECT_TRUE(RemoveNode(model_, client_, folder_->id(), true, &error));
  EXPECT_EQ(2, model_->other_node()->child_count());
}

TEST_F(ExtensionBookmarksTest, GetMetaInfo) {
  base::DictionaryValue id_to_meta_info_map;
  GetMetaInfo(*model_->other_node(), &id_to_meta_info_map);
  EXPECT_EQ(8u, id_to_meta_info_map.size());

  // Verify top level node.
  const base::Value* value = NULL;
  EXPECT_TRUE(id_to_meta_info_map.Get(
      base::Int64ToString(model_->other_node()->id()), &value));
  ASSERT_TRUE(NULL != value);
  const base::DictionaryValue* dictionary_value = NULL;
  EXPECT_TRUE(value->GetAsDictionary(&dictionary_value));
  ASSERT_TRUE(NULL != dictionary_value);
  EXPECT_EQ(0u, dictionary_value->size());

  // Verify bookmark with two meta info key/value pairs.
  value = NULL;
  EXPECT_TRUE(id_to_meta_info_map.Get(
      base::Int64ToString(node_->id()), &value));
  ASSERT_TRUE(NULL != value);
  dictionary_value = NULL;
  EXPECT_TRUE(value->GetAsDictionary(&dictionary_value));
  ASSERT_TRUE(NULL != dictionary_value);
  EXPECT_EQ(2u, dictionary_value->size());
  std::string string_value;
  EXPECT_TRUE(dictionary_value->GetString("some_key1", &string_value));
  EXPECT_EQ("some_value1", string_value);
  EXPECT_TRUE(dictionary_value->GetString("some_key2", &string_value));
  EXPECT_EQ("some_value2", string_value);

  // Verify folder with one meta info key/value pair.
  value = NULL;
  EXPECT_TRUE(id_to_meta_info_map.Get(
      base::Int64ToString(folder_->id()), &value));
  ASSERT_TRUE(NULL != value);
  dictionary_value = NULL;
  EXPECT_TRUE(value->GetAsDictionary(&dictionary_value));
  ASSERT_TRUE(NULL != dictionary_value);
  EXPECT_EQ(1u, dictionary_value->size());
  EXPECT_TRUE(dictionary_value->GetString("some_key1", &string_value));
  EXPECT_EQ("some_value1", string_value);

  // Verify bookmark in a subfolder with one meta info key/value pairs.
  value = NULL;
  EXPECT_TRUE(id_to_meta_info_map.Get(
      base::Int64ToString(node2_->id()), &value));
  ASSERT_TRUE(NULL != value);
  dictionary_value = NULL;
  EXPECT_TRUE(value->GetAsDictionary(&dictionary_value));
  ASSERT_TRUE(NULL != dictionary_value);
  EXPECT_EQ(1u, dictionary_value->size());
  string_value.clear();
  EXPECT_FALSE(dictionary_value->GetString("some_key1", &string_value));
  EXPECT_EQ("", string_value);
  EXPECT_TRUE(dictionary_value->GetString("some_key2", &string_value));
  EXPECT_EQ("some_value2", string_value);

}

}  // namespace bookmark_api_helpers
}  // namespace extensions
