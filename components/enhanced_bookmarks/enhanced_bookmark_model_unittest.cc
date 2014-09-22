// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/enhanced_bookmark_model.h"

#include "base/base64.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_model_observer.h"
#include "components/enhanced_bookmarks/proto/metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using enhanced_bookmarks::EnhancedBookmarkModel;

namespace {
const std::string BOOKMARK_URL("http://example.com/index.html");
}  // namespace

class EnhancedBookmarkModelTest
    : public testing::Test,
      public enhanced_bookmarks::EnhancedBookmarkModelObserver {
 public:
  EnhancedBookmarkModelTest()
      : loaded_calls_(0),
        shutting_down_calls_(0),
        added_calls_(0),
        removed_calls_(0),
        all_user_nodes_removed_calls_(0),
        remote_id_changed_calls_(0),
        last_added_(NULL),
        last_removed_(NULL),
        last_remote_id_node_(NULL) {}
  virtual ~EnhancedBookmarkModelTest() {}

  virtual void SetUp() OVERRIDE {
    message_loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_DEFAULT));
    bookmark_client_.reset(new bookmarks::TestBookmarkClient());
    bookmark_model_.reset(bookmark_client_->CreateModel().release());
    model_.reset(new EnhancedBookmarkModel(bookmark_model_.get(), "v1.0"));
    model_->AddObserver(this);
  }

  virtual void TearDown() OVERRIDE {
    if (model_)
      model_->Shutdown();
    model_.reset();
    bookmark_model_.reset();
    bookmark_client_.reset();
    message_loop_.reset();
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
    return model_->AddURL(parent,
                          0,  // index.
                          base::ASCIIToUTF16(name),
                          GURL(BOOKMARK_URL),
                          base::Time::Now());
  }

  const BookmarkNode* AddFolder(const std::string& name,
                                const BookmarkNode* parent) {
    return model_->AddFolder(parent, 0, base::ASCIIToUTF16(name));
  }

  std::string GetVersion(const BookmarkNode* node) {
    return GetMetaInfoField(node, "stars.version");
  }

  std::string GetId(const BookmarkNode* node) {
    return GetMetaInfoField(node, "stars.id");
  }

  std::string GetOldId(const BookmarkNode* node) {
    return GetMetaInfoField(node, "stars.oldId");
  }

  std::string GetMetaInfoField(const BookmarkNode* node,
                               const std::string& name) {
    std::string value;
    if (!node->GetMetaInfo(name, &value))
      return std::string();
    return value;
  }

  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<bookmarks::TestBookmarkClient> bookmark_client_;
  scoped_ptr<BookmarkModel> bookmark_model_;
  scoped_ptr<EnhancedBookmarkModel> model_;

  // EnhancedBookmarkModelObserver implementation:
  virtual void EnhancedBookmarkModelLoaded() OVERRIDE { loaded_calls_++; }
  virtual void EnhancedBookmarkModelShuttingDown() OVERRIDE {
    shutting_down_calls_++;
  }
  virtual void EnhancedBookmarkAdded(const BookmarkNode* node) OVERRIDE {
    added_calls_++;
    last_added_ = node;
  }
  virtual void EnhancedBookmarkRemoved(const BookmarkNode* node) OVERRIDE {
    removed_calls_++;
    last_removed_ = node;
  }
  virtual void EnhancedBookmarkAllUserNodesRemoved() OVERRIDE {
    all_user_nodes_removed_calls_++;
  }
  virtual void EnhancedBookmarkRemoteIdChanged(
      const BookmarkNode* node,
      const std::string& old_remote_id,
      const std::string& remote_id) OVERRIDE {
    remote_id_changed_calls_++;
    last_remote_id_node_ = node;
    last_old_remote_id_ = old_remote_id;
    last_remote_id_ = remote_id;
  }

  // Observer call counters:
  int loaded_calls_;
  int shutting_down_calls_;
  int added_calls_;
  int removed_calls_;
  int all_user_nodes_removed_calls_;
  int remote_id_changed_calls_;

  // Observer parameter cache:
  const BookmarkNode* last_added_;
  const BookmarkNode* last_removed_;
  const BookmarkNode* last_remote_id_node_;
  std::string last_old_remote_id_;
  std::string last_remote_id_;

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
  EXPECT_EQ("v1.0", GetVersion(node));
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
  EXPECT_EQ("v1.0", GetVersion(node));
}

TEST_F(EnhancedBookmarkModelTest, TestRemoteId) {
  const BookmarkNode* node = AddBookmark();
  // Verify that the remote id starts with the correct prefix.
  EXPECT_TRUE(StartsWithASCII(model_->GetRemoteId(node), "ebc_", true));

  // Getting the remote id for nodes that don't have them should return the
  // empty string.
  const BookmarkNode* existing_node =
      bookmark_model_->AddURL(bookmark_model_->other_node(),
                              0,
                              base::ASCIIToUTF16("Title"),
                              GURL(GURL(BOOKMARK_URL)));
  EXPECT_TRUE(model_->GetRemoteId(existing_node).empty());

  // Folder nodes should not have a remote id set on creation.
  const BookmarkNode* folder_node = AddFolder();
  EXPECT_TRUE(model_->GetRemoteId(folder_node).empty());
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
  EXPECT_EQ("v1.0", GetVersion(node));
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
  EXPECT_EQ("", GetVersion(node));

  model_->SetDescription(node, "foo");
  EXPECT_EQ("v1.0", GetVersion(node));

  // Add a suffix to the version to set.
  model_->SetVersionSuffix("alpha");

  model_->SetDescription(node, "foo");
  // Since the description didn't actually change, the version field should
  // not either.
  EXPECT_EQ("v1.0", GetVersion(node));

  model_->SetDescription(node, "bar");
  EXPECT_EQ("v1.0/alpha", GetVersion(node));
}

