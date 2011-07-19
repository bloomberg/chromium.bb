// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_tree_browser_cell.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/platform_test.h"

class BookmarkTreeBrowserCellTest : public PlatformTest {
 public:
  BookmarkTreeBrowserCellTest() {
    // Set up our mocks.
    GURL gurl;
    bookmarkNodeMock_.reset(new BookmarkNode(gurl));
    matrixMock_.reset([[NSMatrix alloc] init]);
    targetMock_.reset([[NSObject alloc] init]);
  }

  scoped_ptr<BookmarkNode> bookmarkNodeMock_;
  scoped_nsobject<NSMatrix> matrixMock_;
  scoped_nsobject<NSObject> targetMock_;
};

TEST_F(BookmarkTreeBrowserCellTest, BasicAllocDealloc) {
  BookmarkTreeBrowserCell* cell = [[[BookmarkTreeBrowserCell alloc]
                                    initTextCell:@"TEST STRING"] autorelease];
  [cell setMatrix:matrixMock_.get()];
  [cell setTarget:targetMock_.get()];
  [cell setAction:@selector(mockAction:)];
  [cell setBookmarkNode:bookmarkNodeMock_.get()];

  NSMatrix* testMatrix = [cell matrix];
  EXPECT_EQ(testMatrix, matrixMock_.get());
  id testTarget = [cell target];
  EXPECT_EQ(testTarget, targetMock_.get());
  SEL testAction = [cell action];
  EXPECT_EQ(testAction, @selector(mockAction:));
  const BookmarkNode* testBookmarkNode = [cell bookmarkNode];
  EXPECT_EQ(testBookmarkNode, bookmarkNodeMock_.get());
}
