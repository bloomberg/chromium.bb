// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#import "ios/chrome/browser/ui/settings/accounts_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#import "ios/chrome/browser/ui/tools_menu/tools_popup_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"

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

// Opens the signin screen from the settings page. Assumes the tools menu is
// accessible. User must not be signed in.
void OpenSignInFromSettings() {
  const CGFloat scroll_displacement = 50.0;

  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kToolsMenuSettingsId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:grey_accessibilityID(kToolsMenuTableViewId)]
      performAction:grey_tap()];

  TapViewWithAccessibilityId(kSettingsSignInCellId);
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
                  @"Unexpected Gaia ID of the signed in user [expected = "
                  @"\"%@\", actual = \"%s\"]",
                  identity.gaiaID, info.gaia.c_str());
}
}

// Integration tests using the Account Settings screen.
@interface AccountCollectionsTestCase : ChromeTestCase
@end

@implementation AccountCollectionsTestCase

// Tests that the Sync and Account Settings screen are correctly popped if the
// signed in account is removed.
- (void)testSignInPopUpAccountOnSyncSettings {
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Sign In |identity|, then open the Sync Settings.
  OpenSignInFromSettings();
  TapButtonWithAccessibilityLabel(identity.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity);
  TapViewWithAccessibilityId(kSettingsAccountCellId);
  TapViewWithAccessibilityId(kSettingsAccountsSyncCellId);

  // Forget |identity|, screens should be popped back to the Main Settings.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->ForgetIdentity(identity, nil);

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsSignInCellId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  AssertAuthenticatedIdentityInActiveProfile(nil);

  // Close Settings.
  TapButtonWithLabelId(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
}

// Tests that the Account Settings screen is correctly popped if the signed in
// account is removed while the "Disconnect Account" dialog is up.
- (void)testSignInPopUpAccountOnDisconnectAccount {
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Sign In |identity|, then open the Account Settings.
  OpenSignInFromSettings();
  TapButtonWithAccessibilityLabel(identity.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity);
  TapViewWithAccessibilityId(kSettingsAccountCellId);

  // Open the "Disconnect Account" dialog.
  const CGFloat scroll_displacement = 100.0;
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kSettingsAccountsSignoutCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:grey_accessibilityID(kSettingsAccountsId)]
      performAction:grey_tap()];

  // Forget |identity|, screens should be popped back to the Main Settings.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->ForgetIdentity(identity, nil);

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsSignInCellId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  AssertAuthenticatedIdentityInActiveProfile(nil);

  // Close Settings.
  TapButtonWithLabelId(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
}

// Tests that the Account Settings screen is correctly reloaded when one of
// the non-primary account is removed.
- (void)testSignInReloadOnRemoveAccount {
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* identity1 = GetFakeIdentity1();
  ChromeIdentity* identity2 = GetFakeIdentity2();
  identity_service->AddIdentity(identity1);
  identity_service->AddIdentity(identity2);

  // Sign In |identity|, then open the Account Settings.
  OpenSignInFromSettings();
  TapButtonWithAccessibilityLabel(identity1.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity1);
  TapViewWithAccessibilityId(kSettingsAccountCellId);

  // Remove |identity2| from the device.
  TapButtonWithAccessibilityLabel(identity2.userEmail);
  TapButtonWithAccessibilityLabel(@"Remove account");

  // Check that |identity2| isn't available anymore on the Account Settings.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(identity2.userEmail),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  AssertAuthenticatedIdentityInActiveProfile(identity1);

  // Close Settings.
  TapButtonWithLabelId(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
}

// Tests that the Sync Settings screen is correctly reloaded when one of the
// secondary accounts disappears.
- (void)testSignInReloadSyncOnForgetIdentity {
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* identity1 = GetFakeIdentity1();
  ChromeIdentity* identity2 = GetFakeIdentity2();
  identity_service->AddIdentity(identity1);
  identity_service->AddIdentity(identity2);

  // Sign In |identity|, then open the Sync Settings.
  OpenSignInFromSettings();
  TapButtonWithAccessibilityLabel(identity1.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity1);
  TapViewWithAccessibilityId(kSettingsAccountCellId);
  TapViewWithAccessibilityId(kSettingsAccountsSyncCellId);

  // Forget |identity2|.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  identity_service->ForgetIdentity(identity2, nil);

  // Check that both |identity1| and |identity2| aren't shown in the Sync
  // Settings.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(identity1.userEmail),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(identity2.userEmail),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  AssertAuthenticatedIdentityInActiveProfile(identity1);

  // Close Settings.
  TapButtonWithLabelId(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
}

// Tests that the Account Settings screen is popped and the user signed out
// when the account is removed.
- (void)testSignOutOnRemoveAccount {
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Sign In |identity|, then open the Account Settings.
  OpenSignInFromSettings();
  TapButtonWithAccessibilityLabel(identity.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity);
  TapViewWithAccessibilityId(kSettingsAccountCellId);

  // Remove |identity| from the device.
  TapButtonWithAccessibilityLabel(identity.userEmail);
  TapButtonWithAccessibilityLabel(@"Remove account");

  // Check that the user is signed out and the Main Settings screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsSignInCellId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  AssertAuthenticatedIdentityInActiveProfile(nil);

  // Close Settings.
  TapButtonWithLabelId(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
}

// Tests that the user isn't signed out and the UI is correct when the
// disconnect is cancelled in the Account Settings screen.
- (void)testSignInDisconnectCancelled {
// TODO(crbug.com/669613): Re-enable this test on devices.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Test disabled on device.");
#endif
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Sign In |identity|, then open the Account Settings.
  OpenSignInFromSettings();
  TapButtonWithAccessibilityLabel(identity.userEmail);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity);
  TapViewWithAccessibilityId(kSettingsAccountCellId);

  // Open the "Disconnect Account" dialog, then tap "Cancel".
  const CGFloat scroll_displacement = 100.0;
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kSettingsAccountsSignoutCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:grey_accessibilityID(kSettingsAccountsId)]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_CANCEL);

  // Check that Account Settings screen is open and |identity| is signed in.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kSettingsAccountsId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  AssertAuthenticatedIdentityInActiveProfile(identity);

  // Close Settings.
  TapButtonWithLabelId(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
}

@end
