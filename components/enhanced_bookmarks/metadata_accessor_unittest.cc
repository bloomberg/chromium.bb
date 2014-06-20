// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/metadata_accessor.h"

#include "base/base64.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/enhanced_bookmarks/proto/metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using namespace image::collections;

const std::string BOOKMARK_URL("http://example.com/index.html");

class MetadataAccessorTest : public testing::Test {
 public:
  MetadataAccessorTest() {}
  virtual ~MetadataAccessorTest() {}

 protected:
  DISALLOW_COPY_AND_ASSIGN(MetadataAccessorTest);

  // Adds a bookmark as the subnode at index 0 to other_node.
  // |name| should be ASCII encoded.
  // Returns the newly added bookmark.
  const BookmarkNode* AddBookmark(BookmarkModel* model, std::string name) {
    return model->AddURL(model->other_node(),
                         0,  // index.
                         base::ASCIIToUTF16(name),
                         GURL(BOOKMARK_URL));
  }
};

TEST_F(MetadataAccessorTest, TestEmptySnippet) {
  scoped_ptr<BookmarkNode> node(new BookmarkNode(GURL(BOOKMARK_URL)));

  std::string snippet(enhanced_bookmarks::SnippetFromBookmark(node.get()));
  CHECK_EQ(snippet, "");
};

TEST_F(MetadataAccessorTest, TestSnippet) {
  scoped_ptr<BookmarkNode> node(new BookmarkNode(GURL(BOOKMARK_URL)));

  // Binary serialize the protobuf.
  PageData data;
  data.set_snippet("I'm happy!");
  ASSERT_TRUE(data.IsInitialized());
  std::string output;
  bool result = data.SerializeToString(&output);
  ASSERT_TRUE(result);

  // base64 encode the output.
  std::string encoded;
  base::Base64Encode(output, &encoded);
  node->SetMetaInfo(enhanced_bookmarks::kPageDataKey, encoded);

  std::string snippet(enhanced_bookmarks::SnippetFromBookmark(node.get()));
  CHECK_EQ(snippet, "I'm happy!");
};

TEST_F(MetadataAccessorTest, TestBadEncodingSnippet) {
  scoped_ptr<BookmarkNode> node(new BookmarkNode(GURL(BOOKMARK_URL)));

  // Binary serialize the protobuf.
  PageData data;
  data.set_snippet("You are happy!");
  ASSERT_TRUE(data.IsInitialized());
  std::string output;
  bool result = data.SerializeToString(&output);
  ASSERT_TRUE(result);

  // don't base 64 encode the output.
  node->SetMetaInfo(enhanced_bookmarks::kPageDataKey, output);

  std::string snippet(enhanced_bookmarks::SnippetFromBookmark(node.get()));
  CHECK_EQ(snippet, "");
};

TEST_F(MetadataAccessorTest, TestOriginalImage) {
  scoped_ptr<BookmarkNode> node(new BookmarkNode(GURL(BOOKMARK_URL)));

  ImageData data;
  // Intentionally make raw pointer.
  ImageData_ImageInfo* info = new ImageData_ImageInfo;
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
  node->SetMetaInfo(enhanced_bookmarks::kImageDataKey, encoded);

  GURL url;
  int width;
  int height;
  result = enhanced_bookmarks::OriginalImageFromBookmark(
      node.get(), &url, &width, &height);
  ASSERT_TRUE(result);
  CHECK_EQ(url, GURL("http://example.com/foobar"));
  CHECK_EQ(width, 15);
  CHECK_EQ(height, 55);
};

TEST_F(MetadataAccessorTest, TestThumbnailImage) {
  scoped_ptr<BookmarkNode> node(new BookmarkNode(GURL(BOOKMARK_URL)));

  ImageData data;
  // Intentionally make raw pointer.
  ImageData_ImageInfo* info = new ImageData_ImageInfo;
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
  node->SetMetaInfo(enhanced_bookmarks::kImageDataKey, encoded);

  GURL url;
  int width;
  int height;
  result = enhanced_bookmarks::ThumbnailImageFromBookmark(
      node.get(), &url, &width, &height);
  ASSERT_TRUE(result);
  CHECK_EQ(url, GURL("http://example.com/foobar"));
  CHECK_EQ(width, 15);
  CHECK_EQ(height, 55);
};

