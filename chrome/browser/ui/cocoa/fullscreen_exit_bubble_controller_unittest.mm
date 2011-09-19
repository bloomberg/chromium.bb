// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen_exit_bubble_controller.h"

#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest_mac.h"
#include "ui/base/models/accelerator_cocoa.h"

@interface FullscreenExitBubbleController(JustForTesting)
// Already defined.
+ (NSString*)keyCommandString;
+ (NSString*)keyCombinationForAccelerator:(const ui::AcceleratorCocoa&)item;
@end

@interface FullscreenExitBubbleController(ExposedForTesting)
- (NSTextField*)exitLabelPlaceholder;
- (NSTextView*)exitLabel;
@end

@implementation FullscreenExitBubbleController(ExposedForTesting)
- (NSTextField*)exitLabelPlaceholder {
  return exitLabelPlaceholder_;
}

- (NSTextView*)exitLabel {
  return exitLabel_;
}
@end

class FullscreenExitBubbleControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();

    controller_.reset(
        [[FullscreenExitBubbleController alloc] initWithOwner:nil browser:nil]);
    EXPECT_TRUE([controller_ view]);

    [[test_window() contentView] addSubview:[controller_ view]];
  }

  scoped_nsobject<FullscreenExitBubbleController> controller_;
};

TEST_VIEW(FullscreenExitBubbleControllerTest, [controller_ view])

TEST_F(FullscreenExitBubbleControllerTest, LabelWasReplaced) {
  EXPECT_FALSE([controller_ exitLabelPlaceholder]);
  EXPECT_TRUE([controller_ exitLabel]);
}

TEST_F(FullscreenExitBubbleControllerTest, LabelContainsShortcut) {
  NSString* shortcut = [FullscreenExitBubbleController keyCommandString];
  EXPECT_GT([shortcut length], 0U);

  NSString* message = [[[controller_ exitLabel] textStorage] string];

  NSRange range = [message rangeOfString:shortcut];
  EXPECT_NE(NSNotFound, range.location);
}

TEST_F(FullscreenExitBubbleControllerTest, ShortcutText) {
  ui::AcceleratorCocoa cmd_F(@"F", NSCommandKeyMask);
  ui::AcceleratorCocoa cmd_shift_f(@"f", NSCommandKeyMask|NSShiftKeyMask);
  NSString* cmd_F_text = [FullscreenExitBubbleController
      keyCombinationForAccelerator:cmd_F];
  NSString* cmd_shift_f_text = [FullscreenExitBubbleController
      keyCombinationForAccelerator:cmd_shift_f];
  EXPECT_NSEQ(cmd_shift_f_text, cmd_F_text);
  EXPECT_NSEQ(@"\u2318\u21E7F", cmd_shift_f_text);
}
