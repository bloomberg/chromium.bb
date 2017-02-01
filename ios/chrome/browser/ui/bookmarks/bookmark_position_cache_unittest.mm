// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/bookmarks/bookmark_position_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

TEST(BookmarkPositionCacheTest, TestMenuItemFolderCoding) {
  BookmarkPositionCache* cache =
      [BookmarkPositionCache cacheForMenuItemFolderWithPosition:1010101
                                                       folderId:6363];

  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:cache];
  BookmarkPositionCache* cache2 =
      [NSKeyedUnarchiver unarchiveObjectWithData:data];
  EXPECT_NSEQ(cache, cache2);
}

}  // anonymous namespace
