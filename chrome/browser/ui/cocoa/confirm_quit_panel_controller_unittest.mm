// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"
#include "testing/gtest_mac.h"

namespace {

class ConfirmQuitPanelControllerTest : public CocoaTest {
 public:
  NSMenuItem* MakeMenuItem(NSString* keyEquivalent, NSUInteger modifiers) {
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:@"Test"
                                                  action:NULL
                                           keyEquivalent:keyEquivalent];
    [item setKeyEquivalentModifierMask:modifiers];
    return [item autorelease];
  }

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

TEST_F(ConfirmQuitPanelControllerTest, KeyCombinationForMenuItem) {
  ConfirmQuitPanelController* controller =
      [ConfirmQuitPanelController sharedController];

  NSMenuItem* item = MakeMenuItem(@"q", NSCommandKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}Q"),
              [controller keyCombinationForMenuItem:item]);

  item = MakeMenuItem(@"c", NSCommandKeyMask | NSShiftKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}{Shift}C"),
              [controller keyCombinationForMenuItem:item]);

  item = MakeMenuItem(@"h",
      NSCommandKeyMask | NSShiftKeyMask | NSAlternateKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}{Opt}{Shift}H"),
              [controller keyCombinationForMenuItem:item]);

  item = MakeMenuItem(@"r",
      NSCommandKeyMask | NSShiftKeyMask | NSAlternateKeyMask |
      NSControlKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}{Ctrl}{Opt}{Shift}R"),
              [controller keyCombinationForMenuItem:item]);

  item = MakeMenuItem(@"o", NSControlKeyMask);
  EXPECT_NSEQ(TestString(@"{Ctrl}O"),
              [controller keyCombinationForMenuItem:item]);

  item = MakeMenuItem(@"m", NSShiftKeyMask | NSControlKeyMask);
  EXPECT_NSEQ(TestString(@"{Ctrl}{Shift}M"),
              [controller keyCombinationForMenuItem:item]);

  item = MakeMenuItem(@"e", NSCommandKeyMask | NSAlternateKeyMask);
  EXPECT_NSEQ(TestString(@"{Cmd}{Opt}E"),
              [controller keyCombinationForMenuItem:item]);
}

}  // namespace
