// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_utils.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

#if !defined(OS_MACOSX)
#include "chrome/browser/browser_process.h"
#endif

using std::string;

namespace bookmark_utils {
namespace {

TEST(BookmarkUtilsTest, GetBookmarksContainingText) {
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
  EXPECT_TRUE(DoesBookmarkContainText(n1, ASCIIToUTF16("foo"), string()));
  nodes.clear();

  GetBookmarksContainingText(
      &model, ASCIIToUTF16("cnn"), 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n2);
  EXPECT_TRUE(DoesBookmarkContainText(n2, ASCIIToUTF16("cnn"), string()));
  nodes.clear();

  GetBookmarksContainingText(
      &model, ASCIIToUTF16("foo bar"), 100, string(), &nodes);
  ASSERT_EQ(1U, nodes.size());
  EXPECT_TRUE(nodes[0] == n1);
  EXPECT_TRUE(DoesBookmarkContainText(n1, ASCIIToUTF16("foo bar"), string()));
  nodes.clear();
}

TEST(BookmarkUtilsTest, DoesBookmarkContainText) {
  BookmarkModel model(NULL);
  const BookmarkNode* node = model.AddURL(model.other_node(),
                                          0,
                                          ASCIIToUTF16("foo bar"),
                                          GURL("http://www.google.com"));
  // Matches to the title.
  EXPECT_TRUE(DoesBookmarkContainText(node, ASCIIToUTF16("ar"), string()));
  // Matches to the URL.
  EXPECT_TRUE(DoesBookmarkContainText(node, ASCIIToUTF16("www"), string()));
  // No match.
  EXPECT_FALSE(DoesBookmarkContainText(node, ASCIIToUTF16("cnn"), string()));

  // Tests for a Japanese IDN.
  const string16 kDecodedIdn = WideToUTF16(L"\x30B0\x30FC\x30B0\x30EB");
  node = model.AddURL(model.other_node(),
                      0,
                      ASCIIToUTF16("foo bar"),
                      GURL("http://xn--qcka1pmc.jp"));
  // Unicode query doesn't match if languages have no "ja".
  EXPECT_FALSE(DoesBookmarkContainText(node, kDecodedIdn, "en"));
  // Unicode query matches if languages have "ja".
  EXPECT_TRUE(DoesBookmarkContainText(node, kDecodedIdn, "ja"));
  // Punycode query also matches as ever.
  EXPECT_TRUE(DoesBookmarkContainText(node, ASCIIToUTF16("qcka1pmc"), "ja"));

  // Tests with various lower/upper case characters.
  node = model.AddURL(model.other_node(),
                      0,
                      ASCIIToUTF16("FOO bar"),
                      GURL("http://www.google.com/search?q=ABC"));
  EXPECT_TRUE(DoesBookmarkContainText(node, ASCIIToUTF16("foo"), string()));
  EXPECT_TRUE(DoesBookmarkContainText(node, ASCIIToUTF16("Foo"), string()));
  EXPECT_TRUE(DoesBookmarkContainText(node, ASCIIToUTF16("FOO"), string()));
  EXPECT_TRUE(DoesBookmarkContainText(node, ASCIIToUTF16("google abc"),
                                      string()));
  EXPECT_TRUE(DoesBookmarkContainText(node, ASCIIToUTF16("google ABC"),
                                      string()));
  EXPECT_TRUE(DoesBookmarkContainText(
      node, ASCIIToUTF16("http://www.google.com/search?q=A"), string()));

  // Test with accents.
  node = model.AddURL(model.other_node(),
                      0,
                      WideToUTF16(L"fr\u00E4n\u00E7\u00F3s\u00EA"),
                      GURL("http://www.google.com/search?q=FBA"));
  EXPECT_TRUE(DoesBookmarkContainText(node, ASCIIToUTF16("francose"),
                                      string()));
  EXPECT_TRUE(DoesBookmarkContainText(node, ASCIIToUTF16("FrAnCoSe"),
                                      string()));
  EXPECT_TRUE(DoesBookmarkContainText(node, WideToUTF16(L"fr\u00E4ncose"),
                                      string()));
  EXPECT_TRUE(DoesBookmarkContainText(node, WideToUTF16(L"fran\u00E7ose"),
                                      string()));
  EXPECT_TRUE(DoesBookmarkContainText(node, WideToUTF16(L"franc\u00F3se"),
                                      string()));
  EXPECT_TRUE(DoesBookmarkContainText(node, WideToUTF16(L"francos\u00EA"),
                                      string()));
  EXPECT_TRUE(DoesBookmarkContainText(
      node, WideToUTF16(L"Fr\u00C4n\u00C7\u00F3S\u00EA"), string()));
  EXPECT_TRUE(DoesBookmarkContainText(
      node, WideToUTF16(L"fr\u00C4n\u00C7\u00D3s\u00CA"), string()));
  EXPECT_TRUE(DoesBookmarkContainText(node, ASCIIToUTF16("fba"), string()));
  EXPECT_TRUE(DoesBookmarkContainText(node, ASCIIToUTF16("FBA"), string()));
}

#if !defined(OS_MACOSX)
TEST(BookmarkUtilsTest, CopyPaste) {
  // Clipboard requires a message loop.
  MessageLoopForUI loop;

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
    ui::ScopedClipboardWriter clipboard_writer(g_browser_process->clipboard(),
                                               ui::Clipboard::BUFFER_STANDARD);
    clipboard_writer.WriteText(ASCIIToUTF16("foo"));
  }

  // Now we shouldn't be able to paste from the clipboard.
  EXPECT_FALSE(CanPasteFromClipboard(model.bookmark_bar_node()));
}
#endif

}  // namespace
}  // namespace bookmark_utils
