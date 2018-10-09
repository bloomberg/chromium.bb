// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"

#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class OmniboxPopupViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    popup_view_controller_ = [[OmniboxPopupViewController alloc] init];
  };

  OmniboxPopupViewController* popup_view_controller_;
};

TEST_F(OmniboxPopupViewControllerTest, hasCellsWhenShortcutsEnabled) {
  // This test makes an assumption that this view controller is a datasource for
  // a table view. Rewrite this test if this is not the case anymore.
  EXPECT_TRUE([popup_view_controller_
      conformsToProtocol:@protocol(UITableViewDataSource)]);
  id<UITableViewDataSource> datasource =
      (id<UITableViewDataSource>)popup_view_controller_;
  UITableView* tableView = [[UITableView alloc] init];

  // A stub view controller.
  UIViewController* shortcutsViewController = [[UIViewController alloc] init];

  // Shortcuts are not enabled by default.
  EXPECT_FALSE(popup_view_controller_.shortcutsEnabled);

  // Check that the shorcuts row doesn't appear.
  [popup_view_controller_ updateMatches:@[] withAnimation:NO];
  EXPECT_EQ([datasource tableView:tableView numberOfRowsInSection:0], 0);

  // Enable shortcuts and verify they appear. When enabling, the view controller
  // has to be non-nil.
  popup_view_controller_.shortcutsViewController = shortcutsViewController;
  popup_view_controller_.shortcutsEnabled = YES;
  EXPECT_EQ([datasource tableView:tableView numberOfRowsInSection:0], 1);

  // Disable and verify it disappears again.
  popup_view_controller_.shortcutsEnabled = NO;
  EXPECT_EQ([datasource tableView:tableView numberOfRowsInSection:0], 0);
}

}  // namespace
