// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/hash_tables.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/model_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/tree_node_iterator.h"
#include "ui/base/models/tree_node_model.h"

using base::Time;
using base::TimeDelta;
using content::BrowserThread;

namespace {

// Test cases used to test the removal of extra whitespace when adding
// a new folder/bookmark or updating a title of a folder/bookmark.
static struct {
  const std::string input_title;
  const std::string expected_title;
} whitespace_test_cases[] = {
  {"foobar", "foobar"},
  // Newlines.
  {"foo\nbar", "foo bar"},
  {"foo\n\nbar", "foo bar"},
  {"foo\n\n\nbar", "foo bar"},
  {"foo\r\nbar", "foo bar"},
  {"foo\r\n\r\nbar", "foo bar"},
  {"\nfoo\nbar\n", "foo bar"},
  // Spaces.
  {"foo  bar", "foo bar"},
  {" foo bar ", "foo bar"},
  {"  foo  bar  ", "foo bar"},
  // Tabs.
  {"\tfoo\tbar\t", "foo bar"},
  {"\tfoo bar\t", "foo bar"},
  // Mixed cases.
  {"\tfoo\nbar\t", "foo bar"},
  {"\tfoo\r\nbar\t", "foo bar"},
  {"  foo\tbar\n", "foo bar"},
  {"\t foo \t  bar  \t", "foo bar"},
  {"\n foo\r\n\tbar\n \t", "foo bar"},
};

// Helper to get a mutable bookmark node.
BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

void SwapDateAdded(BookmarkNode* n1, BookmarkNode* n2) {
  Time tmp = n1->date_added();
  n1->set_date_added(n2->date_added());
  n2->set_date_added(tmp);
}

class BookmarkModelTest : public testing::Test,
                          public BookmarkModelObserver {
 public:
  struct ObserverDetails {
    ObserverDetails() {
      Set(NULL, NULL, -1, -1);
    }

    void Set(const BookmarkNode* node1,
             const BookmarkNode* node2,
             int index1,
             int index2) {
      node1_ = node1;
      node2_ = node2;
      index1_ = index1;
      index2_ = index2;
    }

    void ExpectEquals(const BookmarkNode* node1,
                      const BookmarkNode* node2,
                      int index1,
                      int index2) {
      EXPECT_EQ(node1_, node1);
      EXPECT_EQ(node2_, node2);
      EXPECT_EQ(index1_, index1);
      EXPECT_EQ(index2_, index2);
    }

   private:
    const BookmarkNode* node1_;
    const BookmarkNode* node2_;
    int index1_;
    int index2_;
  };

  BookmarkModelTest()
    : model_(NULL) {
    model_.AddObserver(this);
    ClearCounts();
  }

  void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE {
    // We never load from the db, so that this should never get invoked.
    NOTREACHED();
  }

  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) OVERRIDE {
    ++moved_count_;
    observer_details_.Set(old_parent, new_parent, old_index, new_index);
  }

  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE {
    ++added_count_;
    observer_details_.Set(parent, NULL, index, -1);
  }

  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE {
    ++removed_count_;
    observer_details_.Set(parent, NULL, old_index, -1);
  }

  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE {
    ++changed_count_;
    observer_details_.Set(node, NULL, -1, -1);
  }

  virtual void BookmarkNodeChildrenReordered(
      BookmarkModel* model,
      const BookmarkNode* node) OVERRIDE {
    ++reordered_count_;
  }

  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) OVERRIDE {
    // We never attempt to load favicons, so that this method never
    // gets invoked.
  }

  void ClearCounts() {
    added_count_ = moved_count_ = removed_count_ = changed_count_ =
        reordered_count_ = 0;
  }

  void AssertObserverCount(int added_count,
                           int moved_count,
                           int removed_count,
                           int changed_count,
                           int reordered_count) {
    EXPECT_EQ(added_count_, added_count);
    EXPECT_EQ(moved_count_, moved_count);
    EXPECT_EQ(removed_count_, removed_count);
    EXPECT_EQ(changed_count_, changed_count);
    EXPECT_EQ(reordered_count_, reordered_count);
  }

 protected:
  BookmarkModel model_;
  ObserverDetails observer_details_;

 private:
  int added_count_;
  int moved_count_;
  int removed_count_;
  int changed_count_;
  int reordered_count_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelTest);
};

TEST_F(BookmarkModelTest, InitialState) {
  const BookmarkNode* bb_node = model_.bookmark_bar_node();
  ASSERT_TRUE(bb_node != NULL);
  EXPECT_EQ(0, bb_node->child_count());
  EXPECT_EQ(BookmarkNode::BOOKMARK_BAR, bb_node->type());

  const BookmarkNode* other_node = model_.other_node();
  ASSERT_TRUE(other_node != NULL);
  EXPECT_EQ(0, other_node->child_count());
  EXPECT_EQ(BookmarkNode::OTHER_NODE, other_node->type());

  const BookmarkNode* mobile_node = model_.mobile_node();
  ASSERT_TRUE(mobile_node != NULL);
  EXPECT_EQ(0, mobile_node->child_count());
  EXPECT_EQ(BookmarkNode::MOBILE, mobile_node->type());

  EXPECT_TRUE(bb_node->id() != other_node->id());
  EXPECT_TRUE(bb_node->id() != mobile_node->id());
  EXPECT_TRUE(other_node->id() != mobile_node->id());
}

