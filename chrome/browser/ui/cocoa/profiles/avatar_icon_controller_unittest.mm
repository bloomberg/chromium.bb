// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/avatar_icon_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_menu_bubble_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"

class AvatarIconControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() OVERRIDE {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    controller_.reset(
        [[AvatarIconController alloc] initWithBrowser:browser()]);
    [[controller_ view] setHidden:YES];
  }

  virtual void TearDown() OVERRIDE {
    browser()->window()->Close();
    CocoaProfileTest::TearDown();
  }

  NSButton* button() { return [controller_ buttonView]; }

  NSView* view() { return [controller_ view]; }

  AvatarIconController* controller() { return controller_.get(); }

 private:
  base::scoped_nsobject<AvatarIconController> controller_;
};

TEST_F(AvatarIconControllerTest, AddRemoveProfiles) {
  EXPECT_TRUE([view() isHidden]);

  testing_profile_manager()->CreateTestingProfile("one");

  EXPECT_FALSE([view() isHidden]);

  testing_profile_manager()->CreateTestingProfile("two");
  EXPECT_FALSE([view() isHidden]);

  testing_profile_manager()->DeleteTestingProfile("one");
  EXPECT_FALSE([view() isHidden]);

  testing_profile_manager()->DeleteTestingProfile("two");
  EXPECT_TRUE([view() isHidden]);
}

TEST_F(AvatarIconControllerTest, DoubleOpen) {
  // Create a second profile to enable the avatar menu.
  testing_profile_manager()->CreateTestingProfile("p2");

  EXPECT_FALSE([controller() menuController]);

  [button() performClick:button()];

  BaseBubbleController* menu = [controller() menuController];
  EXPECT_TRUE([menu isKindOfClass:[AvatarMenuBubbleController class]]);

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

TEST_F(AvatarIconControllerTest, SupervisedUserLabel) {
  DCHECK(!profile()->IsSupervised());
  EXPECT_FALSE([controller() labelButtonView]);

  // Create a second, supervised profile to enable the avatar menu.
  std::string name = "p2";
  TestingProfile* profile = testing_profile_manager()->CreateTestingProfile(
      name, scoped_ptr<PrefServiceSyncable>(), base::ASCIIToUTF16(name), 0,
      "asdf", TestingProfile::TestingFactories());
  EXPECT_TRUE(profile->IsSupervised());

  // http://crbug.com/39725
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile, &TemplateURLServiceFactory::BuildInstanceFor);
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      profile, &AutocompleteClassifierFactory::BuildInstanceFor);
  profile->CreateBookmarkModel(true);
  test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForProfile(profile));

  Browser* browser =
      new Browser(Browser::CreateParams(profile, chrome::GetActiveDesktop()));
  // Build a new controller to check if it is initialized correctly for a
  // supervised user profile.
  base::scoped_nsobject<AvatarIconController> controller(
      [[AvatarIconController alloc] initWithBrowser:browser]);

  EXPECT_TRUE([controller labelButtonView]);

  browser->window()->Close();
}
