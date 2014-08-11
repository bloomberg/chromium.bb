// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_utils.h"

#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

using base::ASCIIToUTF16;
using std::string;

namespace bookmarks {
namespace {

class BookmarkUtilsTest : public testing::Test,
                          public BaseBookmarkModelObserver {
 public:
  BookmarkUtilsTest()
      : grouped_changes_beginning_count_(0),
        grouped_changes_ended_count_(0) {}
  virtual ~BookmarkUtilsTest() {}

// Copy and paste is not yet supported on iOS. http://crbug.com/228147
#if !defined(OS_IOS)
  virtual void TearDown() OVERRIDE {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }
#endif  // !defined(OS_IOS)

  // Certain user actions require multiple changes to the bookmark model,
  // however these modifications need to be atomic for the undo framework. The
  // BaseBookmarkModelObserver is used to inform the boundaries of the user
  // action. For example, when multiple bookmarks are cut to the clipboard we
  // expect one call each to GroupedBookmarkChangesBeginning/Ended.
  void ExpectGroupedChangeCount(int expected_beginning_count,
                                int expected_ended_count) {
    // The undo framework is not used under Android.  Thus the group change
    // events will not be fired and so should not be tested for Android.
#if !defined(OS_ANDROID)
    EXPECT_EQ(grouped_changes_beginning_count_, expected_beginning_count);
    EXPECT_EQ(grouped_changes_ended_count_, expected_ended_count);
#endif
  }

 private:
  // BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged() OVERRIDE {}

  virtual void GroupedBookmarkChangesBeginning(BookmarkModel* model) OVERRIDE {
    ++grouped_changes_beginning_count_;
  }

  virtual void GroupedBookmarkChangesEnded(BookmarkModel* model) OVERRIDE {
    ++grouped_changes_ended_count_;
  }

  int grouped_changes_beginning_count_;
  int grouped_changes_ended_count_;

  // Clipboard requires a message loop.
  base::MessageLoopForUI loop_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkUtilsTest);
};

TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesWordPhraseQuery) {
  test::TestBookmarkClient client;
  scoped_ptr<BookmarkModel> model(client.CreateModel(false));
  const BookmarkNode* node1 = model->AddURL(model->other_node(),
                                            0,
                                            ASCIIToUTF16("foo bar"),
                                            GURL("http://www.google.com"));
  const BookmarkNode* node2 = model->AddURL(model->other_node(),
                                            0,
                                            ASCIIToUTF16("baz buz"),
                                            GURL("http://www.cnn.com"));
  const BookmarkNode* folder1 =
      model->AddFolder(model->other_node(), 0, ASCIIToUTF16("foo"));
  std::vector<const BookmarkNode*> nodes;
  QueryFields query;
  query.word_phrase_query.reset(new base::string16);
  // No nodes are returned for empty string.
  *query.word_phrase_query = ASCIIToUTF16("");
  GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
  EXPECT_TRUE(nodes.empty());
  nodes.clear();

  // No nodes are returned for space-only string.
  *query.word_phrase_query = ASCIIToUTF16("   ");
  GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
  EXPECT_TRUE(nodes.empty());
  nodes.clear();

  // Node "foo bar" and folder "foo" are returned in search results.
  *query.word_phrase_query = ASCIIToUTF16("foo");
  GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
  ASSERT_EQ(2U, nodes.size());
  EXPECT_TRUE(nodes[0] == folder1);
  EXPECT_TRUE(nodes[1] == node1);
  nodes.clear();

  // Ensure url matches return in search results.
  *query.word_phrase_query = ASCIIToUTF16("cnn");
  GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node2);
  nodes.clear();

  // Ensure folder "foo" is not returned in more specific search.
  *query.word_phrase_query = ASCIIToUTF16("foo bar");
  GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node1);
  nodes.clear();

  // Bookmark Bar and Other Bookmarks are not returned in search results.
  *query.word_phrase_query = ASCIIToUTF16("Bookmark");
  GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
  ASSERT_EQ(0U, nodes.size());
  nodes.clear();
}

