// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"
#include "testing/gtest_mac.h"
#include "ui/base/accelerators/accelerator_cocoa.h"

namespace {

class ConfirmQuitPanelControllerTest : public CocoaTest {
 public:
  NSString* TestString(NSString* str) {
    str = [str stringByReplacingOccurrencesOfString:@"{Cmd}"
                                         withString:@"\u2318"];
    str = [str stringByReplacingOccurrencesOfString:@"{Ctrl}"
                                         withString:@"\u2303"];
    str = [str stringByReplacingOccurrencesOfString:@"{Opt}"
                                         withString:@"\u2325"];
    str = [str stringByReplacingOccurrencesOfString:@"{Shift}"
                                         withString:@"\u21E7"];
    return str;
  }
};


TEST_F(ConfirmQuitPanelControllerTest, ShowAndDismiss) {
  ConfirmQuitPanelController* controller =
      [ConfirmQuitPanelController sharedController];
  // Test singleton.
  EXPECT_EQ(controller, [ConfirmQuitPanelController sharedController]);
  [controller showWindow:nil];
  [controller dismissPanel];  // Releases self.
  // The controller should still be the singleton instance until after the
  // animation runs and the window closes. That will happen after this test body
  // finishes executing.
  EXPECT_EQ(controller, [ConfirmQuitPanelController sharedController]);
}

TEST_F(ConfirmQuitPanelControllerTest, KeyCombinationForAccelerator) {
  Class controller = [ConfirmQuitPanelController class];

  ui::AcceleratorCocoa item = ui::AcceleratorCocoa(@"q", NSCommandKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}Q"),
              [controller keyCombinationForAccelerator:item]);

  item = ui::AcceleratorCocoa(@"c", NSCommandKeyMask | NSShiftKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}{Shift}C"),
              [controller keyCombinationForAccelerator:item]);

  item = ui::AcceleratorCocoa(@"h",
      NSCommandKeyMask | NSShiftKeyMask | NSAlternateKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}{Opt}{Shift}H"),
              [controller keyCombinationForAccelerator:item]);

  item = ui::AcceleratorCocoa(@"r",
      NSCommandKeyMask | NSShiftKeyMask | NSAlternateKeyMask |
      NSControlKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}{Ctrl}{Opt}{Shift}R"),
              [controller keyCombinationForAccelerator:item]);

  item = ui::AcceleratorCocoa(@"o", NSControlKeyMask);
  EXPECT_NSEQ(TestString(@"{Ctrl}O"),
              [controller keyCombinationForAccelerator:item]);

  item = ui::AcceleratorCocoa(@"m", NSShiftKeyMask | NSControlKeyMask);
  EXPECT_NSEQ(TestString(@"{Ctrl}{Shift}M"),
              [controller keyCombinationForAccelerator:item]);

  item = ui::AcceleratorCocoa(@"e", NSCommandKeyMask | NSAlternateKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}{Opt}E"),
              [controller keyCombinationForAccelerator:item]);
}

}  // namespace
