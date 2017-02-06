// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/profile_chooser_controller.h"

#include <memory>

#include "base/command_line.h"
#import "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/gcm/fake_gcm_profile_service.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/fake_account_fetcher_service_builder.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#include "chrome/browser/ui/cocoa/l10n_util.h"
#include "chrome/browser/ui/cocoa/test/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/test/scoped_force_rtl_mac.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"

const std::string kGaiaId = "gaiaid-user@gmail.com";
const std::string kEmail = "user@gmail.com";
const std::string kSecondaryEmail = "user2@gmail.com";
const std::string kSecondaryGaiaId = "gaiaid-user2@gmail.com";
const std::string kLoginToken = "oauth2_login_token";

class ProfileChooserControllerTest : public CocoaProfileTest {
 public:
  ProfileChooserControllerTest() {
    TestingProfile::TestingFactories factories;
    factories.push_back(
        std::make_pair(ProfileOAuth2TokenServiceFactory::GetInstance(),
                       BuildFakeProfileOAuth2TokenService));
    factories.push_back(
        std::make_pair(AccountFetcherServiceFactory::GetInstance(),
                       FakeAccountFetcherServiceBuilder::BuildForTests));
    AddTestingFactories(factories);
  }

  void SetUp() override {
    CocoaProfileTest::SetUp();

    ASSERT_TRUE(browser()->profile());

    gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(), gcm::FakeGCMProfileService::Build);

    testing_profile_manager()->CreateTestingProfile(
        "test1", std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::ASCIIToUTF16("Test 1"), 0, std::string(), testing_factories());
    testing_profile_manager()->CreateTestingProfile(
        "test2", std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::ASCIIToUTF16("Test 2"), 1, std::string(),
        TestingProfile::TestingFactories());

    menu_ = new AvatarMenu(
        testing_profile_manager()->profile_attributes_storage(), NULL, NULL);
    menu_->RebuildMenu();

