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
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
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
#include "components/signin/core/browser/scoped_account_consistency.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"

using ::testing::Return;

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
    factories.push_back(std::make_pair(ProfileSyncServiceFactory::GetInstance(),
                                       BuildMockProfileSyncService));
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

    mock_sync_service_ = static_cast<browser_sync::ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(
            browser()->profile()));

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
    NSRect frame = [test_window() frame];
    NSPoint point = NSMakePoint(NSMidX(frame), NSMidY(frame));
    controller_.reset([[ProfileChooserController alloc]
        initWithBrowser:browser()
             anchoredAt:point
               viewMode:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER
            serviceType:signin::GAIA_SERVICE_TYPE_NONE
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN]);
    [controller_ showWindow:nil];
  }

  void SignInFirstProfile() {
    std::vector<ProfileAttributesEntry*> entries =
        testing_profile_manager()
            ->profile_attributes_storage()
            ->GetAllProfilesAttributesSortedByName();
    ASSERT_LE(1U, entries.size());
    ProfileAttributesEntry* entry = entries.front();
    entry->SetAuthInfo(kGaiaId, base::ASCIIToUTF16(kEmail));
  }

  void SuppressSyncConfirmationError() {
    EXPECT_CALL(*mock_sync_service_, IsFirstSetupComplete())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_sync_service_, IsFirstSetupInProgress())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_sync_service_, IsSyncConfirmationNeeded())
        .WillRepeatedly(Return(false));
  }

  ProfileChooserController* controller() { return controller_; }
  AvatarMenu* menu() { return menu_; }

  void ExpectGuestButton(NSButton* guest_button) {
    ASSERT_TRUE(guest_button);
    EXPECT_EQ(@selector(switchToGuest:), [guest_button action]);
    EXPECT_EQ(controller(), [guest_button target]);
    EXPECT_TRUE([guest_button isEnabled]);
  }

  void ExpectManagePeopleButton(NSButton* manage_people_button) {
    ASSERT_TRUE(manage_people_button);
    EXPECT_EQ(@selector(showUserManager:), [manage_people_button action]);
    EXPECT_EQ(controller(), [manage_people_button target]);
    EXPECT_TRUE([manage_people_button isEnabled]);
  }

 private:
  base::scoped_nsobject<ProfileChooserController> controller_;
  browser_sync::ProfileSyncServiceMock* mock_sync_service_ = nullptr;

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
  // There are 2 buttons in the initial layout: "Manage People" and "Guest".
  ASSERT_EQ(2U, [buttonSubviews count]);
  // There should be a user switcher button.
  userSwitcherButton =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);

  EXPECT_EQ(@selector(showUserManager:), [userSwitcherButton action]);
  EXPECT_EQ(controller(), [userSwitcherButton target]);

  NSUInteger lastSubviewIndex = 4;
  NSArray* activeCardSubviews =
      [[subviews objectAtIndex:lastSubviewIndex] subviews];

  // There should be the profile avatar, name and links container in the active
  // card view. The links displayed in the container are checked separately.
  // In the MD user menu, the profile avatar and name are in the same subview.
  ASSERT_EQ(2U, [activeCardSubviews count]);
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

TEST_F(ProfileChooserControllerTest,
    LocalProfileActiveCardLinksWithNewMenu) {
  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  // The active card is the last subview and the MD User Menu has 2 extra
  // buttons.
  NSUInteger lastSubviewIndex = 4;
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
  signin::ScopedAccountConsistencyMirror scoped_mirror;

  SignInFirstProfile();

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  // The active card is the last subview and the MD User Menu has 2 extra
  // buttons.
  NSUInteger lastSubviewIndex = 4;
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
  NSUInteger lastSubviewIndex = 4;
  NSArray* activeCardSubviews =
      [[subviews objectAtIndex:lastSubviewIndex] subviews];
  NSArray* activeCardLinks = [[activeCardSubviews objectAtIndex:0] subviews];

  // There is the profile avatar and the profile name.
  ASSERT_EQ(2U, [activeCardLinks count]);
}

TEST_F(ProfileChooserControllerTest, AccountManagementLayout) {
  signin::ScopedAccountConsistencyMirror scoped_mirror;

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

  SuppressSyncConfirmationError();

  StartProfileChooserController();
  [controller()
      showMenuWithViewMode:profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];

  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  // There should be one active card, one accounts container, two separators
  // and one option buttons view. In the MD User Menu, there are 2 more buttons.
  NSUInteger viewsCount = 7;
  ASSERT_EQ(viewsCount, [subviews count]);

  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  NSButton* userSwitcherButton;
  // There should be two buttons in the option buttons view.
  ASSERT_EQ(2U, [buttonSubviews count]);
  // There should be a user switcher button.
  userSwitcherButton =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);

  EXPECT_EQ(@selector(showUserManager:), [userSwitcherButton action]);
  EXPECT_EQ(controller(), [userSwitcherButton target]);

  NSUInteger accountsViewIndex = 4;
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
  // In the MD user menu, the profile name and avatar are in the same subview.
  ASSERT_EQ(2U, [activeCardSubviews count]);
}

TEST_F(ProfileChooserControllerTest, SignedInProfileLockDisabled) {
  SignInFirstProfile();

  // The preference, not the email, determines whether the profile can lock.
  browser()->profile()->GetPrefs()->SetString(
      prefs::kGoogleServicesHostedDomain, "chromium.org");

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  // There will be two buttons in the option buttons view.
  ASSERT_EQ(2U, [buttonSubviews count]);

  // The last button should not be the lock button.
  NSButton* lastButton =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);
  ASSERT_TRUE(lastButton);
  EXPECT_NE(@selector(lockProfile:), [lastButton action]);
}

TEST_F(ProfileChooserControllerTest, SignedInProfileLockEnabled) {
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
  // There will be two buttons and one separator in the option buttons view.
  ASSERT_EQ(3U, [buttonSubviews count]);

  // There should be a lock button.
  NSButton* lockButton =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);
  ASSERT_TRUE(lockButton);
  EXPECT_EQ(@selector(lockProfile:), [lockButton action]);
  EXPECT_EQ(controller(), [lockButton target]);
  EXPECT_TRUE([lockButton isEnabled]);
}

TEST_F(ProfileChooserControllerTest, RegularProfileWithManagePeopleAndGuest) {
  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  ASSERT_EQ(2U, [buttonSubviews count]);

  NSButton* manage_people_button =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);
  ExpectManagePeopleButton(manage_people_button);

  NSButton* guest_button =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:1]);
  ExpectGuestButton(guest_button);
}

TEST_F(ProfileChooserControllerTest, SupervisedProfileWithManagePeopleOnly) {
  TestingProfile* test = static_cast<TestingProfile*>(browser()->profile());
  test->SetSupervisedUserId(kSecondaryGaiaId);

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  ASSERT_EQ(2U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  ASSERT_EQ(1U, [buttonSubviews count]);

  NSButton* manage_people_button =
      base::mac::ObjCCast<NSButton>([buttonSubviews objectAtIndex:0]);
  ExpectManagePeopleButton(manage_people_button);
}