// Verifies that duplicate nodes are reset when the model is created.
TEST_F(EnhancedBookmarkModelTest, ResetDuplicateNodesOnInitialization) {
  model_->Shutdown();

  const BookmarkNode* parent = bookmark_model_->other_node();
  const BookmarkNode* node1 = bookmark_model_->AddURL(
      parent, 0, base::ASCIIToUTF16("Some title"), GURL(BOOKMARK_URL));
  const BookmarkNode* node2 = bookmark_model_->AddURL(
      parent, 0, base::ASCIIToUTF16("Some title"), GURL(BOOKMARK_URL));
  const BookmarkNode* node3 = bookmark_model_->AddURL(
      parent, 0, base::ASCIIToUTF16("Some title"), GURL(BOOKMARK_URL));
  const BookmarkNode* node4 = bookmark_model_->AddURL(
      parent, 0, base::ASCIIToUTF16("Some title"), GURL(BOOKMARK_URL));

  bookmark_model_->SetNodeMetaInfo(node1, "stars.id", "c_1");
  bookmark_model_->SetNodeMetaInfo(node2, "stars.id", "c_2");
  bookmark_model_->SetNodeMetaInfo(node3, "stars.id", "c_1");
  bookmark_model_->SetNodeMetaInfo(node4, "stars.id", "c_1");
  EXPECT_EQ("c_1", GetId(node1));
  EXPECT_EQ("c_2", GetId(node2));
  EXPECT_EQ("c_1", GetId(node3));
  EXPECT_EQ("c_1", GetId(node4));

  model_.reset(new EnhancedBookmarkModel(bookmark_model_.get(), "v2.0"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("c_2", GetId(node2));
  EXPECT_EQ("", GetId(node1));
  EXPECT_EQ("", GetId(node3));
  EXPECT_EQ("", GetId(node4));
  EXPECT_EQ("c_1", GetOldId(node1));
  EXPECT_EQ("c_1", GetOldId(node3));
  EXPECT_EQ("c_1", GetOldId(node4));
  EXPECT_EQ("v2.0", GetVersion(node1));
  EXPECT_EQ("v2.0", GetVersion(node3));
  EXPECT_EQ("v2.0", GetVersion(node4));
}

// Verifies that duplicate nodes are reset if one is created.
TEST_F(EnhancedBookmarkModelTest, ResetDuplicateAddedNodes) {
  BookmarkNode::MetaInfoMap meta_info;
  meta_info["stars.id"] = "c_1";
  const BookmarkNode* parent = bookmark_model_->other_node();

  const BookmarkNode* node1 =
      bookmark_model_->AddURLWithCreationTimeAndMetaInfo(
          parent,
          0,
          base::ASCIIToUTF16("Some title"),
          GURL(BOOKMARK_URL),
          base::Time::Now(),
          &meta_info);
  EXPECT_EQ("c_1", GetId(node1));

  const BookmarkNode* node2 =
      bookmark_model_->AddURLWithCreationTimeAndMetaInfo(
          parent,
          0,
          base::ASCIIToUTF16("Some title"),
          GURL(BOOKMARK_URL),
          base::Time::Now(),
          &meta_info);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", GetId(node1));
  EXPECT_EQ("", GetId(node2));
  EXPECT_EQ("c_1", GetOldId(node1));
  EXPECT_EQ("c_1", GetOldId(node2));
  EXPECT_EQ("v1.0", GetVersion(node1));
  EXPECT_EQ("v1.0", GetVersion(node2));
}

// Verifies that duplicate nodes are reset if an id is changed to a duplicate
// value.
TEST_F(EnhancedBookmarkModelTest, ResetDuplicateChangedNodes) {
  const BookmarkNode* node1 = AddBookmark();
  const BookmarkNode* node2 = AddBookmark();

  bookmark_model_->SetNodeMetaInfo(node1, "stars.id", "c_1");
  EXPECT_EQ("c_1", GetId(node1));

  bookmark_model_->SetNodeMetaInfo(node2, "stars.id", "c_1");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", GetId(node1));
  EXPECT_EQ("", GetId(node2));
  EXPECT_EQ("c_1", GetOldId(node1));
  EXPECT_EQ("c_1", GetOldId(node2));
  EXPECT_EQ("v1.0", GetVersion(node1));
  EXPECT_EQ("v1.0", GetVersion(node2));
}

TEST_F(EnhancedBookmarkModelTest, SetMultipleMetaInfo) {
  const BookmarkNode* node = AddBookmark();
  BookmarkNode::MetaInfoMap meta_info;
  meta_info["a"] = "aa";
  meta_info["b"] = "bb";

  model_->SetVersionSuffix("1");
  model_->SetMultipleMetaInfo(node, meta_info);
  EXPECT_EQ("aa", GetMetaInfoField(node, "a"));
  EXPECT_EQ("bb", GetMetaInfoField(node, "b"));
  EXPECT_EQ("v1.0/1", GetVersion(node));

  // Not present fields does not erase the fields already set on the node.
  meta_info["a"] = "aaa";
  model_->SetVersionSuffix("2");
  model_->SetMultipleMetaInfo(node, meta_info);
  EXPECT_EQ("aaa", GetMetaInfoField(node, "a"));
  EXPECT_EQ("bb", GetMetaInfoField(node, "b"));
  EXPECT_EQ("v1.0/2", GetVersion(node));

  // Not actually changing any values should not set the version field.
  model_->SetVersionSuffix("3");
  model_->SetMultipleMetaInfo(node, meta_info);
  EXPECT_EQ("v1.0/2", GetVersion(node));
}

TEST_F(EnhancedBookmarkModelTest, ObserverShuttingDownEvent) {
  EXPECT_EQ(0, shutting_down_calls_);
  model_->Shutdown();
  EXPECT_EQ(1, shutting_down_calls_);
  model_.reset();
}

TEST_F(EnhancedBookmarkModelTest, ObserverNodeAddedEvent) {
  EXPECT_EQ(0, added_calls_);
  const BookmarkNode* node = AddBookmark();
  EXPECT_EQ(1, added_calls_);
  EXPECT_EQ(node, last_added_);

  const BookmarkNode* folder = AddFolder();
  EXPECT_EQ(2, added_calls_);
  EXPECT_EQ(folder, last_added_);
}

TEST_F(EnhancedBookmarkModelTest, ObserverNodeRemovedEvent) {
  const BookmarkNode* node = AddBookmark();
  const BookmarkNode* folder = AddFolder();

  EXPECT_EQ(0, removed_calls_);
  bookmark_model_->Remove(node->parent(), node->parent()->GetIndexOf(node));
  EXPECT_EQ(1, removed_calls_);
  EXPECT_EQ(node, last_removed_);

  bookmark_model_->Remove(folder->parent(),
                          folder->parent()->GetIndexOf(folder));
  EXPECT_EQ(2, removed_calls_);
  EXPECT_EQ(folder, last_removed_);
}

TEST_F(EnhancedBookmarkModelTest, ObserverAllUserNodesRemovedEvent) {
  AddBookmark();
  AddFolder();
  EXPECT_EQ(0, all_user_nodes_removed_calls_);
  bookmark_model_->RemoveAllUserBookmarks();
  EXPECT_EQ(0, removed_calls_);
  EXPECT_EQ(1, all_user_nodes_removed_calls_);
}

TEST_F(EnhancedBookmarkModelTest, ObserverRemoteIdChangedEvent) {
  const BookmarkNode* node1 = AddFolder();
  const BookmarkNode* node2 = AddFolder();

  EXPECT_EQ(0, remote_id_changed_calls_);
  bookmark_model_->SetNodeMetaInfo(node1, "stars.id", "c_1");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, remote_id_changed_calls_);
  EXPECT_EQ(node1, last_remote_id_node_);
  EXPECT_EQ("", last_old_remote_id_);
  EXPECT_EQ("c_1", last_remote_id_);

  bookmark_model_->SetNodeMetaInfo(node2, "stars.id", "c_2");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, remote_id_changed_calls_);
  EXPECT_EQ(node2, last_remote_id_node_);
  EXPECT_EQ("", last_old_remote_id_);
  EXPECT_EQ("c_2", last_remote_id_);

  bookmark_model_->SetNodeMetaInfo(node1, "stars.id", "c_3");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, remote_id_changed_calls_);
  EXPECT_EQ(node1, last_remote_id_node_);
  EXPECT_EQ("c_1", last_old_remote_id_);
  EXPECT_EQ("c_3", last_remote_id_);

  // Set to duplicate ids.
  bookmark_model_->SetNodeMetaInfo(node2, "stars.id", "c_3");
  EXPECT_EQ(4, remote_id_changed_calls_);
  EXPECT_EQ(node2, last_remote_id_node_);
  EXPECT_EQ("c_2", last_old_remote_id_);
  EXPECT_EQ("c_3", last_remote_id_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(6, remote_id_changed_calls_);
  EXPECT_EQ("", last_remote_id_);
}

TEST_F(EnhancedBookmarkModelTest, ShutDownWhileResetDuplicationScheduled) {
  const BookmarkNode* node1 = AddBookmark();
  const BookmarkNode* node2 = AddBookmark();
  bookmark_model_->SetNodeMetaInfo(node1, "stars.id", "c_1");
  bookmark_model_->SetNodeMetaInfo(node2, "stars.id", "c_1");
  model_->Shutdown();
  model_.reset();
  base::RunLoop().RunUntilIdle();
}
