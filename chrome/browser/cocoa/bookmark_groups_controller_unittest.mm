// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_groups_controller.h"
#import "chrome/browser/cocoa/bookmark_item.h"
#import "chrome/browser/cocoa/bookmark_manager_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class BookmarkGroupsControllerTest : public CocoaTest {
 public:
  void SetUp() {
    CocoaTest::SetUp();
    manager_ = [BookmarkManagerController showBookmarkManager:
                   browser_test_helper_.profile()];
    ASSERT_TRUE(manager_);
    controller_ = [manager_ groupsController];
    ASSERT_TRUE(controller_);
  }

  void TearDown() {
    [manager_ close];
    CocoaTest::TearDown();
  }

  BrowserTestHelper browser_test_helper_;
  BookmarkManagerController* manager_;
  BookmarkGroupsController* controller_;
};

TEST_F(BookmarkGroupsControllerTest, Model) {
  NSArray* groups = [controller_ groups];
  ASSERT_TRUE(groups);
  // TODO(snej): Update this next assertion after I add Search and Recents.
  ASSERT_EQ(1U, [groups count]);
  BookmarkItem* barItem = [groups objectAtIndex:0];
  EXPECT_EQ([manager_ bookmarkBarItem], barItem);

  // Now add an item to Others:
  BookmarkItem* other = [manager_ otherBookmarksItem];
  BookmarkItem* wowbagger = [other addFolderWithTitle:@"Wowbagger" atIndex:0];
  ASSERT_TRUE(wowbagger);

  groups = [controller_ groups];
  ASSERT_TRUE(groups);
  ASSERT_EQ(2U, [groups count]);
  EXPECT_EQ(wowbagger, [groups objectAtIndex:1]);

  // Now remove it:
  EXPECT_TRUE([other removeChild:wowbagger]);

  groups = [controller_ groups];
  ASSERT_TRUE(groups);
  ASSERT_EQ(1U, [groups count]);
  ASSERT_EQ(barItem, [groups objectAtIndex:0]);
}

TEST_F(BookmarkGroupsControllerTest, Selection) {
  // Check bookmarks bar is selected by default:
  BookmarkItem* bookmarksBar = [[controller_ groups] objectAtIndex:0];
  ASSERT_TRUE([controller_ groupsTable]);
  EXPECT_EQ(0, [[controller_ groupsTable] selectedRow]);
  EXPECT_EQ(bookmarksBar, [controller_ selectedGroup]);

  // Now add an item to Others:
  BookmarkItem* other = [manager_ otherBookmarksItem];
  BookmarkItem* wowbagger = [other addFolderWithTitle:@"Wowbagger" atIndex:0];
  ASSERT_TRUE(wowbagger);

  // Select it:
  [controller_ setSelectedGroup:wowbagger];
  EXPECT_EQ(wowbagger, [controller_ selectedGroup]);
  EXPECT_EQ(1, [[controller_ groupsTable] selectedRow]);

  // Select bookmarks bar:
  [controller_ setSelectedGroup:bookmarksBar];
  EXPECT_EQ(bookmarksBar, [controller_ selectedGroup]);
  EXPECT_EQ(0, [[controller_ groupsTable] selectedRow]);
}

}  // namespace
