// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/managed_bookmarks_tracker.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/mock_bookmark_model_observer.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using testing::Mock;
using testing::_;

namespace policy {

class ManagedBookmarksTrackerTest : public testing::Test {
 public:
  ManagedBookmarksTrackerTest() : managed_node_(NULL) {}
  virtual ~ManagedBookmarksTrackerTest() {}

  virtual void SetUp() OVERRIDE {
    prefs_.registry()->RegisterListPref(prefs::kManagedBookmarks);
    prefs_.registry()->RegisterListPref(prefs::kBookmarkEditorExpandedNodes);
  }

  virtual void TearDown() OVERRIDE {
    if (model_)
      model_->RemoveObserver(&observer_);
  }

  void CreateModel() {
    // Simulate the creation of the managed node by the BookmarkClient.
    BookmarkPermanentNode* managed_node = new BookmarkPermanentNode(100);
    policy::ManagedBookmarksTracker::LoadInitial(
        managed_node, prefs_.GetList(prefs::kManagedBookmarks), 101);
    managed_node->set_visible(!managed_node->empty());
    managed_node->SetTitle(l10n_util::GetStringUTF16(
        IDS_BOOKMARK_BAR_MANAGED_FOLDER_DEFAULT_NAME));

    bookmarks::BookmarkPermanentNodeList extra_nodes;
    extra_nodes.push_back(managed_node);
    client_.SetExtraNodesToLoad(extra_nodes.Pass());

    model_.reset(new BookmarkModel(&client_, false));
    model_->AddObserver(&observer_);
    EXPECT_CALL(observer_, BookmarkModelLoaded(model_.get(), _));
    model_->Load(&prefs_,
                 std::string(),
                 base::FilePath(),
                 base::MessageLoopProxy::current(),
                 base::MessageLoopProxy::current());
    test::WaitForBookmarkModelToLoad(model_.get());
    Mock::VerifyAndClearExpectations(&observer_);

    ASSERT_EQ(1u, client_.extra_nodes().size());
    managed_node_ = client_.extra_nodes()[0];
    ASSERT_EQ(managed_node, managed_node_);

    managed_bookmarks_tracker_.reset(new ManagedBookmarksTracker(
        model_.get(),
        &prefs_,
        base::Bind(&ManagedBookmarksTrackerTest::GetManagementDomain)));
    managed_bookmarks_tracker_->Init(managed_node_);
  }

  const BookmarkNode* managed_node() {
    return managed_node_;
  }

  bool IsManaged(const BookmarkNode* node) {
    return node && node->HasAncestor(managed_node_);
  }

  static base::DictionaryValue* CreateBookmark(const std::string& title,
                                               const std::string& url) {
    EXPECT_TRUE(GURL(url).is_valid());
    base::DictionaryValue* dict = new base::DictionaryValue();
    dict->SetString("name", title);
    dict->SetString("url", GURL(url).spec());
    return dict;
  }

  static base::DictionaryValue* CreateFolder(const std::string& title,
                                             base::ListValue* children) {
    base::DictionaryValue* dict = new base::DictionaryValue();
    dict->SetString("name", title);
    dict->Set("children", children);
    return dict;
  }

  static base::ListValue* CreateTestTree() {
    base::ListValue* folder = new base::ListValue();
    base::ListValue* empty = new base::ListValue();
    folder->Append(CreateFolder("Empty", empty));
    folder->Append(CreateBookmark("Youtube", "http://youtube.com/"));

    base::ListValue* list = new base::ListValue();
    list->Append(CreateBookmark("Google", "http://google.com/"));
    list->Append(CreateFolder("Folder", folder));

    return list;
  }

  static std::string GetManagementDomain() {
    return std::string();
  }

  static std::string GetManagedFolderTitle() {
    return l10n_util::GetStringUTF8(
        IDS_BOOKMARK_BAR_MANAGED_FOLDER_DEFAULT_NAME);
  }

  static base::DictionaryValue* CreateExpectedTree() {
    return CreateFolder(GetManagedFolderTitle(), CreateTestTree());
  }

  static bool NodeMatchesValue(const BookmarkNode* node,
                               const base::DictionaryValue* dict) {
    base::string16 title;
    if (!dict->GetString("name", &title) || node->GetTitle() != title)
      return false;

    if (node->is_folder()) {
      const base::ListValue* children = NULL;
      if (!dict->GetList("children", &children) ||
          node->child_count() != static_cast<int>(children->GetSize())) {
        return false;
      }
      for (int i = 0; i < node->child_count(); ++i) {
        const base::DictionaryValue* child = NULL;
        if (!children->GetDictionary(i, &child) ||
            !NodeMatchesValue(node->GetChild(i), child)) {
          return false;
        }
      }
    } else if (node->is_url()) {
      std::string url;
      if (!dict->GetString("url", &url) || node->url() != GURL(url))
        return false;
    } else {
      return false;
    }
    return true;
  }

