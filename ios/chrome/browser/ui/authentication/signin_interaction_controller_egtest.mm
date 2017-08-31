// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/chrome/browser/bookmarks/bookmark_new_generation_features.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_url_command.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/testing/wait_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SecondarySignInButton;

namespace {

// Returns a fake identity.
ChromeIdentity* GetFakeIdentity1() {
  return [FakeChromeIdentity identityWithEmail:@"foo@gmail.com"
                                        gaiaID:@"fooID"
                                          name:@"Fake Foo"];
}

// Returns a second fake identity.
ChromeIdentity* GetFakeIdentity2() {
  return [FakeChromeIdentity identityWithEmail:@"bar@gmail.com"
                                        gaiaID:@"barID"
                                          name:@"Fake Bar"];
}

ChromeIdentity* GetFakeManagedIdentity() {
  return [FakeChromeIdentity identityWithEmail:@"managed@foo.com"
                                        gaiaID:@"managedID"
                                          name:@"Fake Managed"];
}

// Changes the EarlGrey synchronization status to |enabled|.
void SetEarlGreySynchronizationEnabled(BOOL enabled) {
  [[GREYConfiguration sharedInstance]
          setValue:[NSNumber numberWithBool:enabled]
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
}

// Taps the button with accessibility label |label|.
void TapButtonWithAccessibilityLabel(NSString* label) {
  id<GREYMatcher> matcher =
      chrome_test_util::ButtonWithAccessibilityLabel(label);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

// Taps the button with accessibility labelId |message_id|.
void TapButtonWithLabelId(int message_id) {
  id<GREYMatcher> matcher =
      chrome_test_util::ButtonWithAccessibilityLabelId(message_id);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

// Wait until |matcher| is accessible (not nil)
void WaitForMatcher(id<GREYMatcher> matcher) {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()
                                                             error:&error];
    return error == nil;
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForUIElementTimeout, condition),
             @"Waiting for matcher %@ failed.", matcher);
}

// Asserts that |identity| is actually signed in to the active profile.
void AssertAuthenticatedIdentityInActiveProfile(ChromeIdentity* identity) {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  AccountInfo info =
      ios::SigninManagerFactory::GetForBrowserState(browser_state)
          ->GetAuthenticatedAccountInfo();

  GREYAssertEqual(base::SysNSStringToUTF8(identity.gaiaID), info.gaia,
                  @"Gaia ID of signed in user isn't %@ but %s", identity.gaiaID,
                  info.gaia.c_str());
}

}  // namespace

@interface SigninInteractionControllerTestCase : ChromeTestCase
@end

@implementation SigninInteractionControllerTestCase

// Tests that opening the sign-in screen from the Settings and signing in works
// correctly when there is already an identity on the device.
- (void)testSignInOneUser {
  // Set up a fake identity.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [ChromeEarlGreyUI signInToIdentityByEmail:identity.userEmail];
  [ChromeEarlGreyUI confirmSigninConfirmationDialog];
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check |identity| is signed-in.
  AssertAuthenticatedIdentityInActiveProfile(identity);
}

