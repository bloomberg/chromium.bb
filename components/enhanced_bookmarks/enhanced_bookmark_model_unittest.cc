// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/enhanced_bookmark_model.h"

#include "base/base64.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/enhanced_bookmarks/proto/metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const std::string BOOKMARK_URL("http://example.com/index.html");

class EnhancedBookmarkModelTest : public testing::Test {
 public:
  EnhancedBookmarkModelTest() {}
  virtual ~EnhancedBookmarkModelTest() {}

  virtual void SetUp() OVERRIDE {
    bookmarks::TestBookmarkClient bookmark_client;
    bookmark_model_.reset(bookmark_client.CreateModel().release());
    model_.reset(new enhanced_bookmarks::EnhancedBookmarkModel(
        bookmark_model_.get(), "v1.0"));
  }

  virtual void TearDown() OVERRIDE {
    model_.reset();
    bookmark_model_.reset();
  }

 protected:
  const BookmarkNode* AddBookmark() {
    return AddBookmark("Some title", bookmark_model_->other_node());
  }

  const BookmarkNode* AddFolder() {
    return AddFolder("Some title", bookmark_model_->other_node());
  }

  const BookmarkNode* AddBookmark(const std::string& name,
                                  const BookmarkNode* parent) {
    return bookmark_model_->AddURL(parent,
                                   0,  // index.
                                   base::ASCIIToUTF16(name),
                                   GURL(BOOKMARK_URL));
  }

  const BookmarkNode* AddFolder(const std::string& name,
                                const BookmarkNode* parent) {
    return bookmark_model_->AddFolder(parent, 0, base::ASCIIToUTF16(name));
  }

  std::string GetVersionForNode(const BookmarkNode* node) {
    std::string version;
    if (!node->GetMetaInfo("stars.version", &version))
      return std::string();
    return version;
  }

  scoped_ptr<BookmarkModel> bookmark_model_;
  scoped_ptr<enhanced_bookmarks::EnhancedBookmarkModel> model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnhancedBookmarkModelTest);
};

TEST_F(EnhancedBookmarkModelTest, TestEmptySnippet) {
  const BookmarkNode* node = AddBookmark();

  std::string snippet(model_->GetSnippet(node));
  EXPECT_EQ(snippet, "");
};

TEST_F(EnhancedBookmarkModelTest, TestSnippet) {
  const BookmarkNode* node = AddBookmark();

  // Binary serialize the protobuf.
  image::collections::PageData data;
  data.set_snippet("I'm happy!");
  ASSERT_TRUE(data.IsInitialized());
  std::string output;
  bool result = data.SerializeToString(&output);
  ASSERT_TRUE(result);

  // base64 encode the output.
  std::string encoded;
  base::Base64Encode(output, &encoded);
  bookmark_model_->SetNodeMetaInfo(node, "stars.pageData", encoded);

  std::string snippet(model_->GetSnippet(node));
  EXPECT_EQ(snippet, "I'm happy!");
}

TEST_F(EnhancedBookmarkModelTest, TestBadEncodingSnippet) {
  const BookmarkNode* node = AddBookmark();

  // Binary serialize the protobuf.
  image::collections::PageData data;
  data.set_snippet("You are happy!");
  ASSERT_TRUE(data.IsInitialized());
  std::string output;
  bool result = data.SerializeToString(&output);
  ASSERT_TRUE(result);

  // don't base 64 encode the output.
  bookmark_model_->SetNodeMetaInfo(node, "stars.pageData", output);

  std::string snippet(model_->GetSnippet(node));
  EXPECT_EQ(snippet, "");
}

TEST_F(EnhancedBookmarkModelTest, TestOriginalImage) {
  const BookmarkNode* node = AddBookmark();

  image::collections::ImageData data;
  // Intentionally make raw pointer.
  image::collections::ImageData_ImageInfo* info =
      new image::collections::ImageData_ImageInfo;
  info->set_url("http://example.com/foobar");
  info->set_width(15);
  info->set_height(55);
  // This method consumes the pointer.
  data.set_allocated_original_info(info);

  std::string output;
  bool result = data.SerializePartialToString(&output);
  ASSERT_TRUE(result);

  // base64 encode the output.
  std::string encoded;
  base::Base64Encode(output, &encoded);
  bookmark_model_->SetNodeMetaInfo(node, "stars.imageData", encoded);

  GURL url;
  int width;
  int height;
  result = model_->GetOriginalImage(node, &url, &width, &height);
  ASSERT_TRUE(result);
  EXPECT_EQ(url, GURL("http://example.com/foobar"));
  EXPECT_EQ(width, 15);
  EXPECT_EQ(height, 55);
}