TEST_F(MetadataAccessorTest, TestOriginalImageMissingDimensions) {
  scoped_ptr<BookmarkNode> node(new BookmarkNode(GURL(BOOKMARK_URL)));

  ImageData data;
  // Intentionally make raw pointer.
  ImageData_ImageInfo* info = new ImageData_ImageInfo;
  info->set_url("http://example.com/foobar");
  // This method consumes the pointer.
  data.set_allocated_original_info(info);

  std::string output;
  bool result = data.SerializePartialToString(&output);
  ASSERT_TRUE(result);

  // base64 encode the output.
  std::string encoded;
  base::Base64Encode(output, &encoded);
  node->SetMetaInfo(enhanced_bookmarks::kImageDataKey, encoded);

  GURL url;
  int width;
  int height;
  result = enhanced_bookmarks::OriginalImageFromBookmark(
      node.get(), &url, &width, &height);
  ASSERT_FALSE(result);
};

TEST_F(MetadataAccessorTest, TestOriginalImageBadUrl) {
  scoped_ptr<BookmarkNode> node(new BookmarkNode(GURL(BOOKMARK_URL)));

  ImageData data;
  // Intentionally make raw pointer.
  ImageData_ImageInfo* info = new ImageData_ImageInfo;
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
  node->SetMetaInfo(enhanced_bookmarks::kImageDataKey, encoded);

  GURL url;
  int width;
  int height;
  result = enhanced_bookmarks::OriginalImageFromBookmark(
      node.get(), &url, &width, &height);
  ASSERT_FALSE(result);
};

TEST_F(MetadataAccessorTest, TestEncodeDecode) {
  test::TestBookmarkClient bookmark_client;
  scoped_ptr<BookmarkModel> bookmark_model(bookmark_client.CreateModel(false));
  const BookmarkNode* node =
      bookmark_model->AddURL(bookmark_model->other_node(),
                             0,  // index.
                             base::ASCIIToUTF16("whatever"),
                             GURL(BOOKMARK_URL));

  bool result = enhanced_bookmarks::SetOriginalImageForBookmark(
      bookmark_model.get(), node, GURL("http://example.com/i.jpg"), 22, 33);
  ASSERT_TRUE(result);

  GURL url;
  int width;
  int height;
  result = enhanced_bookmarks::OriginalImageFromBookmark(
      node, &url, &width, &height);
  ASSERT_TRUE(result);
  CHECK_EQ(url, GURL("http://example.com/i.jpg"));
  CHECK_EQ(width, 22);
  CHECK_EQ(height, 33);
};

TEST_F(MetadataAccessorTest, TestDoubleEncodeDecode) {
  test::TestBookmarkClient bookmark_client;
  scoped_ptr<BookmarkModel> bookmark_model(bookmark_client.CreateModel(false));
  const BookmarkNode* node =
      bookmark_model->AddURL(bookmark_model->other_node(),
                             0,  // index.
                             base::ASCIIToUTF16("whatever"),
                             GURL(BOOKMARK_URL));

  // Encode some information.
  bool result = enhanced_bookmarks::SetOriginalImageForBookmark(
      bookmark_model.get(), node, GURL("http://example.com/i.jpg"), 22, 33);
  ASSERT_TRUE(result);
  // Encode some different information.
  result = enhanced_bookmarks::SetOriginalImageForBookmark(
      bookmark_model.get(), node, GURL("http://example.com/i.jpg"), 33, 44);
  ASSERT_TRUE(result);

  GURL url;
  int width;
  int height;
  result = enhanced_bookmarks::OriginalImageFromBookmark(
      node, &url, &width, &height);
  ASSERT_TRUE(result);
  CHECK_EQ(url, GURL("http://example.com/i.jpg"));
  CHECK_EQ(width, 33);
  CHECK_EQ(height, 44);
};

