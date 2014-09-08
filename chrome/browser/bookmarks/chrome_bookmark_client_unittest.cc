// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/chrome_bookmark_client.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client_factory.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/mock_bookmark_model_observer.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::Mock;
using testing::_;

class ChromeBookmarkClientTest : public testing::Test {
 public:
  ChromeBookmarkClientTest() : client_(NULL), model_(NULL) {}
  virtual ~ChromeBookmarkClientTest() {}

  virtual void SetUp() OVERRIDE {
    prefs_ = profile_.GetTestingPrefService();
    ASSERT_FALSE(prefs_->HasPrefPath(bookmarks::prefs::kManagedBookmarks));

    prefs_->SetManagedPref(bookmarks::prefs::kManagedBookmarks,
                           CreateTestTree());
    ResetModel();

    // The managed node always exists.
    ASSERT_TRUE(client_->managed_node());
    ASSERT_TRUE(client_->managed_node()->parent() == model_->root_node());
    EXPECT_NE(-1, model_->root_node()->GetIndexOf(client_->managed_node()));
  }

  virtual void TearDown() OVERRIDE {
    model_->RemoveObserver(&observer_);
  }

  void ResetModel() {
    profile_.CreateBookmarkModel(false);
    model_ = BookmarkModelFactory::GetForProfile(&profile_);
    test::WaitForBookmarkModelToLoad(model_);
    model_->AddObserver(&observer_);
    client_ = ChromeBookmarkClientFactory::GetForProfile(&profile_);
    DCHECK(client_);
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

  static base::DictionaryValue* CreateExpectedTree() {
    return CreateFolder(GetManagedFolderTitle(), CreateTestTree());
  }

  static std::string GetManagedFolderTitle() {
    return l10n_util::GetStringUTF8(
        IDS_BOOKMARK_BAR_MANAGED_FOLDER_DEFAULT_NAME);
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

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  TestingPrefServiceSyncable* prefs_;
  bookmarks::MockBookmarkModelObserver observer_;
  ChromeBookmarkClient* client_;
  BookmarkModel* model_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBookmarkClientTest);
};

TEST_F(ChromeBookmarkClientTest, EmptyManagedNode) {
  // Verifies that the managed node is empty and invisible when the policy is
  // not set.
  model_->RemoveObserver(&observer_);
  prefs_->RemoveManagedPref(bookmarks::prefs::kManagedBookmarks);
  ResetModel();

  ASSERT_TRUE(client_->managed_node());
  EXPECT_TRUE(client_->managed_node()->empty());
  EXPECT_FALSE(client_->managed_node()->IsVisible());
}

TEST_F(ChromeBookmarkClientTest, LoadInitial) {
  // Verifies that the initial load picks up the initial policy too.
  EXPECT_TRUE(model_->bookmark_bar_node()->empty());
  EXPECT_TRUE(model_->other_node()->empty());
  EXPECT_FALSE(client_->managed_node()->empty());
  EXPECT_TRUE(client_->managed_node()->IsVisible());

  scoped_ptr<base::DictionaryValue> expected(CreateExpectedTree());
  EXPECT_TRUE(NodeMatchesValue(client_->managed_node(), expected.get()));
}

TEST_F(ChromeBookmarkClientTest, SwapNodes) {
  // Swap the Google bookmark with the Folder.
  scoped_ptr<base::ListValue> updated(CreateTestTree());
  scoped_ptr<base::Value> removed;
  ASSERT_TRUE(updated->Remove(0, &removed));
  updated->Append(removed.release());

  // These two nodes should just be swapped.
  const BookmarkNode* parent = client_->managed_node();
  EXPECT_CALL(observer_, BookmarkNodeMoved(model_, parent, 1, parent, 0));
  prefs_->SetManagedPref(bookmarks::prefs::kManagedBookmarks,
                         updated->DeepCopy());
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  scoped_ptr<base::DictionaryValue> expected(
      CreateFolder(GetManagedFolderTitle(), updated.release()));
  EXPECT_TRUE(NodeMatchesValue(client_->managed_node(), expected.get()));
}

TEST_F(ChromeBookmarkClientTest, RemoveNode) {
  // Remove the Folder.
  scoped_ptr<base::ListValue> updated(CreateTestTree());
  ASSERT_TRUE(updated->Remove(1, NULL));

  const BookmarkNode* parent = client_->managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(model_, parent, 1, _, _));
  prefs_->SetManagedPref(bookmarks::prefs::kManagedBookmarks,
                         updated->DeepCopy());
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  scoped_ptr<base::DictionaryValue> expected(
      CreateFolder(GetManagedFolderTitle(), updated.release()));
  EXPECT_TRUE(NodeMatchesValue(client_->managed_node(), expected.get()));
}

