// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_item.h"
#import "chrome/browser/cocoa/bookmark_manager_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class BookmarkManagerControllerTest : public CocoaTest {
 public:
  void SetUp() {
    CocoaTest::SetUp();
    controller_ = [BookmarkManagerController showBookmarkManager:
                   browser_test_helper_.profile()];
    ASSERT_TRUE(controller_);
  }

  void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

  BrowserTestHelper browser_test_helper_;
  BookmarkManagerController* controller_;
};

TEST_F(BookmarkManagerControllerTest, IsThisThingTurnedOn) {
  NSWindow* w = [controller_ window];
  ASSERT_TRUE(w);
  EXPECT_TRUE([w isVisible]);

  ASSERT_TRUE([controller_ groupsController]);
  ASSERT_TRUE([controller_ treeController]);
}

TEST_F(BookmarkManagerControllerTest, Model) {
  BookmarkModel* model = [controller_ bookmarkModel];
  ASSERT_EQ(browser_test_helper_.profile()->GetBookmarkModel(), model);

  // Check the bookmarks-bar item:
  const BookmarkNode* bar = model->GetBookmarkBarNode();
  ASSERT_TRUE(bar);
  BookmarkItem* barItem = [controller_ itemFromNode:bar];
  ASSERT_TRUE(barItem);
  ASSERT_EQ(barItem, [controller_ bookmarkBarItem]);

  // Check the 'others' item:
  const BookmarkNode* other = model->other_node();
  ASSERT_TRUE(other);
  EXPECT_NE(bar, other);
  scoped_nsobject<BookmarkItem> otherItem(
      [[controller_ itemFromNode:other] retain]);
  ASSERT_TRUE(otherItem);
  ASSERT_EQ(otherItem, [controller_ otherBookmarksItem]);

  EXPECT_NE(barItem, otherItem);
  EXPECT_FALSE([barItem isEqual:otherItem]);

  EXPECT_EQ(bar, [barItem node]);
  EXPECT_EQ(barItem, [controller_ itemFromNode:bar]);
  EXPECT_EQ(other, [otherItem node]);
  EXPECT_EQ(otherItem, [controller_ itemFromNode:other]);

  // Now tell it to forget 'other' and see if we get a different BookmarkItem:
  [controller_ forgetNode:other];
  BookmarkItem* otherItem2 = [controller_ itemFromNode:other];
  EXPECT_TRUE(otherItem2);
  EXPECT_NE(otherItem, otherItem2);
}

}  // namespace