TEST_F(BookmarkModelTest, AddURL) {
  const BookmarkNode* root = model_.bookmark_bar_node();
  const string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");

  const BookmarkNode* new_node = model_.AddURL(root, 0, title, url);
  AssertObserverCount(1, 0, 0, 0, 0);
  observer_details_.ExpectEquals(root, NULL, 0, -1);

  ASSERT_EQ(1, root->child_count());
  ASSERT_EQ(title, new_node->GetTitle());
  ASSERT_TRUE(url == new_node->url());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_TRUE(new_node == model_.GetMostRecentlyAddedNodeForURL(url));

  EXPECT_TRUE(new_node->id() != root->id() &&
              new_node->id() != model_.other_node()->id() &&
              new_node->id() != model_.mobile_node()->id());
}

TEST_F(BookmarkModelTest, AddURLWithWhitespaceTitle) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(whitespace_test_cases); ++i) {
    const BookmarkNode* root = model_.bookmark_bar_node();
    const string16 title(ASCIIToUTF16(whitespace_test_cases[i].input_title));
    const GURL url("http://foo.com");

    const BookmarkNode* new_node = model_.AddURL(root, i, title, url);

    int size = i + 1;
    EXPECT_EQ(size, root->child_count());
    EXPECT_EQ(ASCIIToUTF16(whitespace_test_cases[i].expected_title),
              new_node->GetTitle());
    EXPECT_EQ(BookmarkNode::URL, new_node->type());
  }
}

TEST_F(BookmarkModelTest, AddURLToMobileBookmarks) {
  const BookmarkNode* root = model_.mobile_node();
  const string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");

  const BookmarkNode* new_node = model_.AddURL(root, 0, title, url);
  AssertObserverCount(1, 0, 0, 0, 0);
  observer_details_.ExpectEquals(root, NULL, 0, -1);

  ASSERT_EQ(1, root->child_count());
  ASSERT_EQ(title, new_node->GetTitle());
  ASSERT_TRUE(url == new_node->url());
  ASSERT_EQ(BookmarkNode::URL, new_node->type());
  ASSERT_TRUE(new_node == model_.GetMostRecentlyAddedNodeForURL(url));

  EXPECT_TRUE(new_node->id() != root->id() &&
              new_node->id() != model_.other_node()->id() &&
              new_node->id() != model_.mobile_node()->id());
}

TEST_F(BookmarkModelTest, AddFolder) {
  const BookmarkNode* root = model_.bookmark_bar_node();
  const string16 title(ASCIIToUTF16("foo"));

  const BookmarkNode* new_node = model_.AddFolder(root, 0, title);
  AssertObserverCount(1, 0, 0, 0, 0);
  observer_details_.ExpectEquals(root, NULL, 0, -1);

  ASSERT_EQ(1, root->child_count());
  ASSERT_EQ(title, new_node->GetTitle());
  ASSERT_EQ(BookmarkNode::FOLDER, new_node->type());

  EXPECT_TRUE(new_node->id() != root->id() &&
              new_node->id() != model_.other_node()->id() &&
              new_node->id() != model_.mobile_node()->id());

  // Add another folder, just to make sure folder_ids are incremented correctly.
  ClearCounts();
  model_.AddFolder(root, 0, title);
  AssertObserverCount(1, 0, 0, 0, 0);
  observer_details_.ExpectEquals(root, NULL, 0, -1);
}

TEST_F(BookmarkModelTest, AddFolderWithWhitespaceTitle) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(whitespace_test_cases); ++i) {
    const BookmarkNode* root = model_.bookmark_bar_node();
    const string16 title(ASCIIToUTF16(whitespace_test_cases[i].input_title));

    const BookmarkNode* new_node = model_.AddFolder(root, i, title);

    int size = i + 1;
    EXPECT_EQ(size, root->child_count());
    EXPECT_EQ(ASCIIToUTF16(whitespace_test_cases[i].expected_title),
              new_node->GetTitle());
    EXPECT_EQ(BookmarkNode::FOLDER, new_node->type());
  }
}

TEST_F(BookmarkModelTest, RemoveURL) {
  const BookmarkNode* root = model_.bookmark_bar_node();
  const string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");
  model_.AddURL(root, 0, title, url);
  ClearCounts();

  model_.Remove(root, 0);
  ASSERT_EQ(0, root->child_count());
  AssertObserverCount(0, 0, 1, 0, 0);
  observer_details_.ExpectEquals(root, NULL, 0, -1);

  // Make sure there is no mapping for the URL.
  ASSERT_TRUE(model_.GetMostRecentlyAddedNodeForURL(url) == NULL);
}

