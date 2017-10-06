// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller_protected.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_ios_unittest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using BookmarkHomeViewControllerTest = BookmarkIOSUnitTest;

TEST_F(BookmarkHomeViewControllerTest, LoadBookmarks) {
  @autoreleasepool {
    BookmarkHomeViewController* controller = [[BookmarkHomeViewController alloc]
        initWithLoader:nil
          browserState:chrome_browser_state_.get()
            dispatcher:nil];

    EXPECT_EQ(nil, controller.menuView);
    EXPECT_EQ(nil, controller.panelView);
    EXPECT_EQ(nil, controller.folderView);

    [controller view];
    [controller loadBookmarkViews];

    EXPECT_NE(nil, controller);
    EXPECT_NE(nil, controller.navigationBar);
    EXPECT_NE(nil, controller.menuView);
    EXPECT_NE(nil, controller.panelView);
    EXPECT_NE(nil, controller.folderView);
  }
}

TEST_F(BookmarkHomeViewControllerTest, LoadWaitingView) {
  @autoreleasepool {
    BookmarkHomeViewController* controller = [[BookmarkHomeViewController alloc]
        initWithLoader:nil
          browserState:chrome_browser_state_.get()
            dispatcher:nil];

    EXPECT_TRUE(controller.waitForModelView == nil);

    [controller view];
    [controller loadWaitingView];

    EXPECT_TRUE(controller != nil);
    EXPECT_TRUE(controller.waitForModelView != nil);
  }
}

}  // namespace
