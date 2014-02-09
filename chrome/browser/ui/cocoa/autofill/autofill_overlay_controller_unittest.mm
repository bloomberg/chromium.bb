// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_overlay_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_view_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace {

class AutofillOverlayControllerTest : public ui::CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    controller_.reset(
        [[AutofillOverlayController alloc] initWithDelegate:&delegate_]);
    [[test_window() contentView] addSubview:[controller_ view]];
  }

 protected:
  base::scoped_nsobject<AutofillOverlayController> controller_;
  testing::NiceMock<autofill::MockAutofillDialogViewDelegate> delegate_;
};

}  // namespace

TEST_VIEW(AutofillOverlayControllerTest, [controller_ view])

TEST_F(AutofillOverlayControllerTest, Subviews) {
  NSView* view = [controller_ view];
  ASSERT_EQ(2U, [[view subviews] count]);
  EXPECT_TRUE(
      [[[view subviews] objectAtIndex:0] isKindOfClass:[NSView class]]);
  EXPECT_TRUE(
      [[[view subviews] objectAtIndex:0] conformsToProtocol:
          @protocol(AutofillLayout)]);
  EXPECT_TRUE(
      [[[view subviews] objectAtIndex:1] isMemberOfClass:[NSImageView class]]);
}
