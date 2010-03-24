// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_folder_controller.h"
#import "chrome/browser/cocoa/bookmark_folder_target.h"
#include "chrome/browser/cocoa/bookmark_button.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"


class BookmarkFolderTargetTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    BookmarkModel* model = helper_.profile()->GetBookmarkModel();
    bmbNode_ = model->GetBookmarkBarNode();
  }

  BrowserTestHelper helper_;
  const BookmarkNode* bmbNode_;
};

TEST_F(BookmarkFolderTargetTest, StartWithNothing) {
  // Need a fake "button" which has a bookmark node.
  id sender = [OCMockObject mockForClass:[BookmarkButton class]];
  [[[sender stub] andReturnValue:OCMOCK_VALUE(bmbNode_)] bookmarkNode];

  // Fake controller
  id controller = [OCMockObject mockForClass:[BookmarkBarFolderController
                                               class]];
  // No current folder
  [[[controller stub] andReturn:nil] folderController];

  // Make sure we get an addNew
  [[controller expect] addNewFolderControllerWithParentButton:sender];

  scoped_nsobject<BookmarkFolderTarget> target(
    [[BookmarkFolderTarget alloc] initWithController:controller]);

  [target openBookmarkFolderFromButton:sender];
  [controller verify];
}

TEST_F(BookmarkFolderTargetTest, ReopenSame) {
  // Need a fake "button" which has a bookmark node.
  id sender = [OCMockObject mockForClass:[BookmarkButton class]];
  [[[sender stub] andReturnValue:OCMOCK_VALUE(bmbNode_)] bookmarkNode];

  // Fake controller
  id controller = [OCMockObject mockForClass:[BookmarkBarFolderController
                                               class]];
  // YES a current folder.  Self-mock that as well, so "same" will be true.
  [[[controller stub] andReturn:controller] folderController];
  [[[controller stub] andReturn:sender] parentButton];

  // Make sure we close all and do NOT create a new one.
  [[controller expect] closeAllBookmarkFolders];

  scoped_nsobject<BookmarkFolderTarget> target(
    [[BookmarkFolderTarget alloc] initWithController:controller]);

  [target openBookmarkFolderFromButton:sender];
  [controller verify];
}

TEST_F(BookmarkFolderTargetTest, ReopenNotSame) {
  // Need a fake "button" which has a bookmark node.
  id sender = [OCMockObject mockForClass:[BookmarkButton class]];
  [[[sender stub] andReturnValue:OCMOCK_VALUE(bmbNode_)] bookmarkNode];

  // Fake controller
  id controller = [OCMockObject mockForClass:[BookmarkBarFolderController
                                               class]];
  // YES a current folder but NOT same.
  [[[controller stub] andReturn:controller] folderController];
  [[[controller stub] andReturn:nil] parentButton];

  // Make sure we close all AND create a new one.
  [[controller expect] closeAllBookmarkFolders];
  [[controller expect] addNewFolderControllerWithParentButton:sender];

  scoped_nsobject<BookmarkFolderTarget> target(
    [[BookmarkFolderTarget alloc] initWithController:controller]);

  [target openBookmarkFolderFromButton:sender];
  [controller verify];
}
