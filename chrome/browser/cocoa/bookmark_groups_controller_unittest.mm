// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_groups_controller.h"
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
  NSArray *groups = [controller_ groups];
  ASSERT_TRUE(groups);
  // TODO(snej): Update this next assertion after I add Search and Recents.
  ASSERT_EQ(1U, [groups count]);
  id barItem = [groups objectAtIndex:0];
  BookmarkModel* model = [manager_ bookmarkModel];
  const BookmarkNode* barNode = model->GetBookmarkBarNode();
  EXPECT_EQ(barNode, [manager_ nodeFromItem:barItem]);

  // Now add an item to Others:
  const BookmarkNode* otherNode = model->other_node();
  const BookmarkNode* wowbagger = model->AddGroup(otherNode, 0, L"Wowbagger");

  groups = [controller_ groups];
  ASSERT_TRUE(groups);
  ASSERT_EQ(2U, [groups count]);
  id wowbaggerItem = [groups objectAtIndex:1];
  EXPECT_EQ(wowbagger, [manager_ nodeFromItem:wowbaggerItem]);

  // Now remove it:
  model->Remove(otherNode, 0);

  groups = [controller_ groups];
  ASSERT_TRUE(groups);
  ASSERT_EQ(1U, [groups count]);
  ASSERT_EQ(barItem, [groups objectAtIndex:0]);
}

TEST_F(BookmarkGroupsControllerTest, Selection) {
  // Select nothing:
  ASSERT_TRUE([controller_ groupsTable]);
  [controller_ setSelectedGroup:nil];
  EXPECT_EQ(nil, [controller_ selectedGroup]);
  EXPECT_EQ(-1, [[controller_ groupsTable] selectedRow]);

  // Select bookmarks bar:
  id sel = [[controller_ groups] objectAtIndex:0];
  [controller_ setSelectedGroup:sel];
  EXPECT_EQ(sel, [controller_ selectedGroup]);
  EXPECT_EQ(0, [[controller_ groupsTable] selectedRow]);
}

}  // namespace
