// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_notification_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "testing/gtest_mac.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

namespace {

class AutofillNotificationControllerTest : public ui::CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    controller_.reset([[AutofillNotificationController alloc] init]);
    [[test_window() contentView] addSubview:[controller_ view]];
  }

 protected:
  base::scoped_nsobject<AutofillNotificationController> controller_;
};

}  // namespace

TEST_VIEW(AutofillNotificationControllerTest, [controller_ view])

TEST_F(AutofillNotificationControllerTest, Subviews) {
  NSView* view = [controller_ view];
  ASSERT_EQ(2U, [[view subviews] count]);
  EXPECT_TRUE([[[view subviews] objectAtIndex:0] isKindOfClass:
      [NSTextField class]]);
  EXPECT_TRUE([[[view subviews] objectAtIndex:1] isKindOfClass:
      [NSButton class]]);
  EXPECT_NSEQ([controller_ textfield],
              [[view subviews] objectAtIndex:0]);
  EXPECT_NSEQ([controller_ checkbox],
              [[view subviews] objectAtIndex:1]);

  // Just to exercise the code path.
  [controller_ performLayout];
}
