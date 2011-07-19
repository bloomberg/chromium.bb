// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/test/testing_browser_process_test.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

#if !defined(OS_MACOSX)
#include "chrome/browser/browser_process.h"
#endif

typedef TestingBrowserProcessTest BookmarkUtilsTest;

TEST_F(BookmarkUtilsTest, GetBookmarksContainingText) {
  BookmarkModel model(NULL);
  const BookmarkNode* n1 =
      model.AddURL(model.other_node(), 0, ASCIIToUTF16("foo bar"),
                   GURL("http://www.google.com"));
  const BookmarkNode* n2 =
      model.AddURL(model.other_node(), 0, ASCIIToUTF16("baz buz"),
                   GURL("http://www.cnn.com"));

  model.AddFolder(model.other_node(), 0, ASCIIToUTF16("foo"));

  std::vector<const BookmarkNode*> nodes;
  bookmark_utils::GetBookmarksContainingText(
      &model, ASCIIToUTF16("foo"), 100, std::string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n1);
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      n1, ASCIIToUTF16("foo"), std::string()));
  nodes.clear();

  bookmark_utils::GetBookmarksContainingText(
      &model, ASCIIToUTF16("cnn"), 100, std::string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n2);
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      n2, ASCIIToUTF16("cnn"), std::string()));
  nodes.clear();

  bookmark_utils::GetBookmarksContainingText(
      &model, ASCIIToUTF16("foo bar"), 100, std::string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n1);
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      n1, ASCIIToUTF16("foo bar"), std::string()));
  nodes.clear();
}

TEST_F(BookmarkUtilsTest, DoesBookmarkContainText) {
  BookmarkModel model(NULL);
  const BookmarkNode* node = model.AddURL(model.other_node(), 0,
                                          ASCIIToUTF16("foo bar"),
                                          GURL("http://www.google.com"));
  // Matches to the title.
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("ar"), std::string()));
  // Matches to the URL
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("www"), std::string()));
  // No match.
  ASSERT_FALSE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("cnn"), std::string()));

  // Tests for a Japanese IDN.
  const string16 kDecodedIdn = WideToUTF16(L"\x30B0\x30FC\x30B0\x30EB");
  node = model.AddURL(model.other_node(), 0, ASCIIToUTF16("foo bar"),
                      GURL("http://xn--qcka1pmc.jp"));
  // Unicode query doesn't match if languages have no "ja".
  ASSERT_FALSE(bookmark_utils::DoesBookmarkContainText(
      node, kDecodedIdn, "en"));
  // Unicode query matches if languages have "ja".
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, kDecodedIdn, "ja"));
  // Punycode query also matches as ever.
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("qcka1pmc"), "ja"));

  // Tests with various lower/upper case characters.
  node = model.AddURL(model.other_node(), 0, ASCIIToUTF16("FOO bar"),
                      GURL("http://www.google.com/search?q=ABC"));
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("foo"), std::string()));
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("Foo"), std::string()));
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("FOO"), std::string()));
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("google abc"), std::string()));
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("google ABC"), std::string()));
  ASSERT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("http://www.google.com/search?q=A"), std::string()));
}

#if !defined(OS_MACOSX)
TEST_F(BookmarkUtilsTest, CopyPaste) {
  // Clipboard requires a message loop.
  MessageLoopForUI loop;

  BookmarkModel model(NULL);
  const BookmarkNode* node = model.AddURL(model.other_node(), 0,
                                          ASCIIToUTF16("foo bar"),
                                          GURL("http://www.google.com"));

  // Copy a node to the clipboard.
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  bookmark_utils::CopyToClipboard(&model, nodes, false);

  // And make sure we can paste a bookmark from the clipboard.
  EXPECT_TRUE(
      bookmark_utils::CanPasteFromClipboard(model.GetBookmarkBarNode()));

  // Write some text to the clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(g_browser_process->clipboard());
    clipboard_writer.WriteText(ASCIIToUTF16("foo"));
  }

  // Now we shouldn't be able to paste from the clipboard.
  EXPECT_FALSE(
      bookmark_utils::CanPasteFromClipboard(model.GetBookmarkBarNode()));
}
#endif
