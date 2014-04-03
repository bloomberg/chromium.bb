// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/profile_chooser_controller.h"

#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/common/chrome_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"

const std::string kEmail = "user@gmail.com";
const std::string kSecondaryEmail = "user2@gmail.com";
const std::string kLoginToken = "oauth2_login_token";

class ProfileChooserControllerTest : public CocoaProfileTest {
 public:
  ProfileChooserControllerTest() {
  }

  virtual void SetUp() OVERRIDE {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser()->profile());

    TestingProfile::TestingFactories factories;
    factories.push_back(
        std::make_pair(ProfileOAuth2TokenServiceFactory::GetInstance(),
                       BuildFakeProfileOAuth2TokenService));
    testing_profile_manager()->
        CreateTestingProfile("test1", scoped_ptr<PrefServiceSyncable>(),
                             base::ASCIIToUTF16("Test 1"), 0, std::string(),
                             factories);
    testing_profile_manager()->
        CreateTestingProfile("test2", scoped_ptr<PrefServiceSyncable>(),
                             base::ASCIIToUTF16("Test 2"), 1, std::string(),
                             TestingProfile::TestingFactories());

    menu_ = new AvatarMenu(testing_profile_manager()->profile_info_cache(),
                           NULL, NULL);
    menu_->RebuildMenu();

    // There should be the default profile + two profiles we created.
    EXPECT_EQ(3U, menu_->GetNumberOfItems());
  }

  virtual void TearDown() OVERRIDE {
    [controller() close];
    controller_.reset();
    CocoaProfileTest::TearDown();
  }

  void StartProfileChooserController() {
    NSRect frame = [test_window() frame];
    NSPoint point = NSMakePoint(NSMidX(frame), NSMidY(frame));
    controller_.reset([[ProfileChooserController alloc]
        initWithBrowser:browser()
             anchoredAt:point
               withMode:BUBBLE_VIEW_MODE_PROFILE_CHOOSER]);
    [controller_ showWindow:nil];
  }

  ProfileChooserController* controller() { return controller_; }
  AvatarMenu* menu() { return menu_; }

 private:
  base::scoped_nsobject<ProfileChooserController> controller_;

  // Weak; owned by |controller_|.
  AvatarMenu* menu_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserControllerTest);
};

