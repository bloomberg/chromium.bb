// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_utils.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

using std::string;

namespace bookmark_utils {
namespace {

class BookmarkUtilsTest : public ::testing::Test {
 public:
  virtual void TearDown() OVERRIDE {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

 private:
  // Clipboard requires a message loop.
  base::MessageLoopForUI loop;
};

TEST_F(BookmarkUtilsTest, GetBookmarksContainingText) {
  BookmarkModel model(NULL);
  const BookmarkNode* n1 = model.AddURL(model.other_node(),
                                        0,
                                        ASCIIToUTF16("foo bar"),
                                        GURL("http://www.google.com"));
  const BookmarkNode* n2 = model.AddURL(model.other_node(),
                                        0,
                                        ASCIIToUTF16("baz buz"),
                                        GURL("http://www.cnn.com"));

  model.AddFolder(model.other_node(), 0, ASCIIToUTF16("foo"));

  std::vector<const BookmarkNode*> nodes;
  GetBookmarksContainingText(
      &model, ASCIIToUTF16("foo"), 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n1);
  nodes.clear();

  GetBookmarksContainingText(
      &model, ASCIIToUTF16("cnn"), 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n2);
  nodes.clear();

  GetBookmarksContainingText(
      &model, ASCIIToUTF16("foo bar"), 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n1);
  nodes.clear();
}

TEST_F(BookmarkUtilsTest, CopyPaste) {
  BookmarkModel model(NULL);
  const BookmarkNode* node = model.AddURL(model.other_node(),
                                          0,
                                          ASCIIToUTF16("foo bar"),
                                          GURL("http://www.google.com"));

  // Copy a node to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  CopyToClipboard(&model, nodes, false);

  // And make sure we can paste a bookmark from the clipboard.
  EXPECT_TRUE(CanPasteFromClipboard(model.bookmark_bar_node()));

  // Write some text to the clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(
        ui::Clipboard::GetForCurrentThread(),
        ui::Clipboard::BUFFER_STANDARD);
    clipboard_writer.WriteText(ASCIIToUTF16("foo"));
  }

  // Now we shouldn't be able to paste from the clipboard.
  EXPECT_FALSE(CanPasteFromClipboard(model.bookmark_bar_node()));
}

TEST_F(BookmarkUtilsTest, GetParentForNewNodes) {
  BookmarkModel model(NULL);
  // This tests the case where selection contains one item and that item is a
  // folder.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model.bookmark_bar_node());
  int index = -1;
  const BookmarkNode* real_parent = GetParentForNewNodes(
      model.bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model.bookmark_bar_node());
  EXPECT_EQ(0, index);

  nodes.clear();

  // This tests the case where selection contains one item and that item is an
  // url.
  const BookmarkNode* page1 = model.AddURL(model.bookmark_bar_node(), 0,
                                           ASCIIToUTF16("Google"),
                                           GURL("http://google.com"));
  nodes.push_back(page1);
  real_parent = GetParentForNewNodes(model.bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model.bookmark_bar_node());
  EXPECT_EQ(1, index);

  // This tests the case where selection has more than one item.
  const BookmarkNode* folder1 = model.AddFolder(model.bookmark_bar_node(), 1,
                                                ASCIIToUTF16("Folder 1"));
  nodes.push_back(folder1);
  real_parent = GetParentForNewNodes(model.bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model.bookmark_bar_node());
  EXPECT_EQ(2, index);

  // This tests the case where selection doesn't contain any items.
  nodes.clear();
  real_parent = GetParentForNewNodes(model.bookmark_bar_node(), nodes, &index);
  EXPECT_EQ(real_parent, model.bookmark_bar_node());
  EXPECT_EQ(2, index);
}

}  // namespace
}  // namespace bookmark_utils