    // There should be the default profile + two profiles we created.
    EXPECT_EQ(3U, menu_->GetNumberOfItems());
  }

  void TearDown() override {
    [controller() close];
    controller_.reset();
    CocoaProfileTest::TearDown();
  }

  void StartProfileChooserController() {
    StartProfileChooserControllerWithTutorialMode(profiles::TUTORIAL_MODE_NONE);
  }

  void StartProfileChooserControllerWithTutorialMode(
      profiles::TutorialMode mode) {
    NSRect frame = [test_window() frame];
    NSPoint point = NSMakePoint(NSMidX(frame), NSMidY(frame));
    controller_.reset([[ProfileChooserController alloc]
        initWithBrowser:browser()
             anchoredAt:point
               viewMode:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER
           tutorialMode:mode
            serviceType:signin::GAIA_SERVICE_TYPE_NONE
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN]);
    [controller_ showWindow:nil];
  }

  void AssertRightClickTutorialShown() {
    // The right click menu doesn't exist in the MD user menu, so it doesn't
    // show the tutorial.
    if (switches::IsMaterialDesignUserMenu())
      return;

    NSArray* subviews = [[[controller() window] contentView] subviews];
    ASSERT_EQ(2U, [subviews count]);
    subviews = [[subviews objectAtIndex:0] subviews];

    // There should be 4 views: the tutorial, the active profile card, a
    // separator and the options view.
    ASSERT_EQ(4U, [subviews count]);

    // The tutorial is the topmost view, so the last in the array. It should
    // contain 3 views: the title, the content text and the OK button.
    NSArray* tutorialSubviews = [[subviews objectAtIndex:3] subviews];
    ASSERT_EQ(3U, [tutorialSubviews count]);

    NSTextField* tutorialTitle = base::mac::ObjCCastStrict<NSTextField>(
        [tutorialSubviews objectAtIndex:2]);
    EXPECT_GT([[tutorialTitle stringValue] length], 0U);

    NSTextField* tutorialContent = base::mac::ObjCCastStrict<NSTextField>(
        [tutorialSubviews objectAtIndex:1]);
    EXPECT_GT([[tutorialContent stringValue] length], 0U);

    NSButton* tutorialOKButton = base::mac::ObjCCastStrict<NSButton>(
        [tutorialSubviews objectAtIndex:0]);
    EXPECT_GT([[tutorialOKButton title] length], 0U);
  }

  void StartFastUserSwitcher() {
    NSRect frame = [test_window() frame];
    NSPoint point = NSMakePoint(NSMidX(frame), NSMidY(frame));
    controller_.reset([[ProfileChooserController alloc]
        initWithBrowser:browser()
             anchoredAt:point
               viewMode:profiles::BUBBLE_VIEW_MODE_FAST_PROFILE_CHOOSER
           tutorialMode:profiles::TUTORIAL_MODE_NONE
            serviceType:signin::GAIA_SERVICE_TYPE_NONE
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN]);
    [controller_ showWindow:nil];
  }

  void SignInFirstProfile() {
    std::vector<ProfileAttributesEntry*> entries = testing_profile_manager()->
        profile_attributes_storage()->GetAllProfilesAttributes();
    ASSERT_LE(1U, entries.size());
    ProfileAttributesEntry* entry = entries.front();
    entry->SetAuthInfo(kGaiaId, base::ASCIIToUTF16(kEmail));
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
  StartProfileChooserController();

  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  // Three profiles means we should have one active card, one separator and
  // one option buttons view. We also have an update promo for the new avatar
  // menu.
  // TODO(noms): Enforcing 4U fails on the waterfall debug bots, but it's not
  // reproducible anywhere else.
  ASSERT_GE([subviews count], 3U);

  // There should be one button in the option buttons view.
  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  NSButton* userSwitcherButton;
  if (switches::IsMaterialDesignUserMenu()) {
    // There are 2 buttons in the initial layout: "Manage People" and "Guest".
    ASSERT_EQ(2U, [buttonSubviews count]);
    // There should be a user switcher button.
    userSwitcherButton =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);
  } else {
    // For non-material-design user menu, there should be two buttons and a
    // separator in the option buttons view.
    ASSERT_EQ(3U, [buttonSubviews count]);

    // There should be an incognito button.
    NSButton* incognitoButton =
        base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);
    EXPECT_EQ(@selector(goIncognito:), [incognitoButton action]);
    EXPECT_EQ(controller(), [incognitoButton target]);

    // There should be a separator.
    EXPECT_TRUE([[subviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

    // There should be a user switcher button.
    userSwitcherButton =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:2]);
  }

  EXPECT_EQ(@selector(showUserManager:), [userSwitcherButton action]);
  EXPECT_EQ(controller(), [userSwitcherButton target]);

  NSUInteger lastSubviewIndex = switches::IsMaterialDesignUserMenu() ? 4 : 2;
  NSArray* activeCardSubviews =
      [[subviews objectAtIndex:lastSubviewIndex] subviews];

  // There should be the profile avatar, name and links container in the active
  // card view. The links displayed in the container are checked separately.
  // In the MD user menu, the profile avatar and name are in the same subview.
  if (switches::IsMaterialDesignUserMenu()) {
    ASSERT_EQ(2U, [activeCardSubviews count]);
  } else {
    ASSERT_EQ(3U, [activeCardSubviews count]);

    // Profile icon.
    NSView* activeProfileImage = [activeCardSubviews objectAtIndex:2];
    EXPECT_TRUE([activeProfileImage isKindOfClass:[NSButton class]]);

    // Profile name.
    NSView* activeProfileName = [activeCardSubviews objectAtIndex:1];
    EXPECT_TRUE([activeProfileName isKindOfClass:[NSButton class]]);
    EXPECT_EQ(menu()->GetItemAt(0).name, base::SysNSStringToUTF16(
        [base::mac::ObjCCast<NSButton>(activeProfileName) title]));
  }
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

// Check to see if the bubble is aligned properly in LTR and RTL format.
TEST_F(ProfileChooserControllerTest, BubbleAlignment) {
  // Test the LTR format.
  StartProfileChooserController();
  EXPECT_EQ(info_bubble::kAlignTrailingEdgeToAnchorEdge,
            [[controller() bubble] alignment]);
  [controller() close];

  // Force to RTL format
  cocoa_l10n_util::ScopedForceRTLMac rtl;
  StartProfileChooserController();
  info_bubble::BubbleAlignment expected_alignment =
      cocoa_l10n_util::ShouldFlipWindowControlsInRTL()
          ? info_bubble::kAlignTrailingEdgeToAnchorEdge
          : info_bubble::kAlignLeadingEdgeToAnchorEdge;
  EXPECT_EQ(expected_alignment, [[controller() bubble] alignment]);
  [controller() close];
}

