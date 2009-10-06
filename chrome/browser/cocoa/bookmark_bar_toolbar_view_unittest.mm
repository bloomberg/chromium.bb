// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_toolbar_view.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Allows us to inject our fake controller below.
@interface BookmarkBarToolbarView (TestingAPI)
-(void)setController:(id<BookmarkBarFloating>)controller;
@end

@implementation BookmarkBarToolbarView (TestingAPI)
-(void)setController:(id<BookmarkBarFloating>)controller {
  controller_ = controller;
}
@end

// Allows us to control which way the view is rendered.
@interface DrawFloatingFakeController :
    NSObject<BookmarkBarFloating> {
  BOOL drawAsFloating_;
}
@property(assign) BOOL drawAsFloatingBar;
@end

@implementation DrawFloatingFakeController
@synthesize drawAsFloatingBar = drawAsFloating_;
@end

class BookmarkBarToolbarViewTest : public PlatformTest {
 public:
  BookmarkBarToolbarViewTest() {
    controller_.reset([[DrawFloatingFakeController alloc] init]);
    NSRect frame = NSMakeRect(0, 0, 100, 30);
    view_.reset([[BookmarkBarToolbarView alloc] initWithFrame:frame]);
    [cocoa_helper_.contentView() addSubview:view_.get()];
    [view_.get() setController:controller_.get()];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<DrawFloatingFakeController> controller_;
  scoped_nsobject<BookmarkBarToolbarView> view_;
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(BookmarkBarToolbarViewTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

// Test drawing (part 1), mostly to ensure nothing leaks or crashes.
TEST_F(BookmarkBarToolbarViewTest, DisplayAsNormalBar) {
  [controller_.get() setDrawAsFloatingBar:NO];
  [view_ display];
}

// Test drawing (part 2), mostly to ensure nothing leaks or crashes.
TEST_F(BookmarkBarToolbarViewTest, DisplayAsDisconnectedBar) {
  [controller_.get() setDrawAsFloatingBar:YES];
  [view_ display];
}
