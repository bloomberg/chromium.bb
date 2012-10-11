// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_service_row_view_controller.h"
#import "chrome/browser/ui/intents/web_intent_picker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

NSString* const kTitle = @"A quick brown fox jumps over the lazy dog.";

}  // namespace

class WebIntentServiceRowViewControllerTest : public CocoaTest {
 public:
  WebIntentServiceRowViewControllerTest() {
    NSImage* icon = [NSImage imageNamed:NSImageNameFolder];
    view_controller_.reset([[WebIntentServiceRowViewController alloc]
        initSuggestedServiceRowWithTitle:kTitle
                                    icon:icon
                                  rating:2]);
    view_.reset([[view_controller_ view] retain]);
    [[test_window() contentView] addSubview:view_];
  }

 protected:
  scoped_nsobject<WebIntentServiceRowViewController> view_controller_;
  scoped_nsobject<NSView> view_;
};

TEST_VIEW(WebIntentServiceRowViewControllerTest, view_)

TEST_F(WebIntentServiceRowViewControllerTest, SuggestedService) {
  EXPECT_TRUE([view_controller_ titleLinkButton]);
  EXPECT_TRUE([view_controller_ selectButton]);

  const CGFloat margin = 10;
  NSRect inner_frame =
      NSMakeRect(margin, margin, 250,  WebIntentPicker::kServiceRowHeight);
  NSSize new_size =
      [view_controller_ minimumSizeForInnerWidth:NSWidth(inner_frame)];

  EXPECT_GT(new_size.width, NSWidth(inner_frame));
  EXPECT_EQ(new_size.height, NSHeight(inner_frame));
  inner_frame.size.height = new_size.height;
  [view_ setFrame:NSInsetRect(inner_frame, -margin, -margin)];
  [view_controller_ layoutSubviewsWithinFrame:inner_frame];

  // Verify that all controls are inside the inner frame.
  for (NSView* child in [view_ subviews]) {
    EXPECT_TRUE(NSIsEmptyRect([child frame]) ||
                NSContainsRect(inner_frame, [child frame]));
  }
}

TEST_F(WebIntentServiceRowViewControllerTest, InstalledService) {
  NSImage* icon = [NSImage imageNamed:NSImageNameFolder];
  scoped_nsobject<WebIntentServiceRowViewController> view_controller(
      [[WebIntentServiceRowViewController alloc]
          initInstalledServiceRowWithTitle:kTitle
                                      icon:icon]);
  NSView* view = [view_controller view];
  [[test_window() contentView] addSubview:view];

  EXPECT_FALSE([view_controller titleLinkButton]);
  EXPECT_TRUE([view_controller selectButton]);

  const CGFloat margin = 10;
  NSRect inner_frame =
      NSMakeRect(margin, margin, 250,  WebIntentPicker::kServiceRowHeight);
  NSSize new_size =
      [view_controller minimumSizeForInnerWidth:NSWidth(inner_frame)];

  EXPECT_GT(new_size.width, NSWidth(inner_frame));
  EXPECT_EQ(new_size.height, NSHeight(inner_frame));
  inner_frame.size.height = new_size.height;
  [view_ setFrame:NSInsetRect(inner_frame, -margin, -margin)];
  [view_controller layoutSubviewsWithinFrame:inner_frame];

  // Verify that all controls are inside the inner frame.
  for (NSView* child in [view subviews])
    EXPECT_TRUE(NSContainsRect(inner_frame, [child frame]));
}