TEST_F(BookmarkModelTest, RemoveFolder) {
  const BookmarkNode* root = model_.bookmark_bar_node();
  const BookmarkNode* folder = model_.AddFolder(root, 0, ASCIIToUTF16("foo"));

  ClearCounts();

  // Add a URL as a child.
  const string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");
  model_.AddURL(folder, 0, title, url);

  ClearCounts();

  // Now remove the folder.
  model_.Remove(root, 0);
  ASSERT_EQ(0, root->child_count());
  AssertObserverCount(0, 0, 1, 0, 0);
  observer_details_.ExpectEquals(root, NULL, 0, -1);

  // Make sure there is no mapping for the URL.
  ASSERT_TRUE(model_.GetMostRecentlyAddedNodeForURL(url) == NULL);
}

TEST_F(BookmarkModelTest, SetTitle) {
  const BookmarkNode* root = model_.bookmark_bar_node();
  string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");
  const BookmarkNode* node = model_.AddURL(root, 0, title, url);

  ClearCounts();

  title = ASCIIToUTF16("foo2");
  model_.SetTitle(node, title);
  AssertObserverCount(0, 0, 0, 1, 0);
  observer_details_.ExpectEquals(node, NULL, -1, -1);
  EXPECT_EQ(title, node->GetTitle());
}

TEST_F(BookmarkModelTest, SetTitleWithWhitespace) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(whitespace_test_cases); ++i) {
    const BookmarkNode* root = model_.bookmark_bar_node();
    string16 title(ASCIIToUTF16("dummy"));
    const GURL url("http://foo.com");
    const BookmarkNode* node = model_.AddURL(root, 0, title, url);

    title = ASCIIToUTF16(whitespace_test_cases[i].input_title);
    model_.SetTitle(node, title);
    EXPECT_EQ(ASCIIToUTF16(whitespace_test_cases[i].expected_title),
              node->GetTitle());
  }
}

TEST_F(BookmarkModelTest, SetURL) {
  const BookmarkNode* root = model_.bookmark_bar_node();
  const string16 title(ASCIIToUTF16("foo"));
  GURL url("http://foo.com");
  const BookmarkNode* node = model_.AddURL(root, 0, title, url);

  ClearCounts();

  url = GURL("http://foo2.com");
  model_.SetURL(node, url);
  AssertObserverCount(0, 0, 0, 1, 0);
  observer_details_.ExpectEquals(node, NULL, -1, -1);
  EXPECT_EQ(url, node->url());
}

TEST_F(BookmarkModelTest, Move) {
  const BookmarkNode* root = model_.bookmark_bar_node();
  const string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");
  const BookmarkNode* node = model_.AddURL(root, 0, title, url);
  const BookmarkNode* folder1 = model_.AddFolder(root, 0, ASCIIToUTF16("foo"));
  ClearCounts();

  model_.Move(node, folder1, 0);

  AssertObserverCount(0, 1, 0, 0, 0);
  observer_details_.ExpectEquals(root, folder1, 1, 0);
  EXPECT_TRUE(folder1 == node->parent());
  EXPECT_EQ(1, root->child_count());
  EXPECT_EQ(folder1, root->GetChild(0));
  EXPECT_EQ(1, folder1->child_count());
  EXPECT_EQ(node, folder1->GetChild(0));

  // And remove the folder.
  ClearCounts();
  model_.Remove(root, 0);
  AssertObserverCount(0, 0, 1, 0, 0);
  observer_details_.ExpectEquals(root, NULL, 0, -1);
  EXPECT_TRUE(model_.GetMostRecentlyAddedNodeForURL(url) == NULL);
  EXPECT_EQ(0, root->child_count());
}

TEST_F(BookmarkModelTest, Copy) {
  const BookmarkNode* root = model_.bookmark_bar_node();
  static const std::string model_string("a 1:[ b c ] d 2:[ e f g ] h ");
  model_test_utils::AddNodesFromModelString(model_, root, model_string);

  // Validate initial model.
  std::string actualModelString = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(model_string, actualModelString);

  // Copy 'd' to be after '1:b': URL item from bar to folder.
  const BookmarkNode* nodeToCopy = root->GetChild(2);
  const BookmarkNode* destination = root->GetChild(1);
  model_.Copy(nodeToCopy, destination, 1);
  actualModelString = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ("a 1:[ b d c ] d 2:[ e f g ] h ", actualModelString);

  // Copy '1:d' to be after 'a': URL item from folder to bar.
  const BookmarkNode* folder = root->GetChild(1);
  nodeToCopy = folder->GetChild(1);
  model_.Copy(nodeToCopy, root, 1);
  actualModelString = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ("a d 1:[ b d c ] d 2:[ e f g ] h ", actualModelString);

  // Copy '1' to be after '2:e': Folder from bar to folder.
  nodeToCopy = root->GetChild(2);
  destination = root->GetChild(4);
  model_.Copy(nodeToCopy, destination, 1);
  actualModelString = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ("a d 1:[ b d c ] d 2:[ e 1:[ b d c ] f g ] h ", actualModelString);

  // Copy '2:1' to be after '2:f': Folder within same folder.
  folder = root->GetChild(4);
  nodeToCopy = folder->GetChild(1);
  model_.Copy(nodeToCopy, folder, 3);
  actualModelString = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ("a d 1:[ b d c ] d 2:[ e 1:[ b d c ] f 1:[ b d c ] g ] h ",
            actualModelString);

  // Copy first 'd' to be after 'h': URL item within the bar.
  nodeToCopy = root->GetChild(1);
  model_.Copy(nodeToCopy, root, 6);
  actualModelString = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ("a d 1:[ b d c ] d 2:[ e 1:[ b d c ] f 1:[ b d c ] g ] h d ",
            actualModelString);

  // Copy '2' to be after 'a': Folder within the bar.
  nodeToCopy = root->GetChild(4);
  model_.Copy(nodeToCopy, root, 1);
  actualModelString = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ("a 2:[ e 1:[ b d c ] f 1:[ b d c ] g ] d 1:[ b d c ] "
            "d 2:[ e 1:[ b d c ] f 1:[ b d c ] g ] h d ",
            actualModelString);
}

