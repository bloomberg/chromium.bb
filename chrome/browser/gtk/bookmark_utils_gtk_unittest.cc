// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gtk_dnd_util.h"
#include "chrome/browser/gtk/bookmark_utils_gtk.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BookmarkUtilsGtkTest, GetNodesFromSelectionInvalid) {
  std::vector<const BookmarkNode*> nodes;
  nodes = bookmark_utils::GetNodesFromSelection(NULL, NULL, 0, NULL, NULL,
                                                NULL);
  EXPECT_EQ(0u, nodes.size());

  GtkSelectionData data;
  data.data = NULL;
  data.length = 0;
  nodes = bookmark_utils::GetNodesFromSelection(NULL, &data, 0, NULL, NULL,
                                                NULL);
  EXPECT_EQ(0u, nodes.size());

  nodes = bookmark_utils::GetNodesFromSelection(NULL, NULL,
      gtk_dnd_util::CHROME_BOOKMARK_ITEM, NULL, NULL, NULL);
  EXPECT_EQ(0u, nodes.size());

  data.data = NULL;
  data.length = 0;
  nodes = bookmark_utils::GetNodesFromSelection(NULL, &data,
      gtk_dnd_util::CHROME_BOOKMARK_ITEM, NULL, NULL, NULL);
  EXPECT_EQ(0u, nodes.size());

  guchar test_data[] = "";
  data.data = test_data;
  data.length = 0;
  nodes = bookmark_utils::GetNodesFromSelection(NULL, &data,
      gtk_dnd_util::CHROME_BOOKMARK_ITEM, NULL, NULL, NULL);
  EXPECT_EQ(0u, nodes.size());
}
