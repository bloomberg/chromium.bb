// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

#if !defined(OS_MACOSX)
#include "chrome/browser/browser_process.h"
#endif

typedef testing::Test BookmarkUtilsTest;

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

  // Test with accents.
  node = model.AddURL(model.other_node(), 0,
      WideToUTF16(L"fr\u00E4n\u00E7\u00F3s\u00EA"),
      GURL("http://www.google.com/search?q=FBA"));
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("francose"), std::string()));
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("FrAnCoSe"), std::string()));
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, WideToUTF16(L"fr\u00E4ncose"), std::string()));
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, WideToUTF16(L"fran\u00E7ose"), std::string()));
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, WideToUTF16(L"franc\u00F3se"), std::string()));
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, WideToUTF16(L"francos\u00EA"), std::string()));
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, WideToUTF16(L"Fr\u00C4n\u00C7\u00F3S\u00EA"), std::string()));
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, WideToUTF16(L"fr\u00C4n\u00C7\u00D3s\u00CA"), std::string()));
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("fba"), std::string()));
  EXPECT_TRUE(bookmark_utils::DoesBookmarkContainText(
      node, ASCIIToUTF16("FBA"), std::string()));
}

#if !defined(OS_MACOSX)
#if defined(TOUCH_UI)
// TODO(wyck): Functionality disabled on TOUCH_UI to work around clipboard hang
// in http://crbug.com/96448
#define MAYBE_CopyPaste FAILS_CopyPaste
#else
#define MAYBE_CopyPaste CopyPaste
#endif
TEST_F(BookmarkUtilsTest, MAYBE_CopyPaste) {
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
  EXPECT_TRUE(bookmark_utils::CanPasteFromClipboard(model.bookmark_bar_node()));

  // Write some text to the clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(g_browser_process->clipboard());
    clipboard_writer.WriteText(ASCIIToUTF16("foo"));
  }

  // Now we shouldn't be able to paste from the clipboard.
  EXPECT_FALSE(
      bookmark_utils::CanPasteFromClipboard(model.bookmark_bar_node()));
}
#endif