// Tests signing in with one account, switching sync account to a second and
// choosing to keep the browsing data separate during the switch.
- (void)testSignInSwitchAccountsAndKeepDataSeparate {
  // Set up the fake identities.
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* identity1 = GetFakeIdentity1();
  ChromeIdentity* identity2 = GetFakeIdentity2();
  identity_service->AddIdentity(identity1);
  identity_service->AddIdentity(identity2);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [ChromeEarlGreyUI signInToIdentityByEmail:identity1.userEmail];
  [ChromeEarlGreyUI confirmSigninConfirmationDialog];
  AssertAuthenticatedIdentityInActiveProfile(identity1);

  // Open accounts settings, then sync settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsAccountButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AccountsSyncButton()]
      performAction:grey_tap()];

  // Switch Sync account to |identity2|.
  TapButtonWithAccessibilityLabel(identity2.userEmail);
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::SettingsImportDataKeepSeparateButton()]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_IOS_OPTIONS_IMPORT_DATA_CONTINUE_BUTTON);

  // Check the signed-in user did change.
  AssertAuthenticatedIdentityInActiveProfile(identity2);

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests signing in with one account, switching sync account to a second and
// choosing to import the browsing data during the switch.
- (void)testSignInSwitchAccountsAndImportData {
  // Set up the fake identities.
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* identity1 = GetFakeIdentity1();
  ChromeIdentity* identity2 = GetFakeIdentity2();
  identity_service->AddIdentity(identity1);
  identity_service->AddIdentity(identity2);

  // Sign in to |identity1|.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [ChromeEarlGreyUI signInToIdentityByEmail:identity1.userEmail];
  [ChromeEarlGreyUI confirmSigninConfirmationDialog];
  AssertAuthenticatedIdentityInActiveProfile(identity1);

  // Open accounts settings, then sync settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsAccountButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AccountsSyncButton()]
      performAction:grey_tap()];

  // Switch Sync account to |identity2|.
  TapButtonWithAccessibilityLabel(identity2.userEmail);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsImportDataImportButton()]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_IOS_OPTIONS_IMPORT_DATA_CONTINUE_BUTTON);

  // Check the signed-in user did change.
  AssertAuthenticatedIdentityInActiveProfile(identity2);

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that switching from a managed account to a non-managed account works
// correctly and displays the expected warnings.
- (void)testSignInSwitchManagedAccount {
  // Set up the fake identities.
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* managed_identity = GetFakeManagedIdentity();
  ChromeIdentity* identity = GetFakeIdentity1();
  identity_service->AddIdentity(managed_identity);
  identity_service->AddIdentity(identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [ChromeEarlGreyUI signInToIdentityByEmail:managed_identity.userEmail];

  // Accept warning for signing into a managed identity, with synchronization
  // off due to an infinite spinner.
  SetEarlGreySynchronizationEnabled(NO);
  WaitForMatcher(chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON));
  TapButtonWithLabelId(IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON);
  SetEarlGreySynchronizationEnabled(YES);

  [ChromeEarlGreyUI confirmSigninConfirmationDialog];
  AssertAuthenticatedIdentityInActiveProfile(managed_identity);

  // Switch Sync account to |identity|.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsAccountButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AccountsSyncButton()]
      performAction:grey_tap()];

  TapButtonWithAccessibilityLabel(identity.userEmail);

  SetEarlGreySynchronizationEnabled(NO);
  WaitForMatcher(chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_MANAGED_SWITCH_ACCEPT_BUTTON));
  TapButtonWithLabelId(IDS_IOS_MANAGED_SWITCH_ACCEPT_BUTTON);
  SetEarlGreySynchronizationEnabled(YES);

  AssertAuthenticatedIdentityInActiveProfile(identity);

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that signing out from the Settings works correctly.
- (void)testSignInDisconnectFromChrome {
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Sign in to |identity|.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [ChromeEarlGreyUI signInToIdentityByEmail:identity.userEmail];
  [ChromeEarlGreyUI confirmSigninConfirmationDialog];
  AssertAuthenticatedIdentityInActiveProfile(identity);

  // Go to Accounts Settings and tap the sign out button.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsAccountButton()]
      performAction:grey_tap()];

  const CGFloat scroll_displacement = 100.0;
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::SignOutAccountsButton(),
                                   grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:chrome_test_util::SettingsAccountsCollectionView()]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check that there is no signed in user.
  AssertAuthenticatedIdentityInActiveProfile(nil);
}

// Tests that signing out of a managed account from the Settings works
// correctly.
- (void)testSignInDisconnectFromChromeManaged {
  ChromeIdentity* identity = GetFakeManagedIdentity();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [ChromeEarlGreyUI signInToIdentityByEmail:identity.userEmail];

  // Synchronization off due to an infinite spinner.
  SetEarlGreySynchronizationEnabled(NO);
  WaitForMatcher(chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON));
  TapButtonWithLabelId(IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON);
  SetEarlGreySynchronizationEnabled(YES);

  [ChromeEarlGreyUI confirmSigninConfirmationDialog];
  AssertAuthenticatedIdentityInActiveProfile(identity);

  // Go to Accounts Settings and tap the sign out button.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsAccountButton()]
      performAction:grey_tap()];

  const CGFloat scroll_displacement = 100.0;
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::SignOutAccountsButton(),
                                   grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:chrome_test_util::SettingsAccountsCollectionView()]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_IOS_MANAGED_DISCONNECT_DIALOG_ACCEPT);
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check that there is no signed in user.
  AssertAuthenticatedIdentityInActiveProfile(nil);
}

// Tests that signing in, tapping the Settings link on the confirmation screen
// and closing the Settings correctly leaves the user signed in without any
// Settings shown.
- (void)testSignInOpenSettings {
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [ChromeEarlGreyUI signInToIdentityByEmail:identity.userEmail];

  // Tap Settings link.
  id<GREYMatcher> settings_link_matcher = grey_allOf(
      grey_accessibilityLabel(@"Settings"), grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:settings_link_matcher]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // All Settings should be gone and user signed in.
  id<GREYMatcher> settings_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_SETTINGS_TITLE);
  [[EarlGrey selectElementWithMatcher:settings_matcher]
      assertWithMatcher:grey_notVisible()];
  AssertAuthenticatedIdentityInActiveProfile(identity);
}

// Opens the sign in screen and then cancel it by opening a new tab. Ensures
// that the sign in screen is correctly dismissed. crbug.com/462200
- (void)testSignInCancelIdentityPicker {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

  // Open new tab to cancel sign-in.
  OpenUrlCommand* command =
      [[OpenUrlCommand alloc] initWithURLFromChrome:GURL("about:blank")];
  [chrome_test_util::DispatcherForActiveViewController() openURL:command];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  id<GREYMatcher> signin_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_DESCRIPTION);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Settings.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Opens the add account screen and then cancels it by opening a new tab.