TEST_F(EnhancedBookmarkModelTest, TestThumbnailImage) {
  const BookmarkNode* node = AddBookmark();

  image::collections::ImageData data;
  // Intentionally make raw pointer.
  image::collections::ImageData_ImageInfo* info =
      new image::collections::ImageData_ImageInfo;
  info->set_url("http://example.com/foobar");
  info->set_width(15);
  info->set_height(55);
  // This method consumes the pointer.
  data.set_allocated_thumbnail_info(info);

  std::string output;
  bool result = data.SerializePartialToString(&output);
  ASSERT_TRUE(result);

  // base64 encode the output.
  std::string encoded;
  base::Base64Encode(output, &encoded);
  bookmark_model_->SetNodeMetaInfo(node, "stars.imageData", encoded);

  GURL url;
  int width;
  int height;
  result = model_->GetThumbnailImage(node, &url, &width, &height);
  ASSERT_TRUE(result);
  EXPECT_EQ(url, GURL("http://example.com/foobar"));
  EXPECT_EQ(width, 15);
  EXPECT_EQ(height, 55);
}

TEST_F(EnhancedBookmarkModelTest, TestOriginalImageMissingDimensions) {
  const BookmarkNode* node = AddBookmark();

  image::collections::ImageData data;
  // Intentionally make raw pointer.
  image::collections::ImageData_ImageInfo* info =
      new image::collections::ImageData_ImageInfo;
  info->set_url("http://example.com/foobar");
  // This method consumes the pointer.
  data.set_allocated_original_info(info);

  std::string output;
  bool result = data.SerializePartialToString(&output);
  ASSERT_TRUE(result);

  // base64 encode the output.
  std::string encoded;
  base::Base64Encode(output, &encoded);
  bookmark_model_->SetNodeMetaInfo(node, "stars.imageData", encoded);

  GURL url;
  int width;
  int height;
  result = model_->GetOriginalImage(node, &url, &width, &height);
  ASSERT_FALSE(result);
}

TEST_F(EnhancedBookmarkModelTest, TestOriginalImageBadUrl) {
  const BookmarkNode* node = AddBookmark();

  image::collections::ImageData data;
  // Intentionally make raw pointer.
  image::collections::ImageData_ImageInfo* info =
      new image::collections::ImageData_ImageInfo;
  info->set_url("asdf. 13r");
  info->set_width(15);
  info->set_height(55);
  // This method consumes the pointer.
  data.set_allocated_original_info(info);

  std::string output;
  bool result = data.SerializePartialToString(&output);
  ASSERT_TRUE(result);

  // base64 encode the output.
  std::string encoded;
  base::Base64Encode(output, &encoded);
  bookmark_model_->SetNodeMetaInfo(node, "stars.imageData", encoded);

  GURL url;
  int width;
  int height;
  result = model_->GetOriginalImage(node, &url, &width, &height);
  ASSERT_FALSE(result);
}

TEST_F(EnhancedBookmarkModelTest, TestEncodeDecode) {
  const BookmarkNode* node = AddBookmark();

  bool result =
      model_->SetOriginalImage(node, GURL("http://example.com/i.jpg"), 22, 33);
  ASSERT_TRUE(result);

  GURL url;
  int width;
  int height;
  result = model_->GetOriginalImage(node, &url, &width, &height);
  ASSERT_TRUE(result);
  EXPECT_EQ(url, GURL("http://example.com/i.jpg"));
  EXPECT_EQ(width, 22);
  EXPECT_EQ(height, 33);
  EXPECT_EQ("v1.0", GetVersionForNode(node));
}

