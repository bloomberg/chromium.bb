// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_choose_service_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_service_row_view_controller.h"
#import "chrome/browser/ui/intents/web_intent_picker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

NSString* const kTitle = @"A quick brown fox jumps over the lazy dog.";

}  // namespace

class WebIntentChooseServiceViewControllerTest : public CocoaTest {
 public:
  WebIntentChooseServiceViewControllerTest() {
    view_controller_.reset([[WebIntentChooseServiceViewController alloc] init]);
    view_.reset([[view_controller_ view] retain]);
    [[test_window() contentView] addSubview:view_];

    scoped_nsobject<NSMutableArray> rows([[NSMutableArray alloc] init]);
    [rows addObject:CreateInstalledRow()];
    [rows addObject:CreateInstalledRow()];
    [rows addObject:CreateSuggestedRow()];
    [rows addObject:CreateSuggestedRow()];
    [view_controller_ setRows:rows];
    [view_controller_ setTitle:kTitle];
    [view_controller_ setMessage:kTitle];
  }

 protected:
  // Creates a suggested service row with dummy data.
  WebIntentServiceRowViewController* CreateSuggestedRow() {
    NSImage* icon = [NSImage imageNamed:NSImageNameFolder];
    return [[[WebIntentServiceRowViewController alloc]
        initSuggestedServiceRowWithTitle:kTitle
                                    icon:icon
                                  rating:4] autorelease];
  }

  // Creates a installed service row with dummy data.
  WebIntentServiceRowViewController* CreateInstalledRow() {
    NSImage* icon = [NSImage imageNamed:NSImageNameFolder];
    return [[[WebIntentServiceRowViewController alloc]
        initInstalledServiceRowWithTitle:kTitle
                                    icon:icon] autorelease];
  }

  scoped_nsobject<WebIntentChooseServiceViewController> view_controller_;
  scoped_nsobject<NSView> view_;
};

TEST_VIEW(WebIntentChooseServiceViewControllerTest, view_)

TEST_F(WebIntentChooseServiceViewControllerTest, Layout) {
  const CGFloat margin = 10;
  NSRect inner_frame = NSMakeRect(margin, margin, 250,  100);

  NSSize new_size =
      [view_controller_ minimumSizeForInnerWidth:NSWidth(inner_frame)];
  EXPECT_GT(new_size.width, NSWidth(inner_frame));
  EXPECT_GT(new_size.height, NSHeight(inner_frame));
  inner_frame.size.height = new_size.height;
  [view_ setFrame:NSInsetRect(inner_frame, -margin, -margin)];
  [view_controller_ layoutSubviewsWithinFrame:inner_frame];

  // Verify that all controls are inside the inner frame.
  for (NSView* child in [view_ subviews]) {
    EXPECT_TRUE([child isMemberOfClass:[NSBox class]] ||
                NSIsEmptyRect([child frame]) ||
                NSContainsRect(inner_frame, [child frame]));
  }

  // Remove rows and verify that the frame shrinks.
  [view_controller_ setRows:nil];
  NSSize empty_size =
      [view_controller_ minimumSizeForInnerWidth:NSWidth(inner_frame)];
  EXPECT_LT(empty_size.width, new_size.width);
  EXPECT_LT(empty_size.height, new_size.height);
  inner_frame.size.height = empty_size.height;
  [view_ setFrame:NSInsetRect(inner_frame, -margin, -margin)];
  [view_controller_ layoutSubviewsWithinFrame:inner_frame];
  for (NSView* child in [view_ subviews]) {
    EXPECT_TRUE([child isMemberOfClass:[NSBox class]] ||
                NSIsEmptyRect([child frame]) ||
                NSContainsRect(inner_frame, [child frame]));
  }
}