TEST_F(ProfileChooserControllerTest, RightClickTutorialShownAfterWelcome) {
  // The welcome upgrade tutorial takes precedence so show it then dismiss it.
  // The right click tutorial should be shown right away.
  StartProfileChooserControllerWithTutorialMode(
      profiles::TUTORIAL_MODE_WELCOME_UPGRADE);

  [controller() dismissTutorial:nil];
  AssertRightClickTutorialShown();
}

TEST_F(ProfileChooserControllerTest, RightClickTutorialShownAfterReopen) {
  // The welcome upgrade tutorial takes precedence so show it then close the
  // menu. Reopening the menu should show the tutorial.
  StartProfileChooserController();

  [controller() close];
  StartProfileChooserController();
  AssertRightClickTutorialShown();

  // The tutorial must be manually dismissed so it should still be shown after
  // closing and reopening the menu,
  [controller() close];
  StartProfileChooserController();
  AssertRightClickTutorialShown();
}

TEST_F(ProfileChooserControllerTest, RightClickTutorialNotShownAfterDismiss) {
  // The welcome upgrade tutorial takes precedence so show it then close the
  // menu. Reopening the menu should show the tutorial.
  StartProfileChooserController();

  [controller() close];
  StartProfileChooserControllerWithTutorialMode(
      profiles::TUTORIAL_MODE_RIGHT_CLICK_SWITCHING);
  AssertRightClickTutorialShown();

  // Dismissing the tutorial should prevent it from being shown forever.
  [controller() dismissTutorial:nil];
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  // There should be 3 views since there's no tutorial. There are 2 extra
  // buttons in the MD user menu.
  NSUInteger viewsCount = switches::IsMaterialDesignUserMenu() ? 5 : 3;
  ASSERT_EQ(viewsCount, [subviews count]);

  // Closing and reopening the menu shouldn't show the tutorial.
  [controller() close];
  StartProfileChooserControllerWithTutorialMode(
      profiles::TUTORIAL_MODE_RIGHT_CLICK_SWITCHING);
  subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  ASSERT_EQ(viewsCount, [subviews count]);
}

TEST_F(ProfileChooserControllerTest, OtherProfilesSortedAlphabetically) {
  // This test is related to the fast user switcher, which doesn't exist under
  // the MD user menu.
  if (switches::IsMaterialDesignUserMenu())
    return;
  // Add two extra profiles, to make sure sorting is alphabetical and not
  // by order of creation.
  testing_profile_manager()->CreateTestingProfile(
      "test3", std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
      base::ASCIIToUTF16("New Profile"), 1, std::string(),
      TestingProfile::TestingFactories());
  testing_profile_manager()->CreateTestingProfile(
      "test4", std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
      base::ASCIIToUTF16("Another Test"), 1, std::string(),
      TestingProfile::TestingFactories());
  StartFastUserSwitcher();

  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  NSString* sortedNames[] = { @"Another Test",
                              @"New Profile",
                              @"Test 1",
                              @"Test 2" };
  // There are four "other" profiles, each with a button and a separator.
  ASSERT_EQ([subviews count], 8U);
  // There should be four "other profiles" items, sorted alphabetically. The
  // "other profiles" start at index 2 (after the option buttons view and its
  // separator), and each have a separator. We need to iterate through the
  // profiles in the order displayed in the bubble, which is opposite from the
  // drawn order.
  int sortedNameIndex = 0;
  for (int i = 7; i > 0; i -= 2) {
    // The item at index i is the separator.
    NSButton* button = base::mac::ObjCCast<NSButton>(
        [subviews objectAtIndex:i-1]);
    EXPECT_TRUE(
        [[button title] isEqualToString:sortedNames[sortedNameIndex++]]);
  }
}

