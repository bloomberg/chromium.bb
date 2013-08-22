// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "url/gurl.h"

TEST(BookmarkUtilsGtkTest, GetNodesFromSelectionInvalid) {
  std::vector<const BookmarkNode*> nodes;
  nodes = GetNodesFromSelection(NULL, NULL, 0, NULL, NULL, NULL);
  EXPECT_EQ(0u, nodes.size());

  GtkSelectionData data;
  data.data = NULL;
  data.length = 0;
  nodes = GetNodesFromSelection(NULL, &data, 0, NULL, NULL, NULL);
  EXPECT_EQ(0u, nodes.size());

  nodes = GetNodesFromSelection(
      NULL, NULL, ui::CHROME_BOOKMARK_ITEM, NULL, NULL, NULL);
  EXPECT_EQ(0u, nodes.size());

  data.data = NULL;
  data.length = 0;
  nodes = GetNodesFromSelection(
      NULL, &data, ui::CHROME_BOOKMARK_ITEM, NULL, NULL, NULL);
  EXPECT_EQ(0u, nodes.size());

  guchar test_data[] = "";
  data.data = test_data;
  data.length = 0;
  nodes = GetNodesFromSelection(
      NULL, &data, ui::CHROME_BOOKMARK_ITEM, NULL, NULL, NULL);
  EXPECT_EQ(0u, nodes.size());
}

TEST(BookmarkUtilsGtkTest, WriteBookmarkToSelectionHTML) {
  BookmarkNode x(GURL("http://www.google.com"));
  x.SetTitle(string16(ASCIIToUTF16("Google")));
  GtkSelectionData data;
  data.data = NULL;
  data.length = 0;
  WriteBookmarkToSelection(&x, &data, ui::TEXT_HTML, NULL);
  std::string selection(reinterpret_cast<char*>(data.data), data.length);
  EXPECT_EQ("<a href=\"http://www.google.com/\">Google</a>", selection);

  // Free the copied data in GtkSelectionData
  gtk_selection_data_set(&data, data.type, data.format, NULL, -1);
}