// Tests that adding a URL to a folder updates the last modified time.
TEST_F(BookmarkModelTest, ParentForNewNodes) {
  ASSERT_EQ(model_.bookmark_bar_node(), model_.GetParentForNewNodes());

  const string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");

  model_.AddURL(model_.other_node(), 0, title, url);
  ASSERT_EQ(model_.other_node(), model_.GetParentForNewNodes());
}

// Tests that adding a URL to a folder updates the last modified time.
TEST_F(BookmarkModelTest, ParentForNewMobileNodes) {
  ASSERT_EQ(model_.bookmark_bar_node(), model_.GetParentForNewNodes());

  const string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");

  model_.AddURL(model_.mobile_node(), 0, title, url);
  ASSERT_EQ(model_.mobile_node(), model_.GetParentForNewNodes());
}

// Make sure recently modified stays in sync when adding a URL.
TEST_F(BookmarkModelTest, MostRecentlyModifiedFolders) {
  // Add a folder.
  const BookmarkNode* folder = model_.AddFolder(model_.other_node(), 0,
                                                 ASCIIToUTF16("foo"));
  // Add a URL to it.
  model_.AddURL(folder, 0, ASCIIToUTF16("blah"), GURL("http://foo.com"));

  // Make sure folder is in the most recently modified.
  std::vector<const BookmarkNode*> most_recent_folders =
      bookmark_utils::GetMostRecentlyModifiedFolders(&model_, 1);
  ASSERT_EQ(1U, most_recent_folders.size());
  ASSERT_EQ(folder, most_recent_folders[0]);

  // Nuke the folder and do another fetch, making sure folder isn't in the
  // returned list.
  model_.Remove(folder->parent(), 0);
  most_recent_folders =
      bookmark_utils::GetMostRecentlyModifiedFolders(&model_, 1);
  ASSERT_EQ(1U, most_recent_folders.size());
  ASSERT_TRUE(most_recent_folders[0] != folder);
}

// Make sure MostRecentlyAddedEntries stays in sync.
TEST_F(BookmarkModelTest, MostRecentlyAddedEntries) {
  // Add a couple of nodes such that the following holds for the time of the
  // nodes: n1 > n2 > n3 > n4.
  Time base_time = Time::Now();
  BookmarkNode* n1 = AsMutable(model_.AddURL(model_.bookmark_bar_node(),
                                             0,
                                             ASCIIToUTF16("blah"),
                                             GURL("http://foo.com/0")));
  BookmarkNode* n2 = AsMutable(model_.AddURL(model_.bookmark_bar_node(),
                                             1,
                                             ASCIIToUTF16("blah"),
                                             GURL("http://foo.com/1")));
  BookmarkNode* n3 = AsMutable(model_.AddURL(model_.bookmark_bar_node(),
                                             2,
                                             ASCIIToUTF16("blah"),
                                             GURL("http://foo.com/2")));
  BookmarkNode* n4 = AsMutable(model_.AddURL(model_.bookmark_bar_node(),
                                             3,
                                             ASCIIToUTF16("blah"),
                                             GURL("http://foo.com/3")));
  n1->set_date_added(base_time + TimeDelta::FromDays(4));
  n2->set_date_added(base_time + TimeDelta::FromDays(3));
  n3->set_date_added(base_time + TimeDelta::FromDays(2));
  n4->set_date_added(base_time + TimeDelta::FromDays(1));

  // Make sure order is honored.
  std::vector<const BookmarkNode*> recently_added;
  bookmark_utils::GetMostRecentlyAddedEntries(&model_, 2, &recently_added);
  ASSERT_EQ(2U, recently_added.size());
  ASSERT_TRUE(n1 == recently_added[0]);
  ASSERT_TRUE(n2 == recently_added[1]);

  // swap 1 and 2, then check again.
  recently_added.clear();
  SwapDateAdded(n1, n2);
  bookmark_utils::GetMostRecentlyAddedEntries(&model_, 4, &recently_added);
  ASSERT_EQ(4U, recently_added.size());
  ASSERT_TRUE(n2 == recently_added[0]);
  ASSERT_TRUE(n1 == recently_added[1]);
  ASSERT_TRUE(n3 == recently_added[2]);
  ASSERT_TRUE(n4 == recently_added[3]);
}