TEST_F(ProfileChooserControllerTest, InitialLayout) {
  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];

  // Three profiles means we should have one active card, one separator and
  // one option buttons view.
  EXPECT_EQ(3U, [subviews count]);

  // For a local profile, there should be one button in the option buttons view.
  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  EXPECT_EQ(1U, [buttonSubviews count]);
  NSButton* button = static_cast<NSButton*>([buttonSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(showUserManager:), [button action]);
  EXPECT_EQ(controller(), [button target]);

  // There should be a separator.
  EXPECT_TRUE([[subviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

  // There should be the profile avatar, name and links container in the active
  // card view. The links displayed in the container are checked separately.
  NSArray* activeCardSubviews = [[subviews objectAtIndex:2] subviews];
  EXPECT_EQ(3U, [activeCardSubviews count]);

  NSView* activeProfileImage = [activeCardSubviews objectAtIndex:0];
  EXPECT_TRUE([activeProfileImage isKindOfClass:[NSImageView class]]);

  // There are some links in between. The profile name is added last.
  CGFloat index = [activeCardSubviews count] - 1;
  NSView* activeProfileName = [activeCardSubviews objectAtIndex:index];
  EXPECT_TRUE([activeProfileName isKindOfClass:[NSButton class]]);
  EXPECT_EQ(menu()->GetItemAt(0).name, base::SysNSStringToUTF16(
      [static_cast<NSButton*>(activeProfileName) title]));
}

TEST_F(ProfileChooserControllerTest, InitialLayoutWithFastUserSwitcher) {
  // The fast user switcher is only availbale behind a flag.
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kFastUserSwitching);

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];

  // Three profiles means we should have one active card, two "other" profiles,
  // one separator and one option buttons view.
  EXPECT_EQ(5U, [subviews count]);

  // For a local profile, there should be one button in the option buttons view.
  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  EXPECT_EQ(1U, [buttonSubviews count]);
  NSButton* button = static_cast<NSButton*>([buttonSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(showUserManager:), [button action]);
  EXPECT_EQ(controller(), [button target]);

  // There should be a separator.
  EXPECT_TRUE([[subviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

  // There should be two "other profiles" items. The items are drawn from the
  // bottom up, so in the opposite order of those in the AvatarMenu.
  int profileIndex = 1;
  for (NSUInteger i = 3; i >= 2; --i) {
    NSButton* button = static_cast<NSButton*>([subviews objectAtIndex:i]);
    EXPECT_EQ(menu()->GetItemAt(profileIndex).name,
              base::SysNSStringToUTF16([button title]));
    EXPECT_EQ(profileIndex, [button tag]);
    EXPECT_EQ(@selector(switchToProfile:), [button action]);
    EXPECT_EQ(controller(), [button target]);
    profileIndex++;
  }

  // There should be the profile avatar, name and links container in the active
  // card view. The links displayed in the container are checked separately.
  NSArray* activeCardSubviews = [[subviews objectAtIndex:4] subviews];
  EXPECT_EQ(3U, [activeCardSubviews count]);

  NSView* activeProfileImage = [activeCardSubviews objectAtIndex:0];
  EXPECT_TRUE([activeProfileImage isKindOfClass:[NSImageView class]]);

  // There are some links in between. The profile name is added last.
  CGFloat index = [activeCardSubviews count] - 1;
  NSView* activeProfileName = [activeCardSubviews objectAtIndex:index];
  EXPECT_TRUE([activeProfileName isKindOfClass:[NSButton class]]);
  EXPECT_EQ(menu()->GetItemAt(0).name, base::SysNSStringToUTF16(
      [static_cast<NSButton*>(activeProfileName) title]));
}

TEST_F(ProfileChooserControllerTest, OtherProfilesSortedAlphabetically) {
  // The fast user switcher is only availbale behind a flag.
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kFastUserSwitching);

  // Add two extra profiles, to make sure sorting is alphabetical and not
  // by order of creation.
  testing_profile_manager()->
      CreateTestingProfile("test3", scoped_ptr<PrefServiceSyncable>(),
                           base::ASCIIToUTF16("New Profile"), 1, std::string(),
                           TestingProfile::TestingFactories());
  testing_profile_manager()->
      CreateTestingProfile("test4", scoped_ptr<PrefServiceSyncable>(),
                           base::ASCIIToUTF16("Another Test"), 1, std::string(),
                           TestingProfile::TestingFactories());
  StartProfileChooserController();

  NSArray* subviews = [[[controller() window] contentView] subviews];
  NSString* sortedNames[] = { @"Another Test",
                              @"New Profile",
                              @"Test 1",
                              @"Test 2" };
  // There should be three "other profiles" items, sorted alphabetically.
  // The "other profiles" start at index 2, after the option buttons and
  // a separator. We need to iterate through the profiles in the order
  // displayed in the bubble, which is opposite from the drawn order.
  int sortedNameIndex = 0;
  for (NSUInteger i = 5; i >= 2; --i) {
    NSButton* button = static_cast<NSButton*>([subviews objectAtIndex:i]);
    EXPECT_TRUE(
        [[button title] isEqualToString:sortedNames[sortedNameIndex++]]);
  }
}

TEST_F(ProfileChooserControllerTest, LocalProfileActiveCardLinks) {
  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  NSArray* activeCardSubviews = [[subviews objectAtIndex:2] subviews];
  NSArray* activeCardLinks = [[activeCardSubviews objectAtIndex:1] subviews];

  // There should be one "sign in" link.
  EXPECT_EQ(1U, [activeCardLinks count]);
  NSButton* signinLink =
      static_cast<NSButton*>([activeCardLinks objectAtIndex:0]);
  EXPECT_EQ(@selector(showSigninPage:), [signinLink action]);
  EXPECT_EQ(controller(), [signinLink target]);
}

TEST_F(ProfileChooserControllerTest, SignedInProfileActiveCardLinks) {
  // Sign in the first profile.
  ProfileInfoCache* cache = testing_profile_manager()->profile_info_cache();
  cache->SetUserNameOfProfileAtIndex(0, base::ASCIIToUTF16(kEmail));

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  NSArray* activeCardSubviews = [[subviews objectAtIndex:2] subviews];
  NSArray* activeCardLinks = [[activeCardSubviews objectAtIndex:1] subviews];

  // There is one link: manage accounts.
  EXPECT_EQ(1U, [activeCardLinks count]);
  NSButton* manageAccountsLink =
      static_cast<NSButton*>([activeCardLinks objectAtIndex:0]);
  EXPECT_EQ(@selector(showAccountManagement:), [manageAccountsLink action]);
  EXPECT_EQ(controller(), [manageAccountsLink target]);
}

TEST_F(ProfileChooserControllerTest, AccountManagementLayout) {
  // Sign in the first profile.
  ProfileInfoCache* cache = testing_profile_manager()->profile_info_cache();
  cache->SetUserNameOfProfileAtIndex(0, base::ASCIIToUTF16(kEmail));

  // Set up the signin manager and the OAuth2Tokens.
  Profile* profile = browser()->profile();
  SigninManagerFactory::GetForProfile(profile)->
      SetAuthenticatedUsername(kEmail);
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->
      UpdateCredentials(kEmail, kLoginToken);
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->
      UpdateCredentials(kSecondaryEmail, kLoginToken);

  StartProfileChooserController();
  [controller() initMenuContentsWithView:BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];

  NSArray* subviews = [[[controller() window] contentView] subviews];

  // There should be one active card, one accounts container, two separators
  // and one option buttons view.
  EXPECT_EQ(5U, [subviews count]);

  // There should be two buttons in the option buttons view.
  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  const SEL buttonSelectors[] = { @selector(showUserManager:),
                                  @selector(lockProfile:) };
  EXPECT_EQ(2U, [buttonSubviews count]);
  for (NSUInteger i = 0; i < [buttonSubviews count]; ++i) {
    NSButton* button = static_cast<NSButton*>([buttonSubviews objectAtIndex:i]);
    EXPECT_EQ(buttonSelectors[i], [button action]);
    EXPECT_EQ(controller(), [button target]);
  }

  // There should be a separator.
  EXPECT_TRUE([[subviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

  // In the accounts view, there should be the account list container
  // accounts and one "add accounts" button.
  NSArray* accountsSubviews = [[subviews objectAtIndex:2] subviews];
  EXPECT_EQ(2U, [accountsSubviews count]);

  NSButton* addAccountsButton =
      static_cast<NSButton*>([accountsSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(addAccount:), [addAccountsButton action]);
  EXPECT_EQ(controller(), [addAccountsButton target]);

  // There should be two accounts in the account list container.
  NSArray* accountsListSubviews = [[accountsSubviews objectAtIndex:1] subviews];
  EXPECT_EQ(2U, [accountsListSubviews count]);

  NSButton* genericAccount =
      static_cast<NSButton*>([accountsListSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(showAccountRemovalView:), [genericAccount action]);
  EXPECT_EQ(controller(), [genericAccount target]);

  // Primary accounts are always last.
  NSButton* primaryAccount =
      static_cast<NSButton*>([accountsListSubviews objectAtIndex:1]);
  EXPECT_EQ(@selector(showAccountRemovalView:), [primaryAccount action]);
  EXPECT_EQ(controller(), [primaryAccount target]);

  // There should be another separator.
  EXPECT_TRUE([[subviews objectAtIndex:3] isKindOfClass:[NSBox class]]);

  // There should be the profile avatar, name and no links container in the
  // active card view.
  NSArray* activeCardSubviews = [[subviews objectAtIndex:4] subviews];
  EXPECT_EQ(2U, [activeCardSubviews count]);

  NSView* activeProfileImage = [activeCardSubviews objectAtIndex:0];
  EXPECT_TRUE([activeProfileImage isKindOfClass:[NSImageView class]]);

  NSView* activeProfileName = [activeCardSubviews objectAtIndex:1];
  EXPECT_TRUE([activeProfileName isKindOfClass:[NSButton class]]);
  EXPECT_EQ(menu()->GetItemAt(0).name, base::SysNSStringToUTF16(
      [static_cast<NSButton*>(activeProfileName) title]));
}
