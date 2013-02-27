// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/avatar_button_controller.h"

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser/avatar_menu_bubble_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/info_bubble_window.h"

class AvatarButtonControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    controller_.reset(
        [[AvatarButtonController alloc] initWithBrowser:browser()]);
    [[controller_ view] setHidden:YES];
  }

  virtual void TearDown() {
    browser()->window()->Close();
    CocoaProfileTest::TearDown();
  }

  NSButton* button() { return [controller_ buttonView]; }

  AvatarButtonController* controller() { return controller_.get(); }

 private:
  scoped_nsobject<AvatarButtonController> controller_;
};

TEST_F(AvatarButtonControllerTest, AddRemoveProfiles) {
  EXPECT_TRUE([button() isHidden]);

  testing_profile_manager()->CreateTestingProfile("one");

  EXPECT_FALSE([button() isHidden]);

  testing_profile_manager()->CreateTestingProfile("two");
  EXPECT_FALSE([button() isHidden]);

  testing_profile_manager()->DeleteTestingProfile("one");
  EXPECT_FALSE([button() isHidden]);

  testing_profile_manager()->DeleteTestingProfile("two");
  EXPECT_TRUE([button() isHidden]);
}

TEST_F(AvatarButtonControllerTest, DoubleOpen) {
  // Create a second profile to enable the avatar menu.
  testing_profile_manager()->CreateTestingProfile("p2");

  EXPECT_FALSE([controller() menuController]);

  [button() performClick:button()];

  AvatarMenuBubbleController* menu = [controller() menuController];
  EXPECT_TRUE(menu);

  [button() performClick:button()];
  EXPECT_EQ(menu, [controller() menuController]);

  // Do not animate out because that is hard to test around.
  static_cast<InfoBubbleWindow*>(menu.window).allowedAnimations =
      info_bubble::kAnimateNone;
  [menu close];
  EXPECT_FALSE([controller() menuController]);

  testing_profile_manager()->DeleteTestingProfile("p2");
}