// Check exact matching against a URL query.
TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesUrl) {
  test::TestBookmarkClient client;
  scoped_ptr<BookmarkModel> model(client.CreateModel(false));
  const BookmarkNode* node1 = model->AddURL(model->other_node(),
                                            0,
                                            ASCIIToUTF16("Google"),
                                            GURL("https://www.google.com/"));
  model->AddURL(model->other_node(),
                0,
                ASCIIToUTF16("Google Calendar"),
                GURL("https://www.google.com/calendar"));

  model->AddFolder(model->other_node(), 0, ASCIIToUTF16("Folder"));

  std::vector<const BookmarkNode*> nodes;
  QueryFields query;
  query.url.reset(new base::string16);
  *query.url = ASCIIToUTF16("https://www.google.com/");
  GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node1);
  nodes.clear();

  *query.url = ASCIIToUTF16("calendar");
  GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
  ASSERT_EQ(0U, nodes.size());
  nodes.clear();

  // Empty URL should not match folders.
  *query.url = ASCIIToUTF16("");
  GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
  ASSERT_EQ(0U, nodes.size());
  nodes.clear();
}

// Check exact matching against a title query.
TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesTitle) {
  test::TestBookmarkClient client;
  scoped_ptr<BookmarkModel> model(client.CreateModel(false));
  const BookmarkNode* node1 = model->AddURL(model->other_node(),
                                            0,
                                            ASCIIToUTF16("Google"),
                                            GURL("https://www.google.com/"));
  model->AddURL(model->other_node(),
                0,
                ASCIIToUTF16("Google Calendar"),
                GURL("https://www.google.com/calendar"));

  const BookmarkNode* folder1 =
      model->AddFolder(model->other_node(), 0, ASCIIToUTF16("Folder"));

  std::vector<const BookmarkNode*> nodes;
  QueryFields query;
  query.title.reset(new base::string16);
  *query.title = ASCIIToUTF16("Google");
  GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node1);
  nodes.clear();

  *query.title = ASCIIToUTF16("Calendar");
  GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
  ASSERT_EQ(0U, nodes.size());
  nodes.clear();

  // Title should match folders.
  *query.title = ASCIIToUTF16("Folder");
  GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == folder1);
  nodes.clear();
}

// Check matching against a query with multiple predicates.
TEST_F(BookmarkUtilsTest, GetBookmarksMatchingPropertiesConjunction) {
  test::TestBookmarkClient client;
  scoped_ptr<BookmarkModel> model(client.CreateModel(false));
  const BookmarkNode* node1 = model->AddURL(model->other_node(),
                                            0,
                                            ASCIIToUTF16("Google"),
                                            GURL("https://www.google.com/"));
  model->AddURL(model->other_node(),
                0,
                ASCIIToUTF16("Google Calendar"),
                GURL("https://www.google.com/calendar"));

  model->AddFolder(model->other_node(), 0, ASCIIToUTF16("Folder"));

  std::vector<const BookmarkNode*> nodes;
  QueryFields query;

  // Test all fields matching.
  query.word_phrase_query.reset(new base::string16(ASCIIToUTF16("www")));
  query.url.reset(new base::string16(ASCIIToUTF16("https://www.google.com/")));
  query.title.reset(new base::string16(ASCIIToUTF16("Google")));
  GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == node1);
  nodes.clear();

  scoped_ptr<base::string16>* fields[] = {
    &query.word_phrase_query, &query.url, &query.title };

  // Test two fields matching.
  for (size_t i = 0; i < arraysize(fields); i++) {
    scoped_ptr<base::string16> original_value(fields[i]->release());
    GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
    ASSERT_EQ(1U, nodes.size());
    EXPECT_TRUE(nodes[0] == node1);
    nodes.clear();
    fields[i]->reset(original_value.release());
  }

  // Test two fields matching with one non-matching field.
  for (size_t i = 0; i < arraysize(fields); i++) {
    scoped_ptr<base::string16> original_value(fields[i]->release());
    fields[i]->reset(new base::string16(ASCIIToUTF16("fjdkslafjkldsa")));
    GetBookmarksMatchingProperties(model.get(), query, 100, string(), &nodes);
    ASSERT_EQ(0U, nodes.size());
    nodes.clear();
    fields[i]->reset(original_value.release());
  }
}

// Copy and paste is not yet supported on iOS. http://crbug.com/228147
#if !defined(OS_IOS)
TEST_F(BookmarkUtilsTest, CopyPaste) {
  test::TestBookmarkClient client;
  scoped_ptr<BookmarkModel> model(client.CreateModel(false));
  const BookmarkNode* node = model->AddURL(model->other_node(),
                                           0,
                                           ASCIIToUTF16("foo bar"),
                                           GURL("http://www.google.com"));

  // Copy a node to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  CopyToClipboard(model.get(), nodes, false);

  // And make sure we can paste a bookmark from the clipboard.
  EXPECT_TRUE(CanPasteFromClipboard(model.get(), model->bookmark_bar_node()));

  // Write some text to the clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(
        ui::Clipboard::GetForCurrentThread(),
        ui::CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteText(ASCIIToUTF16("foo"));
  }

  // Now we shouldn't be able to paste from the clipboard.
  EXPECT_FALSE(CanPasteFromClipboard(model.get(), model->bookmark_bar_node()));
}