// Makes sure GetMostRecentlyAddedNodeForURL stays in sync.
TEST_F(BookmarkModelTest, GetMostRecentlyAddedNodeForURL) {
  // Add a couple of nodes such that the following holds for the time of the
  // nodes: n1 > n2
  Time base_time = Time::Now();
  const GURL url("http://foo.com/0");
  BookmarkNode* n1 = AsMutable(model_.AddURL(
      model_.bookmark_bar_node(), 0, ASCIIToUTF16("blah"), url));
  BookmarkNode* n2 = AsMutable(model_.AddURL(
      model_.bookmark_bar_node(), 1, ASCIIToUTF16("blah"), url));
  n1->set_date_added(base_time + TimeDelta::FromDays(4));
  n2->set_date_added(base_time + TimeDelta::FromDays(3));

  // Make sure order is honored.
  ASSERT_EQ(n1, model_.GetMostRecentlyAddedNodeForURL(url));

  // swap 1 and 2, then check again.
  SwapDateAdded(n1, n2);
  ASSERT_EQ(n2, model_.GetMostRecentlyAddedNodeForURL(url));
}

// Makes sure GetBookmarks removes duplicates.
TEST_F(BookmarkModelTest, GetBookmarksWithDups) {
  const GURL url("http://foo.com/0");
  model_.AddURL(model_.bookmark_bar_node(), 0, ASCIIToUTF16("blah"), url);
  model_.AddURL(model_.bookmark_bar_node(), 1, ASCIIToUTF16("blah"), url);

  std::vector<GURL> urls;
  model_.GetBookmarks(&urls);
  EXPECT_EQ(1U, urls.size());
  ASSERT_TRUE(urls[0] == url);
}

TEST_F(BookmarkModelTest, HasBookmarks) {
  const GURL url("http://foo.com/");
  model_.AddURL(model_.bookmark_bar_node(), 0, ASCIIToUTF16("bar"), url);

  EXPECT_TRUE(model_.HasBookmarks());
}

// content::NotificationObserver implementation used in verifying we've received
// the NOTIFY_URLS_STARRED method correctly.
class StarredListener : public content::NotificationObserver {
 public:
  StarredListener() : notification_count_(0), details_(false) {
    registrar_.Add(this, chrome::NOTIFICATION_URLS_STARRED,
                   content::Source<Profile>(NULL));
  }

  // NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_URLS_STARRED) {
      notification_count_++;
      details_ =
          *(content::Details<history::URLsStarredDetails>(details).ptr());
    }
  }

  // Number of times NOTIFY_URLS_STARRED has been observed.
  int notification_count_;

  // Details from the last NOTIFY_URLS_STARRED.
  history::URLsStarredDetails details_;

 private:
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(StarredListener);
};

// Makes sure NOTIFY_URLS_STARRED is sent correctly.
TEST_F(BookmarkModelTest, NotifyURLsStarred) {
  StarredListener listener;
  const GURL url("http://foo.com/0");
  const BookmarkNode* n1 = model_.AddURL(
      model_.bookmark_bar_node(), 0, ASCIIToUTF16("blah"), url);

  // Starred notification should be sent.
  EXPECT_EQ(1, listener.notification_count_);
  ASSERT_TRUE(listener.details_.starred);
  ASSERT_EQ(1U, listener.details_.changed_urls.size());
  EXPECT_TRUE(url == *(listener.details_.changed_urls.begin()));
  listener.notification_count_ = 0;
  listener.details_.changed_urls.clear();

  // Add another bookmark for the same URL. This should not send any
  // notification.
  const BookmarkNode* n2 = model_.AddURL(
      model_.bookmark_bar_node(), 1, ASCIIToUTF16("blah"), url);

  EXPECT_EQ(0, listener.notification_count_);

  // Remove n2.
  model_.Remove(n2->parent(), 1);
  n2 = NULL;

  // Shouldn't have received any notification as n1 still exists with the same
  // URL.
  EXPECT_EQ(0, listener.notification_count_);

  EXPECT_TRUE(model_.GetMostRecentlyAddedNodeForURL(url) == n1);

  // Remove n1.
  model_.Remove(n1->parent(), 0);

  // Now we should get the notification.
  EXPECT_EQ(1, listener.notification_count_);
  ASSERT_FALSE(listener.details_.starred);
  ASSERT_EQ(1U, listener.details_.changed_urls.size());
  EXPECT_TRUE(url == *(listener.details_.changed_urls.begin()));
}

// See comment in PopulateNodeFromString.
typedef ui::TreeNodeWithValue<BookmarkNode::Type> TestNode;

// Does the work of PopulateNodeFromString. index gives the index of the current
// element in description to process.
void PopulateNodeImpl(const std::vector<std::string>& description,
                      size_t* index,
                      TestNode* parent) {
  while (*index < description.size()) {
    const std::string& element = description[*index];
    (*index)++;
    if (element == "[") {
      // Create a new folder and recurse to add all the children.
      // Folders are given a unique named by way of an ever increasing integer
      // value. The folders need not have a name, but one is assigned to help
      // in debugging.
      static int next_folder_id = 1;
      TestNode* new_node =
          new TestNode(base::IntToString16(next_folder_id++),
                       BookmarkNode::FOLDER);
      parent->Add(new_node, parent->child_count());
      PopulateNodeImpl(description, index, new_node);
    } else if (element == "]") {
      // End the current folder.
      return;
    } else {
      // Add a new URL.

      // All tokens must be space separated. If there is a [ or ] in the name it
      // likely means a space was forgotten.
      DCHECK(element.find('[') == std::string::npos);
      DCHECK(element.find(']') == std::string::npos);
      parent->Add(new TestNode(UTF8ToUTF16(element), BookmarkNode::URL),
                  parent->child_count());
    }
  }
}

