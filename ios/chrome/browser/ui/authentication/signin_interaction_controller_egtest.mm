// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#import "ios/chrome/browser/ui/commands/open_url_command.h"
#import "ios/chrome/browser/ui/settings/accounts_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/import_data_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#import "ios/chrome/browser/ui/tools_menu/tools_popup_controller.h"
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

// Taps the view with acessibility identifier |accessibility_id|.
void TapViewWithAccessibilityId(NSString* accessiblity_id) {
  // grey_sufficientlyVisible() is necessary because reloading a cell in a
  // collection view might duplicate it (with the old one being hidden but
  // EarlGrey can find it).
  id<GREYMatcher> matcher = grey_allOf(grey_accessibilityID(accessiblity_id),
                                       grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
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

// Opens the signin screen from the settings page. Must be called from the NTP.
// User must not be signed in.
void OpenSignInFromSettings() {
  [ChromeEarlGreyUI openSettingsMenu];
  TapViewWithAccessibilityId(kSettingsSignInCellId);
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

  // Open the sign in screen.
  OpenSignInFromSettings();

  // Select the user.
  TapButtonWithAccessibilityLabel(identity.userEmail);

  // Sign in and confirm.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);

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

  // Sign in to |identity1|.
  OpenSignInFromSettings();
  TapButtonWithAccessibilityLabel(identity1.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity1);

  // Open accounts settings, then sync settings.
  TapViewWithAccessibilityId(kSettingsAccountCellId);
  TapViewWithAccessibilityId(kSettingsAccountsSyncCellId);

  // Switch Sync account to |identity2|.
  TapButtonWithAccessibilityLabel(identity2.userEmail);

  // Keep data separate, with synchronization off due to an infinite spinner.
  SetEarlGreySynchronizationEnabled(NO);
  WaitForMatcher(grey_accessibilityID(kImportDataKeepSeparateCellId));
  TapViewWithAccessibilityId(kImportDataKeepSeparateCellId);
  TapButtonWithLabelId(IDS_IOS_OPTIONS_IMPORT_DATA_CONTINUE_BUTTON);
  SetEarlGreySynchronizationEnabled(YES);

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
  OpenSignInFromSettings();
  TapButtonWithAccessibilityLabel(identity1.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity1);

  // Open accounts settings, then sync settings.
  TapViewWithAccessibilityId(kSettingsAccountCellId);
  TapViewWithAccessibilityId(kSettingsAccountsSyncCellId);

  // Switch Sync account to |identity2|.
  TapButtonWithAccessibilityLabel(identity2.userEmail);

  // Import data, with synchronization off due to an infinite spinner.
  SetEarlGreySynchronizationEnabled(NO);
  WaitForMatcher(grey_accessibilityID(kImportDataImportCellId));
  TapViewWithAccessibilityId(kImportDataImportCellId);
  TapButtonWithLabelId(IDS_IOS_OPTIONS_IMPORT_DATA_CONTINUE_BUTTON);
  SetEarlGreySynchronizationEnabled(YES);

  // Check the signed-in user did change.
  AssertAuthenticatedIdentityInActiveProfile(identity2);

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that switching from a managed account to a non-managed account works
// correctly and displays the expected warnings.
- (void)testSignInSwitchManagedAccount {
  if (!experimental_flags::IsMDMIntegrationEnabled()) {
    EARL_GREY_TEST_SKIPPED(@"Only enabled with MDM integration.");
  }

  // Set up the fake identities.
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* managed_identity = GetFakeManagedIdentity();
  ChromeIdentity* identity = GetFakeIdentity1();
  identity_service->AddIdentity(managed_identity);
  identity_service->AddIdentity(identity);

  // Sign in to |managed_identity|.
  OpenSignInFromSettings();
  TapButtonWithAccessibilityLabel(managed_identity.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);

  // Accept warning for signing into a managed identity, with synchronization
  // off due to an infinite spinner.
  SetEarlGreySynchronizationEnabled(NO);
  WaitForMatcher(chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON));
  TapButtonWithLabelId(IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON);
  SetEarlGreySynchronizationEnabled(YES);

  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(managed_identity);

  // Switch Sync account to |identity|.
  TapViewWithAccessibilityId(kSettingsAccountCellId);
  TapViewWithAccessibilityId(kSettingsAccountsSyncCellId);
  TapButtonWithAccessibilityLabel(identity.userEmail);

  // Accept warning for signout out of a managed identity, with synchronization
  // off due to an infinite spinner.
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
  OpenSignInFromSettings();
  TapButtonWithAccessibilityLabel(identity.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity);

  // Go to Accounts Settings and tap the sign out button.
  TapViewWithAccessibilityId(kSettingsAccountCellId);
  const CGFloat scroll_displacement = 100.0;
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kSettingsAccountsSignoutCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:grey_accessibilityID(kSettingsAccountsId)]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);

  // Check that the settings home screen is shown.
  WaitForMatcher(grey_accessibilityID(kSettingsSignInCellId));

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check that there is no signed in user.
  AssertAuthenticatedIdentityInActiveProfile(nil);
}

