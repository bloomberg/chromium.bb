// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"

namespace {

class ConfirmQuitPanelControllerTest : public CocoaTest {
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

}  // namespace