TEST_F(ChromeBookmarkClientTest, CreateNewNodes) {
  // Put all the nodes inside another folder.
  scoped_ptr<base::ListValue> updated(new base::ListValue);
  updated->Append(CreateFolder("Container", CreateTestTree()));

  EXPECT_CALL(observer_, BookmarkNodeAdded(model_, _, _)).Times(5);
  // The remaining nodes have been pushed to positions 1 and 2; they'll both be
  // removed when at position 1.
  const BookmarkNode* parent = client_->managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(model_, parent, 1, _, _))
      .Times(2);
  prefs_->SetManagedPref(bookmarks::prefs::kManagedBookmarks,
                         updated->DeepCopy());
  Mock::VerifyAndClearExpectations(&observer_);

  // Verify the final tree.
  scoped_ptr<base::DictionaryValue> expected(
      CreateFolder(GetManagedFolderTitle(), updated.release()));
  EXPECT_TRUE(NodeMatchesValue(client_->managed_node(), expected.get()));
}

TEST_F(ChromeBookmarkClientTest, RemoveAllUserBookmarks) {
  // Remove the policy.
  const BookmarkNode* parent = client_->managed_node();
  EXPECT_CALL(observer_, BookmarkNodeRemoved(model_, parent, 0, _, _))
      .Times(2);
  prefs_->RemoveManagedPref(bookmarks::prefs::kManagedBookmarks);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(client_->managed_node()->empty());
  EXPECT_FALSE(client_->managed_node()->IsVisible());
}

TEST_F(ChromeBookmarkClientTest, IsDescendantOfManagedNode) {
  EXPECT_FALSE(client_->IsDescendantOfManagedNode(model_->root_node()));
  EXPECT_FALSE(client_->IsDescendantOfManagedNode(model_->bookmark_bar_node()));
  EXPECT_FALSE(client_->IsDescendantOfManagedNode(model_->other_node()));
  EXPECT_FALSE(client_->IsDescendantOfManagedNode(model_->mobile_node()));
  EXPECT_TRUE(client_->IsDescendantOfManagedNode(client_->managed_node()));

  const BookmarkNode* parent = client_->managed_node();
  ASSERT_EQ(2, parent->child_count());
  EXPECT_TRUE(client_->IsDescendantOfManagedNode(parent->GetChild(0)));
  EXPECT_TRUE(client_->IsDescendantOfManagedNode(parent->GetChild(1)));

  parent = parent->GetChild(1);
  ASSERT_EQ(2, parent->child_count());
  EXPECT_TRUE(client_->IsDescendantOfManagedNode(parent->GetChild(0)));
  EXPECT_TRUE(client_->IsDescendantOfManagedNode(parent->GetChild(1)));
}

TEST_F(ChromeBookmarkClientTest, RemoveAllDoesntRemoveManaged) {
  EXPECT_EQ(2, client_->managed_node()->child_count());

  EXPECT_CALL(observer_,
              BookmarkNodeAdded(model_, model_->bookmark_bar_node(), 0));
  EXPECT_CALL(observer_,
              BookmarkNodeAdded(model_, model_->bookmark_bar_node(), 1));
  model_->AddURL(model_->bookmark_bar_node(),
                 0,
                 base::ASCIIToUTF16("Test"),
                 GURL("http://google.com/"));
  model_->AddFolder(
      model_->bookmark_bar_node(), 1, base::ASCIIToUTF16("Test Folder"));
  EXPECT_EQ(2, model_->bookmark_bar_node()->child_count());
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, BookmarkAllUserNodesRemoved(model_, _));
  model_->RemoveAllUserBookmarks();
  EXPECT_EQ(2, client_->managed_node()->child_count());
  EXPECT_EQ(0, model_->bookmark_bar_node()->child_count());
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(ChromeBookmarkClientTest, HasDescendantsOfManagedNode) {
  const BookmarkNode* user_node = model_->AddURL(model_->other_node(),
                                                 0,
                                                 base::ASCIIToUTF16("foo bar"),
                                                 GURL("http://www.google.com"));
  const BookmarkNode* managed_node = client_->managed_node()->GetChild(0);
  ASSERT_TRUE(managed_node);

  std::vector<const BookmarkNode*> nodes;
  EXPECT_FALSE(client_->HasDescendantsOfManagedNode(nodes));
  nodes.push_back(user_node);
  EXPECT_FALSE(client_->HasDescendantsOfManagedNode(nodes));
  nodes.push_back(managed_node);
  EXPECT_TRUE(client_->HasDescendantsOfManagedNode(nodes));
}