TEST_F(ProfileChooserControllerTest,
    LocalProfileActiveCardLinksWithNewMenu) {
  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  // The active card is the last subview and the MD User Menu has 2 extra
  // buttons.
  NSUInteger lastSubviewIndex = switches::IsMaterialDesignUserMenu() ? 4 : 2;
  NSArray* activeCardSubviews =
      [[subviews objectAtIndex:lastSubviewIndex] subviews];
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
      base::CommandLine::ForCurrentProcess());

  SignInFirstProfile();

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  // The active card is the last subview and the MD User Menu has 2 extra
  // buttons.
  NSUInteger lastSubviewIndex = switches::IsMaterialDesignUserMenu() ? 4 : 2;
  NSArray* activeCardSubviews =
      [[subviews objectAtIndex:lastSubviewIndex] subviews];
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
  SignInFirstProfile();

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  // The active card is the last subview and the MD User Menu has 2 extra
  // buttons.
  NSUInteger lastSubviewIndex = switches::IsMaterialDesignUserMenu() ? 4 : 2;
  NSArray* activeCardSubviews =
      [[subviews objectAtIndex:lastSubviewIndex] subviews];
  NSArray* activeCardLinks = [[activeCardSubviews objectAtIndex:0] subviews];

  if (switches::IsMaterialDesignUserMenu()) {
    // There is the profile avatar and the profile name.
    ASSERT_EQ(2U, [activeCardLinks count]);
  } else {
    // There is one disabled button with the user's email.
    ASSERT_EQ(1U, [activeCardLinks count]);
    NSButton* emailButton =
        base::mac::ObjCCast<NSButton>([activeCardLinks objectAtIndex:0]);
    EXPECT_EQ(kEmail, base::SysNSStringToUTF8([emailButton title]));
    EXPECT_EQ(nil, [emailButton action]);
    EXPECT_FALSE([emailButton isEnabled]);
  }
}

TEST_F(ProfileChooserControllerTest, AccountManagementLayout) {
  switches::EnableAccountConsistencyForTesting(
      base::CommandLine::ForCurrentProcess());

  SignInFirstProfile();

  // Mark that we are using the profile name on purpose, so that we don't
  // fallback to testing the algorithm that chooses which default name
  // should be used.
  ProfileAttributesEntry* entry = testing_profile_manager()->
      profile_attributes_storage()->GetAllProfilesAttributes().front();
  entry->SetIsUsingDefaultName(false);

  // Set up the AccountTrackerService, signin manager and the OAuth2Tokens.
  Profile* profile = browser()->profile();
  AccountTrackerServiceFactory::GetForProfile(profile)
      ->SeedAccountInfo(kGaiaId, kEmail);
  AccountTrackerServiceFactory::GetForProfile(profile)
      ->SeedAccountInfo(kSecondaryGaiaId, kSecondaryEmail);
  SigninManagerFactory::GetForProfile(profile)
      ->SetAuthenticatedAccountInfo(kGaiaId, kEmail);
  std::string account_id =
      SigninManagerFactory::GetForProfile(profile)->GetAuthenticatedAccountId();
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile)
      ->UpdateCredentials(account_id, kLoginToken);
  account_id = AccountTrackerServiceFactory::GetForProfile(profile)
                   ->PickAccountIdForAccount(kSecondaryGaiaId, kSecondaryEmail);
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile)
      ->UpdateCredentials(account_id, kLoginToken);

  StartProfileChooserController();
  [controller() initMenuContentsWithView:
      profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];

  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  // There should be one active card, one accounts container, two separators
  // and one option buttons view. In the MD User Menu, there are 2 more buttons.
  NSUInteger viewsCount = switches::IsMaterialDesignUserMenu() ? 7 : 5;
  ASSERT_EQ(viewsCount, [subviews count]);

  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  NSButton* userSwitcherButton;
  if (switches::IsMaterialDesignUserMenu()) {
    // There should be two buttons in the option buttons view.
    ASSERT_EQ(2U, [buttonSubviews count]);
    // There should be a user switcher button.
    userSwitcherButton =
        base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);
  } else {
    // For non-material-design user menu, there should be two buttons and one
    // separator in the option buttons view.
    ASSERT_EQ(3U, [buttonSubviews count]);

    // There should be an incognito button.
    NSButton* incognitoButton =
        base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);
    EXPECT_EQ(@selector(goIncognito:), [incognitoButton action]);
    EXPECT_EQ(controller(), [incognitoButton target]);

    // There should be a separator.
    EXPECT_TRUE([[buttonSubviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

    // There should be a user switcher button.
    userSwitcherButton =
        base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:2]);
  }

  EXPECT_EQ(@selector(showUserManager:), [userSwitcherButton action]);
  EXPECT_EQ(controller(), [userSwitcherButton target]);

  NSUInteger accountsViewIndex = switches::IsMaterialDesignUserMenu() ? 4 : 2;
  // In the accounts view, there should be the account list container
  // accounts and one "add accounts" button.
  NSArray* accountsSubviews =
      [[subviews objectAtIndex:accountsViewIndex] subviews];
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
  if (switches::IsMaterialDesignUserMenu()) {
    // In the MD user menu, the profile name and avatar are in the same subview.
    ASSERT_EQ(2U, [activeCardSubviews count]);
  } else {
    ASSERT_EQ(3U, [activeCardSubviews count]);

    // Profile icon.
    NSView* activeProfileImage = [activeCardSubviews objectAtIndex:2];
    EXPECT_TRUE([activeProfileImage isKindOfClass:[NSButton class]]);

    // Profile name.
    NSView* activeProfileName = [activeCardSubviews objectAtIndex:1];
    EXPECT_TRUE([activeProfileName isKindOfClass:[NSButton class]]);
    EXPECT_EQ(menu()->GetItemAt(0).name, base::SysNSStringToUTF16(
        [base::mac::ObjCCast<NSButton>(activeProfileName) title]));

    // Profile links. This is a local profile, so there should be a signin
    // button.
    NSArray* linksSubviews = [[activeCardSubviews objectAtIndex:0] subviews];
    ASSERT_EQ(1U, [linksSubviews count]);
    NSButton* link = base::mac::ObjCCast<NSButton>(
        [linksSubviews objectAtIndex:0]);
    EXPECT_EQ(@selector(hideAccountManagement:), [link action]);
    EXPECT_EQ(controller(), [link target]);
  }
}

