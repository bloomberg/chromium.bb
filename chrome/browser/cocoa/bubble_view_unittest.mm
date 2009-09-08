// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bubble_view.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class BubbleViewTest : public PlatformTest {
 public:
  BubbleViewTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 50);
    view_.reset([[BubbleView alloc] initWithFrame:frame
                                    themeProvider:cocoa_helper_.window()]);
    [cocoa_helper_.contentView() addSubview:view_.get()];
    [view_ setContent:@"Hi there, I'm a bubble view"];
  }

  CocoaTestHelper cocoa_helper_;
  scoped_nsobject<BubbleView> view_;
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(BubbleViewTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(BubbleViewTest, Display) {
  [view_ display];
}

// Test a nil themeProvider in init.
TEST_F(BubbleViewTest, NilThemeProvider) {
  NSRect frame = NSMakeRect(0, 0, 50, 50);
  view_.reset([[BubbleView alloc] initWithFrame:frame
                                  themeProvider:nil]);
  [cocoa_helper_.contentView() addSubview:view_.get()];
  [view_ display];
}

// Make sure things don't go haywire when given invalid or long strings.
TEST_F(BubbleViewTest, SetContent) {
  [view_ setContent:nil];
  EXPECT_TRUE([view_ content] == nil);
  [view_ setContent:@""];
  EXPECT_TRUE([[view_ content] isEqualToString:@""]);
  NSString* str = @"This is a really really long string that's just too long";
  [view_ setContent:str];
  EXPECT_TRUE([[view_ content] isEqualToString:str]);
}

TEST_F(BubbleViewTest, CornerFlags) {
  // Set some random flags just to check.
  [view_ setCornerFlags:kRoundedTopRightCorner | kRoundedTopLeftCorner];
  EXPECT_EQ([view_ cornerFlags],
            (unsigned long)kRoundedTopRightCorner | kRoundedTopLeftCorner);
  // Set no flags (all 4 draw corners are square).
  [view_ setCornerFlags:0];
  EXPECT_EQ([view_ cornerFlags], 0UL);
  // Set all bits. Meaningless past the first 4, but harmless to set too many.
  [view_ setCornerFlags:0xFFFFFFFF];
  EXPECT_EQ([view_ cornerFlags], 0xFFFFFFFF);
}