  base::MessageLoop loop_;
  TestingPrefServiceSimple prefs_;
  test::TestBookmarkClient client_;
  scoped_ptr<BookmarkModel> model_;
  MockBookmarkModelObserver observer_;
  BookmarkPermanentNode* managed_node_;
  scoped_ptr<ManagedBookmarksTracker> managed_bookmarks_tracker_;
};

TEST_F(ManagedBookmarksTrackerTest, Empty) {
  CreateModel();
  EXPECT_TRUE(model_->bookmark_bar_node()->empty());
  EXPECT_TRUE(model_->other_node()->empty());
  EXPECT_TRUE(managed_node()->empty());
  EXPECT_FALSE(managed_node()->IsVisible());
}

TEST_F(ManagedBookmarksTrackerTest, LoadInitial) {
  // Set a policy before loading the model.
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();
  EXPECT_TRUE(model_->bookmark_bar_node()->empty());
  EXPECT_TRUE(model_->other_node()->empty());
  EXPECT_FALSE(managed_node()->empty());
  EXPECT_TRUE(managed_node()->IsVisible());

  scoped_ptr<base::DictionaryValue> expected(CreateExpectedTree());
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected.get()));
}

TEST_F(ManagedBookmarksTrackerTest, SwapNodes) {
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();

  // Swap the Google bookmark with the Folder.
  scoped_ptr<base::ListValue> updated(CreateTestTree());
  scoped_ptr<base::Value> removed;
  ASSERT_TRUE(updated->Remove(0, &removed));
  updated->Append(removed.release());

  // These two nodes should just be swapped.
  const BookmarkNode* parent = managed_node();
  EXPECT_CALL(observer_, BookmarkNodeMoved(model_.get(), parent, 1, parent, 0));
  prefs_.SetManagedPref(prefs::kManagedBookmarks, updated->DeepCopy());
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  scoped_ptr<base::DictionaryValue> expected(
      CreateFolder(GetManagedFolderTitle(), updated.release()));
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected.get()));
}

TEST_F(ManagedBookmarksTrackerTest, RemoveNode) {
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();

  // Remove the Folder.
  scoped_ptr<base::ListValue> updated(CreateTestTree());
  ASSERT_TRUE(updated->Remove(1, NULL));

  const BookmarkNode* parent = managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(model_.get(), parent, 1, _, _));
  prefs_.SetManagedPref(prefs::kManagedBookmarks, updated->DeepCopy());
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  scoped_ptr<base::DictionaryValue> expected(
      CreateFolder(GetManagedFolderTitle(), updated.release()));
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected.get()));
}

TEST_F(ManagedBookmarksTrackerTest, CreateNewNodes) {
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();

  // Put all the nodes inside another folder.
  scoped_ptr<base::ListValue> updated(new base::ListValue);
  updated->Append(CreateFolder("Container", CreateTestTree()));

  EXPECT_CALL(observer_, BookmarkNodeAdded(model_.get(), _, _)).Times(5);
  // The remaining nodes have been pushed to positions 1 and 2; they'll both be
  // removed when at position 1.
  const BookmarkNode* parent = managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(model_.get(), parent, 1, _, _))
      .Times(2);
  prefs_.SetManagedPref(prefs::kManagedBookmarks, updated->DeepCopy());
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  scoped_ptr<base::DictionaryValue> expected(
      CreateFolder(GetManagedFolderTitle(), updated.release()));
  EXPECT_TRUE(NodeMatchesValue(managed_node(), expected.get()));
}

TEST_F(ManagedBookmarksTrackerTest, RemoveAll) {
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();
  EXPECT_TRUE(managed_node()->IsVisible());

  // Remove the policy.
  const BookmarkNode* parent = managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(model_.get(), parent, 0, _, _))
      .Times(2);
  prefs_.RemoveManagedPref(prefs::kManagedBookmarks);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(managed_node()->empty());
  EXPECT_FALSE(managed_node()->IsVisible());
}

TEST_F(ManagedBookmarksTrackerTest, IsManaged) {
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();

  EXPECT_FALSE(IsManaged(model_->root_node()));
  EXPECT_FALSE(IsManaged(model_->bookmark_bar_node()));
  EXPECT_FALSE(IsManaged(model_->other_node()));
  EXPECT_FALSE(IsManaged(model_->mobile_node()));
  EXPECT_TRUE(IsManaged(managed_node()));

  const BookmarkNode* parent = managed_node();
  ASSERT_EQ(2, parent->child_count());
  EXPECT_TRUE(IsManaged(parent->GetChild(0)));
  EXPECT_TRUE(IsManaged(parent->GetChild(1)));

  parent = parent->GetChild(1);
  ASSERT_EQ(2, parent->child_count());
  EXPECT_TRUE(IsManaged(parent->GetChild(0)));
  EXPECT_TRUE(IsManaged(parent->GetChild(1)));
}

TEST_F(ManagedBookmarksTrackerTest, RemoveAllUserBookmarksDoesntRemoveManaged) {
  prefs_.SetManagedPref(prefs::kManagedBookmarks, CreateTestTree());
  CreateModel();
  EXPECT_EQ(2, managed_node()->child_count());

  EXPECT_CALL(observer_,
              BookmarkNodeAdded(model_.get(), model_->bookmark_bar_node(), 0));
  EXPECT_CALL(observer_,
              BookmarkNodeAdded(model_.get(), model_->bookmark_bar_node(), 1));
  model_->AddURL(model_->bookmark_bar_node(),
                 0,
                 base::ASCIIToUTF16("Test"),
                 GURL("http://google.com/"));
  model_->AddFolder(
      model_->bookmark_bar_node(), 1, base::ASCIIToUTF16("Test Folder"));
  EXPECT_EQ(2, model_->bookmark_bar_node()->child_count());
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, BookmarkAllUserNodesRemoved(model_.get(), _));
  model_->RemoveAllUserBookmarks();
  EXPECT_EQ(2, managed_node()->child_count());
  EXPECT_EQ(0, model_->bookmark_bar_node()->child_count());
  Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace policy
