// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/memory/scoped_nsobject.h"
#include "base/message_loop.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_progress_view_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class WebIntentProgressViewControllerTest : public CocoaTest {
 public:
  WebIntentProgressViewControllerTest() {
    view_controller_.reset([[WebIntentProgressViewController alloc] init]);
    view_.reset([[view_controller_ view] retain]);
    [[test_window() contentView] addSubview:view_];
  }

 protected:
  scoped_nsobject<WebIntentProgressViewController> view_controller_;
  scoped_nsobject<NSView> view_;
  // Need for the progress indicator timer.
  MessageLoop message_loop_;
};

TEST_VIEW(WebIntentProgressViewControllerTest, view_)

TEST_F(WebIntentProgressViewControllerTest, Layout) {
  const CGFloat margin = 10;
  NSRect inner_frame = NSMakeRect(margin, margin, 100, 50);

  // Layout with empty title and message.
  NSSize empty_size =
      [view_controller_ minimumSizeForInnerWidth:NSWidth(inner_frame)];
  EXPECT_LE(empty_size.width, NSWidth(inner_frame));
  [view_ setFrame:NSInsetRect(inner_frame, -margin, -margin)];
  [view_controller_ layoutSubviewsWithinFrame:inner_frame];

  // Layout with a long string that wraps.
  NSString* string = @"A quick brown fox jumps over the lazy dog.";
  [view_controller_ setTitle:string];
  [view_controller_ setMessage:string];
  NSSize new_size =
      [view_controller_ minimumSizeForInnerWidth:NSWidth(inner_frame)];
  EXPECT_GE(new_size.width, empty_size.width);
  EXPECT_GT(new_size.height, empty_size.height);
  EXPECT_EQ(NSWidth(inner_frame), new_size.width);
  inner_frame.size.height = new_size.height;
  [view_ setFrame:NSInsetRect(inner_frame, -margin, -margin)];
  [view_controller_ layoutSubviewsWithinFrame:inner_frame];

  // Verify that all controls are inside the inner frame.
  for (NSView* child in [view_ subviews])
    EXPECT_TRUE(NSContainsRect(inner_frame, [child frame]));
}

TEST_F(WebIntentProgressViewControllerTest, Progress) {
  [view_controller_ setPercentDone:-1];
  [view_ display];
  [view_controller_ setPercentDone:50];
  [view_ display];
}
