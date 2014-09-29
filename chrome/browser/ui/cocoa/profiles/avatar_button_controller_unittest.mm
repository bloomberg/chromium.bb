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

class AvatarButtonControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() OVERRIDE {
    switches::EnableNewAvatarMenuForTesting(CommandLine::ForCurrentProcess());
    DCHECK(profiles::IsMultipleProfilesEnabled());

    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    controller_.reset(
        [[AvatarButtonController alloc] initWithBrowser:browser()]);
  }

  virtual void TearDown() OVERRIDE {
    browser()->window()->Close();
    CocoaProfileTest::TearDown();
  }

  NSButton* button() { return [controller_ buttonView]; }

  NSView* view() { return [controller_ view]; }

  AvatarButtonController* controller() { return controller_.get(); }

 private:
  base::scoped_nsobject<AvatarButtonController> controller_;
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