TEST_F(BookmarkUtilsTest, CopyPasteMetaInfo) {
  test::TestBookmarkClient client;
  scoped_ptr<BookmarkModel> model(client.CreateModel(false));
  const BookmarkNode* node = model->AddURL(model->other_node(),
                                           0,
                                           ASCIIToUTF16("foo bar"),
                                           GURL("http://www.google.com"));
  model->SetNodeMetaInfo(node, "somekey", "somevalue");
  model->SetNodeMetaInfo(node, "someotherkey", "someothervalue");

  // Copy a node to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  CopyToClipboard(model.get(), nodes, false);

  // Paste node to a different folder.
  const BookmarkNode* folder =
      model->AddFolder(model->bookmark_bar_node(), 0, ASCIIToUTF16("Folder"));
  EXPECT_EQ(0, folder->child_count());

  // And make sure we can paste a bookmark from the clipboard.
  EXPECT_TRUE(CanPasteFromClipboard(model.get(), folder));

  PasteFromClipboard(model.get(), folder, 0);
  ASSERT_EQ(1, folder->child_count());

  // Verify that the pasted node contains the same meta info.
  const BookmarkNode* pasted = folder->GetChild(0);
  ASSERT_TRUE(pasted->GetMetaInfoMap());
  EXPECT_EQ(2u, pasted->GetMetaInfoMap()->size());
  std::string value;
  EXPECT_TRUE(pasted->GetMetaInfo("somekey", &value));
  EXPECT_EQ("somevalue", value);
  EXPECT_TRUE(pasted->GetMetaInfo("someotherkey", &value));
  EXPECT_EQ("someothervalue", value);
}

#if defined(OS_LINUX) || defined(OS_MACOSX)
// http://crbug.com/396472
#define MAYBE_CutToClipboard DISABLED_CutToClipboard
#else
#define MAYBE_CutToClipboard CutToClipboard
#endif
TEST_F(BookmarkUtilsTest, MAYBE_CutToClipboard) {
  test::TestBookmarkClient client;
  scoped_ptr<BookmarkModel> model(client.CreateModel(false));
  model->AddObserver(this);

  base::string16 title(ASCIIToUTF16("foo"));
  GURL url("http://foo.com");
  const BookmarkNode* n1 = model->AddURL(model->other_node(), 0, title, url);
  const BookmarkNode* n2 = model->AddURL(model->other_node(), 1, title, url);

  // Cut the nodes to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(n1);
  nodes.push_back(n2);
  CopyToClipboard(model.get(), nodes, true);

  // Make sure the nodes were removed.
  EXPECT_EQ(0, model->other_node()->child_count());

  // Make sure observers were notified the set of changes should be grouped.
  ExpectGroupedChangeCount(1, 1);

  // And make sure we can paste from the clipboard.
  EXPECT_TRUE(CanPasteFromClipboard(model.get(), model->other_node()));
}

TEST_F(BookmarkUtilsTest, PasteNonEditableNodes) {
  test::TestBookmarkClient client;
  // Load a model with an extra node that is not editable.
  BookmarkPermanentNode* extra_node = new BookmarkPermanentNode(100);
  BookmarkPermanentNodeList extra_nodes;
  extra_nodes.push_back(extra_node);
  client.SetExtraNodesToLoad(extra_nodes.Pass());

  scoped_ptr<BookmarkModel> model(client.CreateModel(false));
  const BookmarkNode* node = model->AddURL(model->other_node(),
                                           0,
                                           ASCIIToUTF16("foo bar"),
                                           GURL("http://www.google.com"));

  // Copy a node to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  CopyToClipboard(model.get(), nodes, false);

  // And make sure we can paste a bookmark from the clipboard.
  EXPECT_TRUE(CanPasteFromClipboard(model.get(), model->bookmark_bar_node()));

  // But it can't be pasted into a non-editable folder.
  BookmarkClient* upcast = &client;
  EXPECT_FALSE(upcast->CanBeEditedByUser(extra_node));
  EXPECT_FALSE(CanPasteFromClipboard(model.get(), extra_node));
}
#endif  // !defined(OS_IOS)

