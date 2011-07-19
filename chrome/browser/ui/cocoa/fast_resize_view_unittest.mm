// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

namespace {

class FastResizeViewTest : public CocoaTest {
 public:
  FastResizeViewTest() {
    NSRect frame = NSMakeRect(0, 0, 100, 30);
    scoped_nsobject<FastResizeView> view(
        [[FastResizeView alloc] initWithFrame:frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];

    scoped_nsobject<NSView> childView([[NSView alloc] initWithFrame:frame]);
    childView_ = childView.get();
    [view_ addSubview:childView_];
  }

  FastResizeView* view_;
  NSView* childView_;
};

TEST_VIEW(FastResizeViewTest, view_);

TEST_F(FastResizeViewTest, TestResizingOfChildren) {
  NSRect squareFrame = NSMakeRect(0, 0, 200, 200);
  NSRect rectFrame = NSMakeRect(1, 1, 150, 300);

  // Test that changing the view's frame also changes the child's frame.
  [view_ setFrame:squareFrame];
  EXPECT_TRUE(NSEqualRects([view_ bounds], [childView_ frame]));

  // Turn fast resize mode on and change the view's frame.  This time, the child
  // should not resize, but it should be anchored to the top left.
  [view_ setFastResizeMode:YES];
  [view_ setFrame:NSMakeRect(15, 30, 250, 250)];
  EXPECT_TRUE(NSEqualSizes([childView_ frame].size, squareFrame.size));
  EXPECT_EQ(NSMinX([view_ bounds]), NSMinX([childView_ frame]));
  EXPECT_EQ(NSMaxY([view_ bounds]), NSMaxY([childView_ frame]));

  // Another resize with fast resize mode on.
  [view_ setFrame:rectFrame];
  EXPECT_TRUE(NSEqualSizes([childView_ frame].size, squareFrame.size));
  EXPECT_EQ(NSMinX([view_ bounds]), NSMinX([childView_ frame]));
  EXPECT_EQ(NSMaxY([view_ bounds]), NSMaxY([childView_ frame]));

  // Turn fast resize mode off.  This should initiate an immediate resize, even
  // though we haven't called setFrame directly.
  [view_ setFastResizeMode:NO];
  EXPECT_TRUE(NSEqualRects([view_ frame], rectFrame));
  EXPECT_TRUE(NSEqualRects([view_ bounds], [childView_ frame]));
}

}  // namespace