// Creates and adds nodes to parent based on description. description consists
// of the following tokens (all space separated):
//   [ : creates a new USER_FOLDER node. All elements following the [ until the
//       next balanced ] is encountered are added as children to the node.
//   ] : closes the last folder created by [ so that any further nodes are added
//       to the current folders parent.
//   text: creates a new URL node.
// For example, "a [b] c" creates the following nodes:
//   a 1 c
//     |
//     b
// In words: a node of type URL with the title a, followed by a folder node with
// the title 1 having the single child of type url with name b, followed by
// the url node with the title c.
//
// NOTE: each name must be unique, and folders are assigned a unique title by
// way of an increasing integer.
void PopulateNodeFromString(const std::string& description, TestNode* parent) {
  std::vector<std::string> elements;
  base::SplitStringAlongWhitespace(description, &elements);
  size_t index = 0;
  PopulateNodeImpl(elements, &index, parent);
}

// Populates the BookmarkNode with the children of parent.
void PopulateBookmarkNode(TestNode* parent,
                          BookmarkModel* model,
                          const BookmarkNode* bb_node) {
  for (int i = 0; i < parent->child_count(); ++i) {
    TestNode* child = parent->GetChild(i);
    if (child->value == BookmarkNode::FOLDER) {
      const BookmarkNode* new_bb_node =
          model->AddFolder(bb_node, i, child->GetTitle());
      PopulateBookmarkNode(child, model, new_bb_node);
    } else {
      model->AddURL(bb_node, i, child->GetTitle(),
          GURL("http://" + UTF16ToASCII(child->GetTitle())));
    }
  }
}

// Test class that creates a BookmarkModel with a real history backend.
class BookmarkModelTestWithProfile : public testing::Test {
 public:
  BookmarkModelTestWithProfile()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {}

  // testing::Test:
  virtual void TearDown() OVERRIDE {
    profile_.reset(NULL);
  }

 protected:
  // Verifies the contents of the bookmark bar node match the contents of the
  // TestNode.
  void VerifyModelMatchesNode(TestNode* expected, const BookmarkNode* actual) {
    ASSERT_EQ(expected->child_count(), actual->child_count());
    for (int i = 0; i < expected->child_count(); ++i) {
      TestNode* expected_child = expected->GetChild(i);
      const BookmarkNode* actual_child = actual->GetChild(i);
      ASSERT_EQ(expected_child->GetTitle(), actual_child->GetTitle());
      if (expected_child->value == BookmarkNode::FOLDER) {
        ASSERT_TRUE(actual_child->type() == BookmarkNode::FOLDER);
        // Recurse throught children.
        VerifyModelMatchesNode(expected_child, actual_child);
        if (HasFatalFailure())
          return;
      } else {
        // No need to check the URL, just the title is enough.
        ASSERT_TRUE(actual_child->is_url());
      }
    }
  }

  void VerifyNoDuplicateIDs(BookmarkModel* model) {
    ui::TreeNodeIterator<const BookmarkNode> it(model->root_node());
    base::hash_set<int64> ids;
    while (it.has_next())
      ASSERT_TRUE(ids.insert(it.Next()->id()).second);
  }

  void BlockTillBookmarkModelLoaded() {
    bb_model_ = profile_->GetBookmarkModel();
    profile_->BlockUntilBookmarkModelLoaded();
  }

  // Destroys the current profile, creates a new one and creates the history
  // service.
  void RecreateProfile() {
    // Need to shutdown the old one before creating a new one.
    profile_.reset(NULL);
    profile_.reset(new TestingProfile());
    profile_->CreateHistoryService(true, false);
  }

  // The profile.
  scoped_ptr<TestingProfile> profile_;
  BookmarkModel* bb_model_;

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
};

