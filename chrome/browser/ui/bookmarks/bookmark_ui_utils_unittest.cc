// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_utils.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(BookmarkUIUtilsTest, HasBookmarkURLs) {
  std::vector<const BookmarkNode*> nodes;

  BookmarkNode b1(GURL("http://google.com"));
  nodes.push_back(&b1);
  EXPECT_TRUE(chrome::HasBookmarkURLs(nodes));

  nodes.clear();

  BookmarkNode folder1(GURL::EmptyGURL());
  nodes.push_back(&folder1);
  EXPECT_FALSE(chrome::HasBookmarkURLs(nodes));

  BookmarkNode* b2 = new BookmarkNode(GURL("http://google.com"));
  folder1.Add(b2, 0);
  EXPECT_TRUE(chrome::HasBookmarkURLs(nodes));

  folder1.Remove(b2);

  BookmarkNode* folder11 = new BookmarkNode(GURL::EmptyGURL());
  folder11->Add(b2, 0);
  folder1.Add(folder11, 0);
  BookmarkNode folder2(GURL::EmptyGURL());
  nodes.push_back(&folder2);
  EXPECT_FALSE(chrome::HasBookmarkURLs(nodes));

  folder2.Add(b2, 0);
  EXPECT_TRUE(chrome::HasBookmarkURLs(nodes));

  folder2.Remove(b2);
  EXPECT_FALSE(chrome::HasBookmarkURLs(nodes));

  nodes.push_back(b2);
  EXPECT_TRUE(chrome::HasBookmarkURLs(nodes));
}

}  // namespace
