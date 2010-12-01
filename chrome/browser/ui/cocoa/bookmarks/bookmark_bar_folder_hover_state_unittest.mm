// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/message_loop.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_hover_state.h"
#import "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

namespace {

typedef CocoaTest BookmarkBarFolderHoverStateTest;

// Hover state machine interface.
// A strict call order is implied with these calls.  It is ONLY valid to make
// these specific state transitions.
TEST(BookmarkBarFolderHoverStateTest, HoverState) {
  BrowserTestHelper helper;
  scoped_nsobject<BookmarkBarFolderHoverState> bbfhs;
  bbfhs.reset([[BookmarkBarFolderHoverState alloc] init]);

  // Initial state.
  EXPECT_FALSE([bbfhs hoverButton]);
  ASSERT_EQ(kHoverStateClosed, [bbfhs hoverState]);

  scoped_nsobject<BookmarkButton> button;
  button.reset([[BookmarkButton alloc] initWithFrame:NSMakeRect(0, 0, 20, 20)]);

  // Test transition from closed to opening.
  ASSERT_EQ(kHoverStateClosed, [bbfhs hoverState]);
  [bbfhs scheduleOpenBookmarkFolderOnHoverButton:button];
  ASSERT_EQ(kHoverStateOpening, [bbfhs hoverState]);

  // Test transition from opening to closed (aka cancel open).
  [bbfhs cancelPendingOpenBookmarkFolderOnHoverButton];
  ASSERT_EQ(kHoverStateClosed, [bbfhs hoverState]);
  ASSERT_EQ(nil, [bbfhs hoverButton]);

  // Test transition from closed to opening.
  ASSERT_EQ(kHoverStateClosed, [bbfhs hoverState]);
  [bbfhs scheduleOpenBookmarkFolderOnHoverButton:button];
  ASSERT_EQ(kHoverStateOpening, [bbfhs hoverState]);

  // Test transition from opening to opened.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      new MessageLoop::QuitTask,
      bookmarks::kDragHoverOpenDelay * 1000.0 * 1.5);
  MessageLoop::current()->Run();
  ASSERT_EQ(kHoverStateOpen, [bbfhs hoverState]);
  ASSERT_EQ(button, [bbfhs hoverButton]);

  // Test transition from opening to opened.
  [bbfhs scheduleCloseBookmarkFolderOnHoverButton];
  ASSERT_EQ(kHoverStateClosing, [bbfhs hoverState]);

  // Test transition from closing to open (aka cancel close).
  [bbfhs cancelPendingCloseBookmarkFolderOnHoverButton];
  ASSERT_EQ(kHoverStateOpen, [bbfhs hoverState]);
  ASSERT_EQ(button, [bbfhs hoverButton]);

  // Test transition from closing to closed.
  [bbfhs scheduleCloseBookmarkFolderOnHoverButton];
  ASSERT_EQ(kHoverStateClosing, [bbfhs hoverState]);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      new MessageLoop::QuitTask,
      bookmarks::kDragHoverCloseDelay * 1000.0 * 1.5);
  MessageLoop::current()->Run();
  ASSERT_EQ(kHoverStateClosed, [bbfhs hoverState]);
  ASSERT_EQ(nil, [bbfhs hoverButton]);
}

}  // namespace