TEST_F(MetadataAccessorTest, TestThumbnail) {
  test::TestBookmarkClient bookmark_client;
  scoped_ptr<BookmarkModel> bookmark_model(bookmark_client.CreateModel(false));
  const BookmarkNode* node =
      bookmark_model->AddURL(bookmark_model->other_node(),
                             0,  // index.
                             base::ASCIIToUTF16("whatever"),
                             GURL(BOOKMARK_URL));

  // Encode some information.
  ASSERT_TRUE(enhanced_bookmarks::SetAllImagesForBookmark(
      bookmark_model.get(),
      node,
      GURL(),
      0,
      0,
      GURL("http://google.com/img/thumb.jpg"),
      33,
      44));
  GURL url;
  int width;
  int height;
  bool result = enhanced_bookmarks::ThumbnailImageFromBookmark(
      node, &url, &width, &height);
  ASSERT_TRUE(result);
  CHECK_EQ(url, GURL("http://google.com/img/thumb.jpg"));
  CHECK_EQ(width, 33);
  CHECK_EQ(height, 44);
};

TEST_F(MetadataAccessorTest, TestRemoteId) {
  test::TestBookmarkClient bookmark_client;
  scoped_ptr<BookmarkModel> bookmark_model(bookmark_client.CreateModel(false));
  const BookmarkNode* node = AddBookmark(bookmark_model.get(), "Aga Khan");

  // First call creates the UUID, second call should return the same.
  ASSERT_EQ(
      enhanced_bookmarks::RemoteIdFromBookmark(bookmark_model.get(), node),
      enhanced_bookmarks::RemoteIdFromBookmark(bookmark_model.get(), node));
}

TEST_F(MetadataAccessorTest, TestEmptyDescription) {
  scoped_ptr<BookmarkNode> node(new BookmarkNode(GURL(BOOKMARK_URL)));

  std::string description(
      enhanced_bookmarks::DescriptionFromBookmark(node.get()));
  CHECK_EQ(description, "");
}

TEST_F(MetadataAccessorTest, TestDescription) {
  test::TestBookmarkClient bookmark_client;
  scoped_ptr<BookmarkModel> bookmark_model(bookmark_client.CreateModel(false));
  scoped_ptr<BookmarkNode> node(new BookmarkNode(GURL(BOOKMARK_URL)));
  const std::string description("This is the most useful description of all.");

  // Set the description.
  enhanced_bookmarks::SetDescriptionForBookmark(
      bookmark_model.get(), node.get(), description);

  // Check the description is the one that was set.
  CHECK_EQ(enhanced_bookmarks::DescriptionFromBookmark(node.get()),
           description);
}

// If there is no notes field, the description should fall back on the snippet.
TEST_F(MetadataAccessorTest, TestDescriptionFallback) {
  test::TestBookmarkClient bookmark_client;
  scoped_ptr<BookmarkModel> bookmark_model(bookmark_client.CreateModel(false));
  scoped_ptr<BookmarkNode> node(new BookmarkNode(GURL(BOOKMARK_URL)));

  // Binary serialize the protobuf.
  PageData data;
  data.set_snippet("Joe Bar Team");
  ASSERT_TRUE(data.IsInitialized());
  std::string output;
  bool result = data.SerializeToString(&output);
  ASSERT_TRUE(result);

  // base64 encode the output.
  std::string encoded;
  base::Base64Encode(output, &encoded);
  node->SetMetaInfo(enhanced_bookmarks::kPageDataKey, encoded);

  // The snippet is used as the description.
  std::string snippet(enhanced_bookmarks::SnippetFromBookmark(node.get()));
  CHECK_EQ("Joe Bar Team",
           enhanced_bookmarks::DescriptionFromBookmark(node.get()));

  // Set the description.
  const std::string description("This is the most useful description of all.");
  enhanced_bookmarks::SetDescriptionForBookmark(
      bookmark_model.get(), node.get(), description);

  // Check the description is the one that was set.
  CHECK_EQ(enhanced_bookmarks::DescriptionFromBookmark(node.get()),
           description);
}
}  // namespace