TEST_F(EnhancedBookmarkModelTest, TestDoubleEncodeDecode) {
  const BookmarkNode* node = AddBookmark();

  // Encode some information.
  bool result =
      model_->SetOriginalImage(node, GURL("http://example.com/i.jpg"), 22, 33);
  ASSERT_TRUE(result);
  // Encode some different information.
  result =
      model_->SetOriginalImage(node, GURL("http://example.com/i.jpg"), 33, 44);
  ASSERT_TRUE(result);

  GURL url;
  int width;
  int height;
  result = model_->GetOriginalImage(node, &url, &width, &height);
  ASSERT_TRUE(result);
  EXPECT_EQ(url, GURL("http://example.com/i.jpg"));
  EXPECT_EQ(width, 33);
  EXPECT_EQ(height, 44);
  EXPECT_EQ("v1.0", GetVersionForNode(node));
}

TEST_F(EnhancedBookmarkModelTest, TestRemoteId) {
  const BookmarkNode* node = AddBookmark();
  const BookmarkNode* folder_node = AddFolder();

  std::string remote_id = model_->GetRemoteId(node);
  // First call creates the UUID, second call should return the same.
  EXPECT_EQ(remote_id, model_->GetRemoteId(node));

  // Verify that the remote id starts with the correct prefix.
  EXPECT_TRUE(StartsWithASCII(remote_id, "ebc_", true));
  std::string folder_remote_id = model_->GetRemoteId(folder_node);
  EXPECT_TRUE(StartsWithASCII(folder_remote_id, "ebf_", true));

  // Verifiy version field was set.
  EXPECT_EQ("v1.0", GetVersionForNode(node));
  EXPECT_EQ("v1.0", GetVersionForNode(folder_node));
}

TEST_F(EnhancedBookmarkModelTest, TestEmptyDescription) {
  const BookmarkNode* node = AddBookmark();

  std::string description(model_->GetDescription(node));
  EXPECT_EQ(description, "");
}

TEST_F(EnhancedBookmarkModelTest, TestDescription) {
  const BookmarkNode* node = AddBookmark();
  const std::string description("This is the most useful description of all.");

  // Set the description.
  model_->SetDescription(node, description);

  // Check the description is the one that was set.
  EXPECT_EQ(model_->GetDescription(node), description);
  EXPECT_EQ("v1.0", GetVersionForNode(node));
}

// If there is no notes field, the description should fall back on the snippet.
TEST_F(EnhancedBookmarkModelTest, TestDescriptionFallback) {
  const BookmarkNode* node = AddBookmark();

  // Binary serialize the protobuf.
  image::collections::PageData data;
  data.set_snippet("Joe Bar Team");
  ASSERT_TRUE(data.IsInitialized());
  std::string output;
  bool result = data.SerializeToString(&output);
  ASSERT_TRUE(result);

  // base64 encode the output.
  std::string encoded;
  base::Base64Encode(output, &encoded);
  bookmark_model_->SetNodeMetaInfo(node, "stars.pageData", encoded);

  // The snippet is used as the description.
  std::string snippet(model_->GetSnippet(node));
  EXPECT_EQ("Joe Bar Team", model_->GetDescription(node));

  // Set the description.
  const std::string description("This is the most useful description of all.");
  model_->SetDescription(node, description);

  // Check the description is the one that was set.
  EXPECT_EQ(model_->GetDescription(node), description);
}

// Makes sure that the stars.version field is set every time
// EnhancedBookmarkModel makes a change to a node.
TEST_F(EnhancedBookmarkModelTest, TestVersionField) {
  const BookmarkNode* node = AddBookmark();
  EXPECT_EQ("", GetVersionForNode(node));

  model_->SetDescription(node, "foo");
  EXPECT_EQ("v1.0", GetVersionForNode(node));

  // Add a suffix to the version to set.
  model_->SetVersionSuffix("alpha");

  model_->SetDescription(node, "foo");
  // Since the description didn't actually change, the version field should
  // not either.
  EXPECT_EQ("v1.0", GetVersionForNode(node));

  model_->SetDescription(node, "bar");
  EXPECT_EQ("v1.0/alpha", GetVersionForNode(node));
}

// Verifies that the stars.userEdit field is set appropriately when editing a
// node.
TEST_F(EnhancedBookmarkModelTest, TestUserEdit) {
  const BookmarkNode* node = AddBookmark();

  model_->SetDescription(node, "foo");
  std::string user_edit;
  ASSERT_TRUE(node->GetMetaInfo("stars.userEdit", &user_edit));
  EXPECT_EQ("true", user_edit);
}

}  // namespace
