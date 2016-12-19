// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_parent_folder_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

TEST(BookmarkParentFolderItemTest, LabelGetsTitle) {
  BookmarkParentFolderItem* item =
      [[BookmarkParentFolderItem alloc] initWithType:0];
  BookmarkParentFolderCell* cell =
      [[BookmarkParentFolderCell alloc] initWithFrame:CGRectZero];

  item.title = @"Foo";
  [item configureCell:cell];
  EXPECT_EQ(@"Foo", cell.parentFolderNameLabel.text);
}

}  // namespace
