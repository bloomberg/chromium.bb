// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/confirm_quit_panel_controller.h"

#import "chrome/browser/cocoa/cocoa_test_helper.h"

namespace {

class ConfirmQuitPanelControllerTest : public CocoaTest {
 public:
  ConfirmQuitPanelControllerTest() : controller_(nil) {
  }

  ConfirmQuitPanelController* controller_;  // Weak, owns self.
};


TEST_F(ConfirmQuitPanelControllerTest, ShowAndDismiss) {
  controller_ = [[ConfirmQuitPanelController alloc] init];
  [controller_ showWindow:nil];
  [controller_ dismissPanel];  // Releases self.
}

}  // namespace
