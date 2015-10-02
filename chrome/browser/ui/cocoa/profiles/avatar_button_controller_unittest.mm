// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/avatar_button_controller.h"

#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/profiles/profile_chooser_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "grit/theme_resources.h"
#import "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// Defined in the AvatarButtonController implementation.
@interface AvatarButtonController (ExposedForTesting)
- (void)updateErrorStatus:(BOOL)hasError;
@end

// Subclassing AvatarButtonController to be able to control the state of
// keyboard modifierFlags.
@interface AvatarButtonControllerForTesting : AvatarButtonController {
 @private
  bool isCtrlPressed_;
}
@end

@interface AvatarButtonControllerForTesting (ExposedForTesting)
- (void)setIsCtrlPressed:(BOOL)isPressed;
- (BOOL)isCtrlPressed;
@end

@implementation AvatarButtonControllerForTesting
- (void)setIsCtrlPressed:(BOOL)isPressed {
  isCtrlPressed_ = isPressed;
}

- (BOOL)isCtrlPressed {
 // Always report that Cmd is not pressed since that's the case we're testing
 // and otherwise running the test while holding the Cmd key makes it fail.
 return isCtrlPressed_;
}
@end

class AvatarButtonControllerTest : public CocoaProfileTest {
 public:
  void SetUp() override {
    DCHECK(profiles::IsMultipleProfilesEnabled());

    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    controller_.reset(
        [[AvatarButtonControllerForTesting alloc] initWithBrowser:browser()]);
    [controller_ setIsCtrlPressed:false];
  }

  void TearDown() override {
    browser()->window()->Close();
    CocoaProfileTest::TearDown();
  }

  NSButton* button() { return [controller_ buttonView]; }

  NSView* view() { return [controller_ view]; }

  AvatarButtonControllerForTesting* controller() { return controller_.get(); }

 private:
  base::scoped_nsobject<AvatarButtonControllerForTesting> controller_;
};

TEST_F(AvatarButtonControllerTest, GenericButtonShown) {
  ASSERT_FALSE([view() isHidden]);
  // There is only one local profile, which means displaying the generic
  // avatar button.
  EXPECT_NSEQ(@"", [button() title]);
}

TEST_F(AvatarButtonControllerTest, ProfileButtonShown) {
  // Create a second profile, to force the button to display the profile name.
  testing_profile_manager()->CreateTestingProfile("batman");

  ASSERT_FALSE([view() isHidden]);
  EXPECT_NSEQ(@"Person 1", [button() title]);
}

TEST_F(AvatarButtonControllerTest, ProfileButtonWithErrorShown) {
  // Create a second profile, to force the button to display the profile name.
  testing_profile_manager()->CreateTestingProfile("batman");

  EXPECT_EQ(0, [button() image].size.width);
  [controller() updateErrorStatus:true];

  ASSERT_FALSE([view() isHidden]);
  EXPECT_NSEQ(@"Person 1", [button() title]);

  // If the button has an authentication error, it should display an error icon.
  int errorWidth = ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_ICON_PROFILES_AVATAR_BUTTON_ERROR).Width();
  EXPECT_EQ(errorWidth, [button() image].size.width);
}

TEST_F(AvatarButtonControllerTest, DoubleOpen) {
  EXPECT_FALSE([controller() menuController]);

  [button() performClick:button()];

  BaseBubbleController* menu = [controller() menuController];
  EXPECT_TRUE(menu);
  EXPECT_TRUE([menu isKindOfClass:[ProfileChooserController class]]);

  [button() performClick:button()];
  EXPECT_EQ(menu, [controller() menuController]);

  // Do not animate out because that is hard to test around.
  static_cast<InfoBubbleWindow*>(menu.window).allowedAnimations =
      info_bubble::kAnimateNone;
  [menu close];
  EXPECT_FALSE([controller() menuController]);
}

TEST_F(AvatarButtonControllerTest, DontOpenFastSwitcherWithoutTarget) {
  EXPECT_FALSE([controller() menuController]);

  [controller() setIsCtrlPressed:YES];
  [button() performClick:button()];

  // If there's only one profile and the fast user switcher is requested,
  // nothing should happen.
  EXPECT_FALSE([controller() menuController]);
}

TEST_F(AvatarButtonControllerTest, OpenFastUserSwitcherWithTarget) {
  testing_profile_manager()->CreateTestingProfile("batman");
  EXPECT_FALSE([controller() menuController]);

  [controller() setIsCtrlPressed:YES];
  [button() performClick:button()];

  BaseBubbleController* menu = [controller() menuController];
  EXPECT_TRUE(menu);
  EXPECT_TRUE([menu isKindOfClass:[ProfileChooserController class]]);

  // Do not animate out because that is hard to test around.
  static_cast<InfoBubbleWindow*>(menu.window).allowedAnimations =
      info_bubble::kAnimateNone;
  [menu close];
  EXPECT_FALSE([controller() menuController]);
}