// Creates a set of nodes in the bookmark bar model, then recreates the
// bookmark bar model which triggers loading from the db and checks the loaded
// structure to make sure it is what we first created.
TEST_F(BookmarkModelTestWithProfile, CreateAndRestore) {
  struct TestData {
    // Structure of the children of the bookmark bar model node.
    const std::string bbn_contents;
    // Structure of the children of the other node.
    const std::string other_contents;
    // Structure of the children of the synced node.
    const std::string mobile_contents;
  } data[] = {
    // See PopulateNodeFromString for a description of these strings.
    { "", "" },
    { "a", "b" },
    { "a [ b ]", "" },
    { "", "[ b ] a [ c [ d e [ f ] ] ]" },
    { "a [ b ]", "" },
    { "a b c [ d e [ f ] ]", "g h i [ j k [ l ] ]"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    // Recreate the profile. We need to reset with NULL first so that the last
    // HistoryService releases the locks on the files it creates and we can
    // delete them.
    profile_.reset(NULL);
    profile_.reset(new TestingProfile());
    profile_->CreateBookmarkModel(true);
    profile_->CreateHistoryService(true, false);
    BlockTillBookmarkModelLoaded();

    TestNode bbn;
    PopulateNodeFromString(data[i].bbn_contents, &bbn);
    PopulateBookmarkNode(&bbn, bb_model_, bb_model_->bookmark_bar_node());

    TestNode other;
    PopulateNodeFromString(data[i].other_contents, &other);
    PopulateBookmarkNode(&other, bb_model_, bb_model_->other_node());

    TestNode mobile;
    PopulateNodeFromString(data[i].mobile_contents, &mobile);
    PopulateBookmarkNode(&mobile, bb_model_, bb_model_->mobile_node());

    profile_->CreateBookmarkModel(false);
    BlockTillBookmarkModelLoaded();

    VerifyModelMatchesNode(&bbn, bb_model_->bookmark_bar_node());
    VerifyModelMatchesNode(&other, bb_model_->other_node());
    VerifyModelMatchesNode(&mobile, bb_model_->mobile_node());
    VerifyNoDuplicateIDs(bb_model_);
  }
}

// Test class that creates a BookmarkModel with a real history backend.
class BookmarkModelTestWithProfile2 : public BookmarkModelTestWithProfile {
 public:
  virtual void SetUp() {
    profile_.reset(new TestingProfile());
  }

 protected:
  // Verifies the state of the model matches that of the state in the saved
  // history file.
  void VerifyExpectedState() {
    // Here's the structure we expect:
    // bbn
    //   www.google.com - Google
    //   F1
    //     http://www.google.com/intl/en/ads/ - Google Advertising
    //     F11
    //       http://www.google.com/services/ - Google Business Solutions
    // other
    //   OF1
    //   http://www.google.com/intl/en/about.html - About Google
    const BookmarkNode* bbn = bb_model_->bookmark_bar_node();
    ASSERT_EQ(2, bbn->child_count());

    const BookmarkNode* child = bbn->GetChild(0);
    ASSERT_EQ(BookmarkNode::URL, child->type());
    ASSERT_EQ(ASCIIToUTF16("Google"), child->GetTitle());
    ASSERT_TRUE(child->url() == GURL("http://www.google.com"));

    child = bbn->GetChild(1);
    ASSERT_TRUE(child->is_folder());
    ASSERT_EQ(ASCIIToUTF16("F1"), child->GetTitle());
    ASSERT_EQ(2, child->child_count());

    const BookmarkNode* parent = child;
    child = parent->GetChild(0);
    ASSERT_EQ(BookmarkNode::URL, child->type());
    ASSERT_EQ(ASCIIToUTF16("Google Advertising"), child->GetTitle());
    ASSERT_TRUE(child->url() == GURL("http://www.google.com/intl/en/ads/"));

    child = parent->GetChild(1);
    ASSERT_TRUE(child->is_folder());
    ASSERT_EQ(ASCIIToUTF16("F11"), child->GetTitle());
    ASSERT_EQ(1, child->child_count());

    parent = child;
    child = parent->GetChild(0);
    ASSERT_EQ(BookmarkNode::URL, child->type());
    ASSERT_EQ(ASCIIToUTF16("Google Business Solutions"), child->GetTitle());
    ASSERT_TRUE(child->url() == GURL("http://www.google.com/services/"));

    parent = bb_model_->other_node();
    ASSERT_EQ(2, parent->child_count());

    child = parent->GetChild(0);
    ASSERT_TRUE(child->is_folder());
    ASSERT_EQ(ASCIIToUTF16("OF1"), child->GetTitle());
    ASSERT_EQ(0, child->child_count());

    child = parent->GetChild(1);
    ASSERT_EQ(BookmarkNode::URL, child->type());
    ASSERT_EQ(ASCIIToUTF16("About Google"), child->GetTitle());
    ASSERT_TRUE(child->url() ==
                GURL("http://www.google.com/intl/en/about.html"));

    parent = bb_model_->mobile_node();
    ASSERT_EQ(0, parent->child_count());

    ASSERT_TRUE(bb_model_->IsBookmarked(GURL("http://www.google.com")));
  }

  void VerifyUniqueIDs() {
    std::set<int64> ids;
    bool has_unique = true;
    VerifyUniqueIDImpl(bb_model_->bookmark_bar_node(), &ids, &has_unique);
    VerifyUniqueIDImpl(bb_model_->other_node(), &ids, &has_unique);
    ASSERT_TRUE(has_unique);
  }

 private:
  void VerifyUniqueIDImpl(const BookmarkNode* node,
                          std::set<int64>* ids,
                          bool* has_unique) {
    if (!*has_unique)
      return;
    if (ids->count(node->id()) != 0) {
      *has_unique = false;
      return;
    }
    ids->insert(node->id());
    for (int i = 0; i < node->child_count(); ++i) {
      VerifyUniqueIDImpl(node->GetChild(i), ids, has_unique);
      if (!*has_unique)
        return;
    }
  }
};

// Tests migrating bookmarks from db into file. This copies an old history db
// file containing bookmarks and make sure they are loaded correctly and
// persisted correctly.
TEST_F(BookmarkModelTestWithProfile2, MigrateFromDBToFileTest) {
  // Copy db file over that contains starred table.
  FilePath old_history_path;
  PathService::Get(chrome::DIR_TEST_DATA, &old_history_path);
  old_history_path = old_history_path.AppendASCII("bookmarks");
  old_history_path = old_history_path.AppendASCII("History_with_starred");
  FilePath new_history_path = profile_->GetPath();
  file_util::Delete(new_history_path, true);
  file_util::CreateDirectory(new_history_path);
  FilePath new_history_file = new_history_path.Append(
      chrome::kHistoryFilename);
  ASSERT_TRUE(file_util::CopyFile(old_history_path, new_history_file));

  // Create the history service making sure it doesn't blow away the file we
  // just copied.
  profile_->CreateHistoryService(false, false);
  profile_->CreateBookmarkModel(true);
  BlockTillBookmarkModelLoaded();

  // Make sure we loaded OK.
  VerifyExpectedState();
  if (HasFatalFailure())
    return;

  // Make sure the ids are unique.
  VerifyUniqueIDs();
  if (HasFatalFailure())
    return;

  // Create again. This time we shouldn't load from history at all.
  profile_->CreateBookmarkModel(false);
  BlockTillBookmarkModelLoaded();

  // Make sure we loaded OK.
  VerifyExpectedState();
  if (HasFatalFailure())
    return;

  VerifyUniqueIDs();
  if (HasFatalFailure())
    return;

  // Recreate the history service (with a clean db). Do this just to make sure
  // we're loading correctly from the bookmarks file.
  profile_->CreateHistoryService(true, false);
  profile_->CreateBookmarkModel(false);
  BlockTillBookmarkModelLoaded();
  VerifyExpectedState();
  VerifyUniqueIDs();
}

// Simple test that removes a bookmark. This test exercises the code paths in
// History that block till bookmark bar model is loaded.
TEST_F(BookmarkModelTestWithProfile2, RemoveNotification) {
  profile_->CreateHistoryService(false, false);
  profile_->CreateBookmarkModel(true);
  BlockTillBookmarkModelLoaded();

  // Add a URL.
  GURL url("http://www.google.com");
  bookmark_utils::AddIfNotBookmarked(bb_model_, url, string16());

  profile_->GetHistoryService(Profile::EXPLICIT_ACCESS)->AddPage(
      url, NULL, 1, GURL(), content::PAGE_TRANSITION_TYPED,
      history::RedirectList(), history::SOURCE_BROWSED, false);

  // This won't actually delete the URL, rather it'll empty out the visits.
  // This triggers blocking on the BookmarkModel.
  profile_->GetHistoryService(Profile::EXPLICIT_ACCESS)->DeleteURL(url);
}

TEST_F(BookmarkModelTest, Sort) {
  // Populate the bookmark bar node with nodes for 'B', 'a', 'd' and 'C'.
  // 'C' and 'a' are folders.
  TestNode bbn;
  PopulateNodeFromString("B [ a ] d [ a ]", &bbn);
  const BookmarkNode* parent = model_.bookmark_bar_node();
  PopulateBookmarkNode(&bbn, &model_, parent);

  BookmarkNode* child1 = AsMutable(parent->GetChild(1));
  child1->set_title(ASCIIToUTF16("a"));
  delete child1->Remove(child1->GetChild(0));
  BookmarkNode* child3 = AsMutable(parent->GetChild(3));
  child3->set_title(ASCIIToUTF16("C"));
  delete child3->Remove(child3->GetChild(0));

  ClearCounts();

  // Sort the children of the bookmark bar node.
  model_.SortChildren(parent);

  // Make sure we were notified.
  AssertObserverCount(0, 0, 0, 0, 1);

  // Make sure the order matches (remember, 'a' and 'C' are folders and
  // come first).
  EXPECT_EQ(parent->GetChild(0)->GetTitle(), ASCIIToUTF16("a"));
  EXPECT_EQ(parent->GetChild(1)->GetTitle(), ASCIIToUTF16("C"));
  EXPECT_EQ(parent->GetChild(2)->GetTitle(), ASCIIToUTF16("B"));
  EXPECT_EQ(parent->GetChild(3)->GetTitle(), ASCIIToUTF16("d"));
}

TEST_F(BookmarkModelTest, NodeVisibility) {
  EXPECT_TRUE(model_.bookmark_bar_node()->IsVisible());
  EXPECT_TRUE(model_.other_node()->IsVisible());
  // Mobile node invisible by default
  EXPECT_FALSE(model_.mobile_node()->IsVisible());

  // Arbitrary node should be visible
  TestNode bbn;
  PopulateNodeFromString("B", &bbn);
  const BookmarkNode* parent = model_.bookmark_bar_node();
  PopulateBookmarkNode(&bbn, &model_, parent);
  EXPECT_TRUE(parent->GetChild(0)->IsVisible());
}

TEST_F(BookmarkModelTest, MobileNodeVisibileWithChildren) {
  const BookmarkNode* root = model_.mobile_node();
  const string16 title(ASCIIToUTF16("foo"));
  const GURL url("http://foo.com");

  model_.AddURL(root, 0, title, url);
  EXPECT_TRUE(model_.mobile_node()->IsVisible());
}

}  // namespace
