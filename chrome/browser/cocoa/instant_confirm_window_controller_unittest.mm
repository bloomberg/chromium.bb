// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/instant_confirm_window_controller.h"

#import "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/instant/instant_confirm_dialog.h"

namespace {

class InstantConfirmWindowControllerTest : public CocoaTest {
 public:
  InstantConfirmWindowControllerTest() : controller_(nil) {}

  BrowserTestHelper helper_;
  InstantConfirmWindowController* controller_;  // Weak. Owns self.
};

TEST_F(InstantConfirmWindowControllerTest, Init) {
  controller_ =
      [[InstantConfirmWindowController alloc] initWithProfile:
          helper_.profile()];
  EXPECT_TRUE([controller_ window]);
  [controller_ release];
}

TEST_F(InstantConfirmWindowControllerTest, Show) {
  browser::ShowInstantConfirmDialog(test_window(), helper_.profile());
  controller_ = [[test_window() attachedSheet] windowController];
  EXPECT_TRUE(controller_);
  [controller_ cancel:nil];
}

}  // namespace
