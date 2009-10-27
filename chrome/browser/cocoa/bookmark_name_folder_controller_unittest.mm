// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_name_folder_controller.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class BookmarkNameFolderControllerTest : public PlatformTest {
 public:
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  BrowserTestHelper helper_;
};


TEST_F(BookmarkNameFolderControllerTest, AddAndRename) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  const BookmarkNode* node = NULL;
  EXPECT_EQ(0, parent->GetChildCount());

  scoped_nsobject<BookmarkNameFolderController>
    controller([[BookmarkNameFolderController alloc]
                 initWithParentWindow:cocoa_helper_.window()
                              profile:helper_.profile()
                                 node:node]);
  [controller window];  // force nib load

  // Do nothing.
  [controller cancel:nil];
  EXPECT_EQ(0, parent->GetChildCount());

  // Change name then cancel.
  [controller setFolderName:@"Bozo"];
  [controller cancel:nil];
  EXPECT_EQ(0, parent->GetChildCount());

  // Add a new folder.
  [controller ok:nil];
  EXPECT_EQ(1, parent->GetChildCount());
  EXPECT_TRUE(parent->GetChild(0)->is_folder());
  EXPECT_EQ(L"Bozo", parent->GetChild(0)->GetTitle());

  // Rename the folder by creating a controller that originates from
  // the node.
  node = parent->GetChild(0);
  controller.reset([[BookmarkNameFolderController alloc]
                       initWithParentWindow:cocoa_helper_.window()
                                    profile:helper_.profile()
                                       node:node]);
  [controller window];  // force nib load

  [controller setFolderName:@"Zobo"];
  [controller ok:nil];
  EXPECT_EQ(1, parent->GetChildCount());
  EXPECT_TRUE(parent->GetChild(0)->is_folder());
  EXPECT_EQ(L"Zobo", parent->GetChild(0)->GetTitle());
}

TEST_F(BookmarkNameFolderControllerTest, EditAndConfirmOKButton) {
  BookmarkModel* model = helper_.profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->GetBookmarkBarNode();
  const BookmarkNode* node = NULL;
  EXPECT_EQ(0, parent->GetChildCount());

  scoped_nsobject<BookmarkNameFolderController>
    controller([[BookmarkNameFolderController alloc]
                 initWithParentWindow:cocoa_helper_.window()
                              profile:helper_.profile()
                                 node:node]);
  [controller window];  // force nib load

  // We start enabled since the default "New Folder" is added for us.
  EXPECT_TRUE([[controller okButton] isEnabled]);

  [controller setFolderName:@"Bozo"];
  EXPECT_TRUE([[controller okButton] isEnabled]);
  [controller setFolderName:@" "];
  EXPECT_TRUE([[controller okButton] isEnabled]);

  [controller setFolderName:@""];
  EXPECT_FALSE([[controller okButton] isEnabled]);
}