TEST_F(BookmarkUtilsTest, GetParentForNewNodes) {
  test::TestBookmarkClient client;
  scoped_ptr<BookmarkModel> model(client.CreateModel(false));
  // This tests the case where selection contains one item and that item is a
  // folder.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model->bookmark_bar_node());
  int index = -1;
  const BookmarkNode* real_parent =
      GetParentForNewNodes(model->bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model->bookmark_bar_node());
  EXPECT_EQ(0, index);

  nodes.clear();

  // This tests the case where selection contains one item and that item is an
  // url.
  const BookmarkNode* page1 = model->AddURL(model->bookmark_bar_node(),
                                            0,
                                            ASCIIToUTF16("Google"),
                                            GURL("http://google.com"));
  nodes.push_back(page1);
  real_parent = GetParentForNewNodes(model->bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model->bookmark_bar_node());
  EXPECT_EQ(1, index);

  // This tests the case where selection has more than one item.
  const BookmarkNode* folder1 =
      model->AddFolder(model->bookmark_bar_node(), 1, ASCIIToUTF16("Folder 1"));
  nodes.push_back(folder1);
  real_parent = GetParentForNewNodes(model->bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model->bookmark_bar_node());
  EXPECT_EQ(2, index);

  // This tests the case where selection doesn't contain any items.
  nodes.clear();
  real_parent = GetParentForNewNodes(model->bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model->bookmark_bar_node());
  EXPECT_EQ(2, index);
}

// Verifies that meta info is copied when nodes are cloned.
TEST_F(BookmarkUtilsTest, CloneMetaInfo) {
  test::TestBookmarkClient client;
  scoped_ptr<BookmarkModel> model(client.CreateModel(false));
  // Add a node containing meta info.
  const BookmarkNode* node = model->AddURL(model->other_node(),
                                           0,
                                           ASCIIToUTF16("foo bar"),
                                           GURL("http://www.google.com"));
  model->SetNodeMetaInfo(node, "somekey", "somevalue");
  model->SetNodeMetaInfo(node, "someotherkey", "someothervalue");

  // Clone node to a different folder.
  const BookmarkNode* folder =
      model->AddFolder(model->bookmark_bar_node(), 0, ASCIIToUTF16("Folder"));
  std::vector<BookmarkNodeData::Element> elements;
  BookmarkNodeData::Element node_data(node);
  elements.push_back(node_data);
  EXPECT_EQ(0, folder->child_count());
  CloneBookmarkNode(model.get(), elements, folder, 0, false);
  ASSERT_EQ(1, folder->child_count());

  // Verify that the cloned node contains the same meta info.
  const BookmarkNode* clone = folder->GetChild(0);
  ASSERT_TRUE(clone->GetMetaInfoMap());
  EXPECT_EQ(2u, clone->GetMetaInfoMap()->size());
  std::string value;
  EXPECT_TRUE(clone->GetMetaInfo("somekey", &value));
  EXPECT_EQ("somevalue", value);
  EXPECT_TRUE(clone->GetMetaInfo("someotherkey", &value));
  EXPECT_EQ("someothervalue", value);
}

TEST_F(BookmarkUtilsTest, RemoveAllBookmarks) {
  test::TestBookmarkClient client;
  // Load a model with an extra node that is not editable.
  BookmarkPermanentNode* extra_node = new BookmarkPermanentNode(100);
  BookmarkPermanentNodeList extra_nodes;
  extra_nodes.push_back(extra_node);
  client.SetExtraNodesToLoad(extra_nodes.Pass());

  scoped_ptr<BookmarkModel> model(client.CreateModel(false));
  EXPECT_TRUE(model->bookmark_bar_node()->empty());
  EXPECT_TRUE(model->other_node()->empty());
  EXPECT_TRUE(model->mobile_node()->empty());
  EXPECT_TRUE(extra_node->empty());

  const base::string16 title = base::ASCIIToUTF16("Title");
  const GURL url("http://google.com");
  model->AddURL(model->bookmark_bar_node(), 0, title, url);
  model->AddURL(model->other_node(), 0, title, url);
  model->AddURL(model->mobile_node(), 0, title, url);
  model->AddURL(extra_node, 0, title, url);

  std::vector<const BookmarkNode*> nodes;
  model->GetNodesByURL(url, &nodes);
  ASSERT_EQ(4u, nodes.size());

  RemoveAllBookmarks(model.get(), url);

  nodes.clear();
  model->GetNodesByURL(url, &nodes);
  ASSERT_EQ(1u, nodes.size());
  EXPECT_TRUE(model->bookmark_bar_node()->empty());
  EXPECT_TRUE(model->other_node()->empty());
  EXPECT_TRUE(model->mobile_node()->empty());
  EXPECT_EQ(1, extra_node->child_count());
}

}  // namespace
}  // namespace bookmarks
