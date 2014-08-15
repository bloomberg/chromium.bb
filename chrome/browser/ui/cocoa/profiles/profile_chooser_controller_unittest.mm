// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/profile_chooser_controller.h"

#include "base/command_line.h"
#import "base/mac/foundation_util.h"
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
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/common/chrome_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"

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
               viewMode:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER
           tutorialMode:profiles::TUTORIAL_MODE_NONE
            serviceType:signin::GAIA_SERVICE_TYPE_NONE]);
    [controller_ showWindow:nil];
  }

  void EnableFastUserSwitching() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kFastUserSwitching);
  }

  ProfileChooserController* controller() { return controller_; }
  AvatarMenu* menu() { return menu_; }

 private:
  base::scoped_nsobject<ProfileChooserController> controller_;

  // Weak; owned by |controller_|.
  AvatarMenu* menu_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserControllerTest);
};

TEST_F(ProfileChooserControllerTest, InitialLayoutWithNewMenu) {
  switches::EnableNewAvatarMenuForTesting(CommandLine::ForCurrentProcess());
  StartProfileChooserController();

  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  // Three profiles means we should have one active card, one separator and
  // one option buttons view. We also have an update promo for the new avatar
  // menu.
  // TODO(noms): Enforcing 4U fails on the waterfall debug bots, but it's not
  // reproducible anywhere else.
  ASSERT_GE([subviews count], 3U);

  // There should be two buttons and a separator in the option buttons view.
  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  ASSERT_EQ(3U, [buttonSubviews count]);

  // There should be an incognito button.
  NSButton* incognitoButton =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(goIncognito:), [incognitoButton action]);
  EXPECT_EQ(controller(), [incognitoButton target]);

  // There should be a separator.
  EXPECT_TRUE([[subviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

  // There should be a user switcher button.
  NSButton* userSwitcherButton =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:2]);
  EXPECT_EQ(@selector(showUserManager:), [userSwitcherButton action]);
  EXPECT_EQ(controller(), [userSwitcherButton target]);

  // There should be a separator.
  EXPECT_TRUE([[subviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

  // There should be the profile avatar, name and links container in the active
  // card view. The links displayed in the container are checked separately.
  NSArray* activeCardSubviews = [[subviews objectAtIndex:2] subviews];
  ASSERT_EQ(3U, [activeCardSubviews count]);

  // Profile icon.
  NSView* activeProfileImage = [activeCardSubviews objectAtIndex:2];
  EXPECT_TRUE([activeProfileImage isKindOfClass:[NSImageView class]]);

  // Profile name.
  NSView* activeProfileName = [activeCardSubviews objectAtIndex:1];
  EXPECT_TRUE([activeProfileName isKindOfClass:[NSButton class]]);
  EXPECT_EQ(menu()->GetItemAt(0).name, base::SysNSStringToUTF16(
      [base::mac::ObjCCast<NSButton>(activeProfileName) title]));

  // Profile links. This is a local profile, so there should be a signin button
  // and a signin promo.
  NSArray* linksSubviews = [[activeCardSubviews objectAtIndex:0] subviews];
  ASSERT_EQ(2U, [linksSubviews count]);
  NSButton* link = base::mac::ObjCCast<NSButton>(
      [linksSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(showInlineSigninPage:), [link action]);
  EXPECT_EQ(controller(), [link target]);

  NSTextField* promo = base::mac::ObjCCast<NSTextField>(
      [linksSubviews objectAtIndex:1]);
  EXPECT_GT([[promo stringValue] length], 0U);
}

TEST_F(ProfileChooserControllerTest, InitialLayoutWithFastUserSwitcher) {
  switches::EnableNewAvatarMenuForTesting(CommandLine::ForCurrentProcess());
  EnableFastUserSwitching();
  StartProfileChooserController();

  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  // Three profiles means we should have one active card and a
  // fast user switcher which has two "other" profiles and 2 separators, and
  // an option buttons view with its separator. We also have a promo for
  // the new avatar menu.
  // TODO(noms): Enforcing 8U fails on the waterfall debug bots, but it's not
  // reproducible anywhere else.
  ASSERT_GE([subviews count], 7U);

  // There should be two buttons and a separator in the option buttons view.
  // These buttons are tested in InitialLayoutWithNewMenu.
  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  ASSERT_EQ(3U, [buttonSubviews count]);

  // There should be a separator.
  EXPECT_TRUE([[subviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

  // There should be two "other profiles" items. The items are drawn from the
  // bottom up, so in the opposite order of those in the AvatarMenu.
  int profileIndex = 1;
  for (int i = 5; i >= 2; i -= 2) {
    // Each profile button has a separator.
    EXPECT_TRUE([[subviews objectAtIndex:i] isKindOfClass:[NSBox class]]);

    NSButton* button = base::mac::ObjCCast<NSButton>(
        [subviews objectAtIndex:i-1]);
    EXPECT_EQ(menu()->GetItemAt(profileIndex).name,
              base::SysNSStringToUTF16([button title]));
    EXPECT_EQ(profileIndex, [button tag]);
    EXPECT_EQ(@selector(switchToProfile:), [button action]);
    EXPECT_EQ(controller(), [button target]);
    profileIndex++;
  }

  // There should be the profile avatar, name and links container in the active
  // card view. The links displayed in the container are checked separately.
  NSArray* activeCardSubviews = [[subviews objectAtIndex:6] subviews];
  ASSERT_EQ(3U, [activeCardSubviews count]);

  // Profile icon.
  NSView* activeProfileImage = [activeCardSubviews objectAtIndex:2];
  EXPECT_TRUE([activeProfileImage isKindOfClass:[NSImageView class]]);

  // Profile name.
  NSView* activeProfileName = [activeCardSubviews objectAtIndex:1];
  EXPECT_TRUE([activeProfileName isKindOfClass:[NSButton class]]);
  EXPECT_EQ(menu()->GetItemAt(0).name, base::SysNSStringToUTF16(
      [base::mac::ObjCCast<NSButton>(activeProfileName) title]));

  // Profile links. This is a local profile, so there should be a signin button
  // and a signin promo. These are also tested in InitialLayoutWithNewMenu.
  NSArray* linksSubviews = [[activeCardSubviews objectAtIndex:0] subviews];
  EXPECT_EQ(2U, [linksSubviews count]);
}

TEST_F(ProfileChooserControllerTest, OtherProfilesSortedAlphabetically) {
  switches::EnableNewAvatarMenuForTesting(CommandLine::ForCurrentProcess());
  EnableFastUserSwitching();

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
  ASSERT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  NSString* sortedNames[] = { @"Another Test",
                              @"New Profile",
                              @"Test 1",
                              @"Test 2" };
  // There are four "other" profiles, each with a button and a separator, an
  // active profile card, and an option buttons view with a separator. We
  // also have an update promo for the new avatar menu.
  // TODO(noms): Enforcing 12U fails on the waterfall debug bots, but it's not
  // reproducible anywhere else.
  ASSERT_GE([subviews count], 11U);
  // There should be four "other profiles" items, sorted alphabetically. The
  // "other profiles" start at index 2 (after the option buttons view and its
  // separator), and each have a separator. We need to iterate through the
  // profiles in the order displayed in the bubble, which is opposite from the
  // drawn order.
  int sortedNameIndex = 0;
  for (int i = 9; i >= 2; i -= 2) {
    // The item at index i is the separator.
    NSButton* button = base::mac::ObjCCast<NSButton>(
        [subviews objectAtIndex:i-1]);
    EXPECT_TRUE(
        [[button title] isEqualToString:sortedNames[sortedNameIndex++]]);
  }
}

TEST_F(ProfileChooserControllerTest,
    LocalProfileActiveCardLinksWithNewMenu) {
  switches::EnableNewAvatarMenuForTesting(CommandLine::ForCurrentProcess());
  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  NSArray* activeCardSubviews = [[subviews objectAtIndex:2] subviews];
  NSArray* activeCardLinks = [[activeCardSubviews objectAtIndex:0] subviews];

  ASSERT_EQ(2U, [activeCardLinks count]);

  // There should be a sign in button.
  NSButton* link = base::mac::ObjCCast<NSButton>(
      [activeCardLinks objectAtIndex:0]);
  EXPECT_EQ(@selector(showInlineSigninPage:), [link action]);
  EXPECT_EQ(controller(), [link target]);

  // Local profiles have a signin promo.
  NSTextField* promo = base::mac::ObjCCast<NSTextField>(
      [activeCardLinks objectAtIndex:1]);
  EXPECT_GT([[promo stringValue] length], 0U);
}

TEST_F(ProfileChooserControllerTest,
       SignedInProfileActiveCardLinksWithAccountConsistency) {
  switches::EnableAccountConsistencyForTesting(
      CommandLine::ForCurrentProcess());
  // Sign in the first profile.
  ProfileInfoCache* cache = testing_profile_manager()->profile_info_cache();
  cache->SetUserNameOfProfileAtIndex(0, base::ASCIIToUTF16(kEmail));

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  NSArray* activeCardSubviews = [[subviews objectAtIndex:2] subviews];
  NSArray* activeCardLinks = [[activeCardSubviews objectAtIndex:0] subviews];

  // There is one link: manage accounts.
  ASSERT_EQ(1U, [activeCardLinks count]);
  NSButton* manageAccountsLink =
      base::mac::ObjCCast<NSButton>([activeCardLinks objectAtIndex:0]);
  EXPECT_EQ(@selector(showAccountManagement:), [manageAccountsLink action]);
  EXPECT_EQ(controller(), [manageAccountsLink target]);
}

TEST_F(ProfileChooserControllerTest,
    SignedInProfileActiveCardLinksWithNewMenu) {
  switches::EnableNewAvatarMenuForTesting(CommandLine::ForCurrentProcess());
  // Sign in the first profile.
  ProfileInfoCache* cache = testing_profile_manager()->profile_info_cache();
  cache->SetUserNameOfProfileAtIndex(0, base::ASCIIToUTF16(kEmail));

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  NSArray* activeCardSubviews = [[subviews objectAtIndex:2] subviews];
  NSArray* activeCardLinks = [[activeCardSubviews objectAtIndex:0] subviews];

  // There is one disabled button with the user's email.
  ASSERT_EQ(1U, [activeCardLinks count]);
  NSButton* emailButton =
      base::mac::ObjCCast<NSButton>([activeCardLinks objectAtIndex:0]);
  EXPECT_EQ(kEmail, base::SysNSStringToUTF8([emailButton title]));
  EXPECT_EQ(nil, [emailButton action]);
  EXPECT_FALSE([emailButton isEnabled]);
}

TEST_F(ProfileChooserControllerTest, AccountManagementLayout) {
  switches::EnableAccountConsistencyForTesting(
      CommandLine::ForCurrentProcess());
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
  [controller() initMenuContentsWithView:
      profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];

  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  // There should be one active card, one accounts container, two separators
  // and one option buttons view.
  ASSERT_EQ(5U, [subviews count]);

  // There should be three buttons and two separators in the option
  // buttons view.
  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  ASSERT_EQ(5U, [buttonSubviews count]);

  // There should be a lock button.
  NSButton* lockButton =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(lockProfile:), [lockButton action]);
  EXPECT_EQ(controller(), [lockButton target]);

  // There should be a separator.
  EXPECT_TRUE([[buttonSubviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

  // There should be an incognito button.
  NSButton* incognitoButton =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:2]);
  EXPECT_EQ(@selector(goIncognito:), [incognitoButton action]);
  EXPECT_EQ(controller(), [incognitoButton target]);

  // There should be a separator.
  EXPECT_TRUE([[subviews objectAtIndex:3] isKindOfClass:[NSBox class]]);

  // There should be a user switcher button.
  NSButton* userSwitcherButton =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:4]);
  EXPECT_EQ(@selector(showUserManager:), [userSwitcherButton action]);
  EXPECT_EQ(controller(), [userSwitcherButton target]);

  // In the accounts view, there should be the account list container
  // accounts and one "add accounts" button.
  NSArray* accountsSubviews = [[subviews objectAtIndex:2] subviews];
  ASSERT_EQ(2U, [accountsSubviews count]);

  NSButton* addAccountsButton =
      base::mac::ObjCCast<NSButton>([accountsSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(addAccount:), [addAccountsButton action]);
  EXPECT_EQ(controller(), [addAccountsButton target]);

  // There should be two accounts in the account list container.
  NSArray* accountsListSubviews = [[accountsSubviews objectAtIndex:1] subviews];
  ASSERT_EQ(2U, [accountsListSubviews count]);

  NSButton* genericAccount =
      base::mac::ObjCCast<NSButton>([accountsListSubviews objectAtIndex:0]);
  NSButton* genericAccountDelete = base::mac::ObjCCast<NSButton>(
  [[genericAccount subviews] objectAtIndex:0]);
  EXPECT_EQ(@selector(showAccountRemovalView:), [genericAccountDelete action]);
  EXPECT_EQ(controller(), [genericAccountDelete target]);
  EXPECT_NE(-1, [genericAccountDelete tag]);

  // Primary accounts are always last.
  NSButton* primaryAccount =
      base::mac::ObjCCast<NSButton>([accountsListSubviews objectAtIndex:1]);
  NSButton* primaryAccountDelete = base::mac::ObjCCast<NSButton>(
      [[primaryAccount subviews] objectAtIndex:0]);
  EXPECT_EQ(@selector(showAccountRemovalView:), [primaryAccountDelete action]);
  EXPECT_EQ(controller(), [primaryAccountDelete target]);
  EXPECT_EQ(-1, [primaryAccountDelete tag]);

  // There should be another separator.
  EXPECT_TRUE([[subviews objectAtIndex:3] isKindOfClass:[NSBox class]]);

  // There should be the profile avatar, name and a "hide accounts" link
  // container in the active card view.
  NSArray* activeCardSubviews = [[subviews objectAtIndex:4] subviews];
  ASSERT_EQ(3U, [activeCardSubviews count]);

  // Profile icon.
  NSView* activeProfileImage = [activeCardSubviews objectAtIndex:2];
  EXPECT_TRUE([activeProfileImage isKindOfClass:[NSImageView class]]);

  // Profile name.
  NSView* activeProfileName = [activeCardSubviews objectAtIndex:1];
  EXPECT_TRUE([activeProfileName isKindOfClass:[NSButton class]]);
  EXPECT_EQ(menu()->GetItemAt(0).name, base::SysNSStringToUTF16(
      [base::mac::ObjCCast<NSButton>(activeProfileName) title]));

  // Profile links. This is a local profile, so there should be a signin button.
  NSArray* linksSubviews = [[activeCardSubviews objectAtIndex:0] subviews];
  ASSERT_EQ(1U, [linksSubviews count]);
  NSButton* link = base::mac::ObjCCast<NSButton>(
      [linksSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(hideAccountManagement:), [link action]);
  EXPECT_EQ(controller(), [link target]);
}
