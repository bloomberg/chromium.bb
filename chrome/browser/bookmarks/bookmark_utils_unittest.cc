// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_utils.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
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
  MessageLoopForUI loop;
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

TEST_F(BookmarkUtilsTest, DoesBookmarkContainText) {
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

TEST_F(BookmarkUtilsTest, ApplyEditsWithNoFolderChange) {
  BookmarkModel model(NULL);
  const BookmarkNode* bookmarkbar = model.bookmark_bar_node();
  model.AddURL(bookmarkbar, 0, ASCIIToUTF16("url0"), GURL("chrome://newtab"));
  model.AddURL(bookmarkbar, 1, ASCIIToUTF16("url1"), GURL("chrome://newtab"));

  {
    BookmarkEditor::EditDetails detail(
        BookmarkEditor::EditDetails::AddFolder(bookmarkbar, 1));
    ApplyEditsWithNoFolderChange(&model, bookmarkbar, detail,
                                 ASCIIToUTF16("folder0"), GURL(""));
    EXPECT_EQ(ASCIIToUTF16("folder0"), bookmarkbar->GetChild(1)->GetTitle());
  }
  {
    BookmarkEditor::EditDetails detail(
        BookmarkEditor::EditDetails::AddFolder(bookmarkbar, -1));
    ApplyEditsWithNoFolderChange(&model, bookmarkbar, detail,
                                 ASCIIToUTF16("folder1"), GURL(""));
    EXPECT_EQ(ASCIIToUTF16("folder1"), bookmarkbar->GetChild(3)->GetTitle());
  }
  {
    BookmarkEditor::EditDetails detail(
        BookmarkEditor::EditDetails::AddFolder(bookmarkbar, 10));
    ApplyEditsWithNoFolderChange(&model, bookmarkbar, detail,
                                 ASCIIToUTF16("folder2"), GURL(""));
    EXPECT_EQ(ASCIIToUTF16("folder2"), bookmarkbar->GetChild(4)->GetTitle());
  }
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