// Tests that signing out of a managed account from the Settings works
// correctly.
- (void)testSignInDisconnectFromChromeManaged {
  if (!experimental_flags::IsMDMIntegrationEnabled()) {
    EARL_GREY_TEST_SKIPPED(@"Only enabled with MDM integration.");
  }

  ChromeIdentity* identity = GetFakeManagedIdentity();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Sign in to |identity|.
  OpenSignInFromSettings();
  TapButtonWithAccessibilityLabel(identity.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  // Synchronization off due to an infinite spinner.
  SetEarlGreySynchronizationEnabled(NO);
  WaitForMatcher(chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON));
  TapButtonWithLabelId(IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON);
  SetEarlGreySynchronizationEnabled(YES);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity);

  // Go to Accounts Settings and tap the sign out button.
  TapViewWithAccessibilityId(kSettingsAccountCellId);
  const CGFloat scroll_displacement = 100.0;
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kSettingsAccountsSignoutCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:grey_accessibilityID(kSettingsAccountsId)]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_IOS_MANAGED_DISCONNECT_DIALOG_ACCEPT);

  // Check that the settings home screen is shown.
  WaitForMatcher(grey_accessibilityID(kSettingsSignInCellId));

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

  // Sign in to |identity|.
  OpenSignInFromSettings();
  TapButtonWithAccessibilityLabel(identity.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);

  // Tap Settings link.
  id<GREYMatcher> settings_link_matcher = grey_allOf(
      grey_accessibilityLabel(@"Settings"), grey_sufficientlyVisible(), nil);
  WaitForMatcher(settings_link_matcher);
  [[EarlGrey selectElementWithMatcher:settings_link_matcher]
      performAction:grey_tap()];

  id<GREYMatcher> done_button_matcher = NavigationBarDoneButton();
  WaitForMatcher(done_button_matcher);
  [[EarlGrey selectElementWithMatcher:done_button_matcher]
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

  OpenSignInFromSettings();

  // Open new tab to cancel sign-in.
  OpenUrlCommand* command =
      [[OpenUrlCommand alloc] initWithURLFromChrome:GURL("about:blank")];
  chrome_test_util::RunCommandWithActiveViewController(command);

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  OpenSignInFromSettings();
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

  OpenSignInFromSettings();

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
  chrome_test_util::RunCommandWithActiveViewController(command);

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  OpenSignInFromSettings();
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
  OpenSignInFromSettings();
  TapButtonWithAccessibilityLabel(identity2.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity2);

  // Go to Accounts Settings and tap the sign out button.
  TapViewWithAccessibilityId(kSettingsAccountCellId);
  const CGFloat scroll_displacement = 100.0;
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kSettingsAccountsSignoutCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:grey_accessibilityID(kSettingsAccountsId)]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
  AssertAuthenticatedIdentityInActiveProfile(nil);

  // Start sign-in with |identity1|.
  WaitForMatcher(grey_accessibilityID(kSettingsSignInCellId));
  TapViewWithAccessibilityId(kSettingsSignInCellId);
  TapButtonWithAccessibilityLabel(identity1.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);

  // Open new tab to cancel sign-in.
  OpenUrlCommand* command =
      [[OpenUrlCommand alloc] initWithURLFromChrome:GURL("about:blank")];
  chrome_test_util::RunCommandWithActiveViewController(command);

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  OpenSignInFromSettings();
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
- (void)testSignInCancelFromBookmarks {
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
      onElementWithMatcher:grey_accessibilityID(kToolsMenuTableViewId)]
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

  TapButtonWithLabelId(IDS_IOS_BOOKMARK_PROMO_SIGN_IN_BUTTON);

  // Assert sign-in screen was shown.
  id<GREYMatcher> signin_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_DESCRIPTION);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open new tab to cancel sign-in.
  OpenUrlCommand* command =
      [[OpenUrlCommand alloc] initWithURLFromChrome:GURL("about:blank")];
  chrome_test_util::RunCommandWithActiveViewController(command);

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kToolsMenuBookmarksId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:grey_accessibilityID(kToolsMenuTableViewId)]
      performAction:grey_tap()];
  if (!IsIPadIdiom()) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Menu")]
        performAction:grey_tap()];
  }
  [[EarlGrey selectElementWithMatcher:all_bookmarks_matcher]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_IOS_BOOKMARK_PROMO_SIGN_IN_BUTTON);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Bookmarks.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
  if (!IsIPadIdiom()) {
    [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
        performAction:grey_tap()];
  }
}

@end
