// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/profile_chooser_controller.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"

class ProfileChooserControllerTest : public CocoaProfileTest {
 public:
  ProfileChooserControllerTest() {
  }

  virtual void SetUp() OVERRIDE {
    CocoaProfileTest::SetUp();

    testing_profile_manager()->
        CreateTestingProfile("test1", scoped_ptr<PrefServiceSyncable>(),
                             ASCIIToUTF16("Test 1"), 0, std::string(),
                             TestingProfile::TestingFactories());
    testing_profile_manager()->
        CreateTestingProfile("test2", scoped_ptr<PrefServiceSyncable>(),
                             ASCIIToUTF16("Test 2"), 1, std::string(),
                             TestingProfile::TestingFactories());

    menu_ = new AvatarMenu(testing_profile_manager()->profile_info_cache(),
                           NULL, NULL);
    menu_->RebuildMenu();

    // There should be the default profile + two profiles we created.
    EXPECT_EQ(3U, menu_->GetNumberOfItems());
  }

  virtual void TearDown() OVERRIDE {
    [controller() close];
    CocoaProfileTest::TearDown();
  }

  void StartProfileChooserController() {
    NSRect frame = [test_window() frame];
    NSPoint point = NSMakePoint(NSMidX(frame), NSMidY(frame));
    controller_ = [[ProfileChooserController alloc] initWithBrowser:browser()
                                                         anchoredAt:point];
    [controller() showWindow:nil];
  }

  ProfileChooserController* controller() { return controller_; }
  AvatarMenu* menu() { return menu_; }

 private:
  // Weak; releases self.
  ProfileChooserController* controller_;

  // Weak; owned by |controller_|.
  AvatarMenu* menu_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserControllerTest);
};

TEST_F(ProfileChooserControllerTest, InitialLayout) {
  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];

  // Three profiles means we shoud have one active card, two "other" profiles,
  // one separator and one option buttons view.
  EXPECT_EQ(5U, [subviews count]);

  // There should be three buttons in the option buttons view.
  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  const SEL buttonSelectors[] = { @selector(showUserManager:),
                                  @selector(addNewProfile:),
                                  @selector(switchToGuestProfile:) };
  EXPECT_EQ(3U, [buttonSubviews count]);
  for (NSUInteger i = 0; i < [buttonSubviews count]; ++i) {
    NSButton* button = static_cast<NSButton*>([buttonSubviews objectAtIndex:i]);
    EXPECT_EQ(buttonSelectors[i], [button action]);
    EXPECT_EQ(controller(), [button target]);
  }

  // There should be a separator.
  EXPECT_TRUE([[subviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

  // There should be two "other profiles" items.
  for (NSUInteger i = 2; i < 4; ++i) {
    int profileIndex = i - 1;  // The separator is a view but not a profile.
    NSButton* button = static_cast<NSButton*>([subviews objectAtIndex:i]);
    EXPECT_EQ(menu()->GetItemAt(profileIndex).name,
              base::SysNSStringToUTF16([button title]));
    EXPECT_EQ(profileIndex, [button tag]);
    EXPECT_EQ(@selector(switchToProfile:), [button action]);
    EXPECT_EQ(controller(), [button target]);
  }

  // There should be at least profile avatar and a name in the active card view.
  // The links displayed in this subview are checked in separate tests.
  NSArray* activeCardSubviews = [[subviews objectAtIndex:4] subviews];
  EXPECT_TRUE([activeCardSubviews count] > 2);

  NSView* activeProfileImage = [activeCardSubviews objectAtIndex:0];
  EXPECT_TRUE([activeProfileImage isKindOfClass:[NSImageView class]]);

  // There are some links in between. The profile name is added last.
  CGFloat index = [activeCardSubviews count] - 1;
  NSView* activeProfileName = [activeCardSubviews objectAtIndex:index];
  EXPECT_TRUE([activeProfileName isKindOfClass:[NSTextField class]]);
  EXPECT_EQ(menu()->GetItemAt(0).name, base::SysNSStringToUTF16(
      [static_cast<NSTextField*>(activeProfileName) stringValue]));
}

TEST_F(ProfileChooserControllerTest, LocalProfileActiveCardLinks) {
  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  NSArray* activeCardSubviews = [[subviews objectAtIndex:4] subviews];

  // There are 2 items for the profile name/photo, and a "sign in" link.
  EXPECT_EQ(3U, [activeCardSubviews count]);
  NSButton* signinLink =
      static_cast<NSButton*>([activeCardSubviews objectAtIndex:1]);
  EXPECT_EQ(@selector(showSigninPage:), [signinLink action]);
  EXPECT_EQ(controller(), [signinLink target]);
}

TEST_F(ProfileChooserControllerTest, SignedInProfileActiveCardLinks) {
  // Sign in the first profile.
  ProfileInfoCache* cache = testing_profile_manager()->profile_info_cache();
  cache->SetUserNameOfProfileAtIndex(0, ASCIIToUTF16("user_name"));

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  NSArray* activeCardSubviews = [[subviews objectAtIndex:4] subviews];

  // There are 2 items for the profile name/photo, and two links.
  EXPECT_EQ(4U, [activeCardSubviews count]);
  NSButton* manageAccountsLink =
      static_cast<NSButton*>([activeCardSubviews objectAtIndex:1]);
  EXPECT_EQ(@selector(showAccountManagement:), [manageAccountsLink action]);
  EXPECT_EQ(controller(), [manageAccountsLink target]);

  NSButton* lockLink =
      static_cast<NSButton*>([activeCardSubviews objectAtIndex:2]);
  EXPECT_EQ(@selector(lockProfile:), [lockLink action]);
  EXPECT_EQ(controller(), [lockLink target]);
}

