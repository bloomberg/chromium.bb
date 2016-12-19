// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/no_tabs/no_tabs_controller.h"
#import "ios/chrome/browser/ui/no_tabs/no_tabs_controller_testing.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class NoTabsControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    view_.reset([[UIView alloc] initWithFrame:CGRectMake(0, 0, 320, 240)]);
    controller_.reset([[NoTabsController alloc] initWithView:view_]);
  };

  base::scoped_nsobject<UIView> view_;
  base::scoped_nsobject<NoTabsController> controller_;
};

// Verifies that the mode toggle switch is properly shown and hidden.
TEST_F(NoTabsControllerTest, ModeToggleSwitch) {
  // The mode toggle button starts out hidden.
  UIButton* button = [controller_ modeToggleButton];
  ASSERT_TRUE([button isHidden]);

  [controller_ setHasModeToggleSwitch:YES];
  ASSERT_FALSE([button isHidden]);

  [controller_ setHasModeToggleSwitch:NO];
  ASSERT_TRUE([button isHidden]);
}

// Tests that creating a NoTabsController properly installs the No-Tabs UI into
// the provided view.
TEST_F(NoTabsControllerTest, Installation) {
  // The toolbar view should be installed in |view_|, and it should take up the
  // full width of |view_|.
  UIView* toolbarView = [controller_ toolbarView];
  EXPECT_EQ(view_, [toolbarView superview]);
  EXPECT_EQ(CGRectGetWidth([view_ bounds]),
            CGRectGetWidth([toolbarView frame]));

  // The background view should fill |view_| completely. Note that its
  // height/width might be larger than those of |view_|, as it has some padding
  // to avoid visual flickering around the edges on device rotation.
  UIView* backgroundView = [controller_ backgroundView];
  EXPECT_EQ(view_, [backgroundView superview]);
  EXPECT_LE([view_ bounds].size.width, [backgroundView frame].size.width);
  EXPECT_LE([view_ bounds].size.height, [backgroundView frame].size.height);
  EXPECT_TRUE(CGPointEqualToPoint([view_ center], [backgroundView center]));
}

// Tests that running through the show and hide methods does not crash.
// TODO(rohitrao): This is a somewhat silly test.  Test something more
// interesting.
TEST_F(NoTabsControllerTest, ShowAndHideNoTabsUI) {
  // Show the UI.
  [controller_ prepareForShowAnimation];
  [controller_ showNoTabsUI];
  [controller_ showAnimationDidFinish];

  // Hide the UI.
  [controller_ prepareForDismissAnimation];
  [controller_ dismissNoTabsUI];
  [controller_ dismissAnimationDidFinish];
}

}  // namespace