// Ensures that the add account screen is correctly dismissed. crbug.com/462200
- (void)testSignInCancelAddAccount {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

  // Open Add Account screen.
  id<GREYMatcher> add_account_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_ADD_ACCOUNT_BUTTON);
  [[EarlGrey selectElementWithMatcher:add_account_matcher]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Open new tab to cancel sign-in.
  OpenUrlCommand* command =
      [[OpenUrlCommand alloc] initWithURLFromChrome:GURL("about:blank")];
  [chrome_test_util::DispatcherForActiveViewController() openURL:command];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  id<GREYMatcher> signin_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_DESCRIPTION);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Settings.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Starts an authentication flow and cancel it by opening a new tab. Ensures
// that the authentication flow is correctly canceled and dismissed.
// crbug.com/462202
- (void)testSignInCancelAuthenticationFlow {
  // Set up the fake identities.
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* identity1 = GetFakeIdentity1();
  ChromeIdentity* identity2 = GetFakeIdentity2();
  identity_service->AddIdentity(identity1);
  identity_service->AddIdentity(identity2);

  // This signs in |identity2| first, ensuring that the "Clear Data Before
  // Syncing" dialog is shown during the second sign-in. This dialog will
  // effectively block the authentication flow, ensuring that the authentication
  // flow is always still running when the sign-in is being cancelled.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [ChromeEarlGreyUI signInToIdentityByEmail:identity2.userEmail];
  [ChromeEarlGreyUI confirmSigninConfirmationDialog];
  AssertAuthenticatedIdentityInActiveProfile(identity2);

  // Go to Accounts Settings and tap the sign out button.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsAccountButton()]
      performAction:grey_tap()];

  const CGFloat scroll_displacement = 100.0;
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::SignOutAccountsButton(),
                                   grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:chrome_test_util::SettingsAccountsCollectionView()]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
  AssertAuthenticatedIdentityInActiveProfile(nil);
  [[EarlGrey selectElementWithMatcher:SecondarySignInButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI signInToIdentityByEmail:identity1.userEmail];

  // Open new tab to cancel sign-in.
  OpenUrlCommand* command =
      [[OpenUrlCommand alloc] initWithURLFromChrome:GURL("about:blank")];
  [chrome_test_util::DispatcherForActiveViewController() openURL:command];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  id<GREYMatcher> signin_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_DESCRIPTION);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Settings.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
  AssertAuthenticatedIdentityInActiveProfile(nil);
}

// Opens the sign in screen from the bookmarks and then cancel it by opening a
// new tab. Ensures that the sign in screen is correctly dismissed.
// Regression test for crbug.com/596029.
// TODO(crbug.com/695749): Check if we need to rewrite this test for the new
// Bookmarks UI.
- (void)testSignInCancelFromBookmarks {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      bookmark_new_generation::features::kBookmarkNewGeneration);

  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Open Bookmarks and tap on Sign In promo button.
  const CGFloat scroll_displacement = 50.0;
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kToolsMenuBookmarksId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      performAction:grey_tap()];

  if (!IsIPadIdiom()) {
    // Opens the bookmark manager sidebar on handsets.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Menu")]
        performAction:grey_tap()];
  }

  // Selects the top level folder (Sign In promo is only shown there).
  NSString* topLevelFolderTitle = @"Mobile Bookmarks";
  id<GREYMatcher> all_bookmarks_matcher =
      grey_allOf(grey_kindOfClass(NSClassFromString(@"BookmarkMenuCell")),
                 grey_descendant(grey_text(topLevelFolderTitle)), nil);
  [[EarlGrey selectElementWithMatcher:all_bookmarks_matcher]
      performAction:grey_tap()];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

  // Assert sign-in screen was shown.
  id<GREYMatcher> signin_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_DESCRIPTION);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open new tab to cancel sign-in.
  OpenUrlCommand* command =
      [[OpenUrlCommand alloc] initWithURLFromChrome:GURL("about:blank")];
  [chrome_test_util::DispatcherForActiveViewController() openURL:command];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kToolsMenuBookmarksId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      performAction:grey_tap()];
  if (!IsIPadIdiom()) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Menu")]
        performAction:grey_tap()];
  }
  [[EarlGrey selectElementWithMatcher:all_bookmarks_matcher]
      performAction:grey_tap()];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Bookmarks.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
  if (IsIPadIdiom()) {
    // Switch back to the Home Panel.  This is to prevent Bookmarks Panel, which
    // has an infinite spinner, from appearing in the coming tests and causing
    // timeouts.
    [chrome_test_util::GetCurrentNewTabPageController()
        selectPanel:ntp_home::HOME_PANEL];
    [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  } else {
    [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
        performAction:grey_tap()];
  }
}

@end