TEST_F(ProfileChooserControllerTest, SignedInProfileLockDisabled) {
  switches::EnableNewProfileManagementForTesting(
      base::CommandLine::ForCurrentProcess());

  SignInFirstProfile();

  // The preference, not the email, determines whether the profile can lock.
  browser()->profile()->GetPrefs()->SetString(
      prefs::kGoogleServicesHostedDomain, "chromium.org");

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  if (switches::IsMaterialDesignUserMenu()) {
    // There will be two buttons in the option buttons view.
    ASSERT_EQ(2U, [buttonSubviews count]);
  } else {
    // For non-material-design user menu, there will be two buttons and one
    // separators in the option buttons view.
    ASSERT_EQ(3U, [buttonSubviews count]);
  }

  // The last button should not be the lock button.
  NSButton* lastButton =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);
  ASSERT_TRUE(lastButton);
  EXPECT_NE(@selector(lockProfile:), [lastButton action]);
}

TEST_F(ProfileChooserControllerTest, SignedInProfileLockEnabled) {
  switches::EnableNewProfileManagementForTesting(
      base::CommandLine::ForCurrentProcess());

  SignInFirstProfile();

  // The preference, not the email, determines whether the profile can lock.
  browser()->profile()->GetPrefs()->SetString(
      prefs::kGoogleServicesHostedDomain, "google.com");
  // Lock is only available where a supervised user is present.
  ProfileAttributesEntry* entry = testing_profile_manager()->
      profile_attributes_storage()->GetAllProfilesAttributes().front();
  entry->SetSupervisedUserId(kEmail);

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  if (switches::IsMaterialDesignUserMenu()) {
    // There will be two buttons and one separator in the option buttons view.
    ASSERT_EQ(3U, [buttonSubviews count]);
  } else {
    // FOr non-material-design user menu, There will be three buttons and two
    // separators in the option buttons view.
    ASSERT_EQ(5U, [buttonSubviews count]);
  }

  // There should be a lock button.
  NSButton* lockButton =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);
  ASSERT_TRUE(lockButton);
  EXPECT_EQ(@selector(lockProfile:), [lockButton action]);
  EXPECT_EQ(controller(), [lockButton target]);
  EXPECT_TRUE([lockButton isEnabled]);
}
