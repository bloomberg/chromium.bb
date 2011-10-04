// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_button_cell.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

namespace {

class BookmarkBarFolderButtonCellTest : public CocoaTest {
};

// Basic creation.
TEST_F(BookmarkBarFolderButtonCellTest, Create) {
  scoped_nsobject<BookmarkBarFolderButtonCell> cell;
  cell.reset([[BookmarkBarFolderButtonCell buttonCellForNode:nil
                                                 contextMenu:nil
                                                    cellText:nil
                                                   cellImage:nil] retain]);
  EXPECT_TRUE(cell);
}

} // namespace
