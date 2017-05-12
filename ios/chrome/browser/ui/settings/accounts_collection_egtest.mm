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
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#import "ios/chrome/browser/ui/tools_menu/tools_popup_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::AccountConsistencyConfirmationOkButton;
using chrome_test_util::AccountConsistencySetupSigninButton;
using chrome_test_util::AccountsSyncButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SignOutAccountsButton;
using chrome_test_util::SignInMenuButton;

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

// Accepts account consistency popup prompts after signing in an account via the
// UI. Should be called right after signing in an account.
void AcceptAccountConsistencyPopup() {
  [[EarlGrey selectElementWithMatcher:AccountConsistencySetupSigninButton()]
      performAction:grey_tap()];

  // The "MORE" button shows up on screens where all content can't be shown, and
  // must be tapped to bring up the confirmation button.
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"MORE")]
      assertWithMatcher:grey_notNil()
                  error:&error];
  if (error == nil) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"MORE")]
        performAction:grey_tap()];
  }

  [[EarlGrey selectElementWithMatcher:AccountConsistencyConfirmationOkButton()]
      performAction:grey_tap()];
}

// Returns a matcher for a button that matches the userEmail in the given
// |identity|.
id<GREYMatcher> ButtonWithIdentity(ChromeIdentity* identity) {
  return ButtonWithAccessibilityLabel(identity.userEmail);
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
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SignInMenuButton()];
  [[EarlGrey selectElementWithMatcher:ButtonWithIdentity(identity)]
      performAction:grey_tap()];
  AcceptAccountConsistencyPopup();
  AssertAuthenticatedIdentityInActiveProfile(identity);
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [ChromeEarlGreyUI tapAccountsMenuButton:AccountsSyncButton()];

  // Forget |identity|, screens should be popped back to the Main Settings.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->ForgetIdentity(identity, nil);

  [[EarlGrey selectElementWithMatcher:SignInMenuButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  AssertAuthenticatedIdentityInActiveProfile(nil);

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is correctly popped if the signed in
// account is removed while the "Disconnect Account" dialog is up.
- (void)testSignInPopUpAccountOnDisconnectAccount {
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Sign In |identity|, then open the Account Settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SignInMenuButton()];
  [[EarlGrey selectElementWithMatcher:ButtonWithIdentity(identity)]
      performAction:grey_tap()];
  AcceptAccountConsistencyPopup();
  AssertAuthenticatedIdentityInActiveProfile(identity);
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];

  // Forget |identity|, screens should be popped back to the Main Settings.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->ForgetIdentity(identity, nil);

  [[EarlGrey selectElementWithMatcher:SignInMenuButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  AssertAuthenticatedIdentityInActiveProfile(nil);

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
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
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SignInMenuButton()];
  [[EarlGrey selectElementWithMatcher:ButtonWithIdentity(identity1)]
      performAction:grey_tap()];
  AcceptAccountConsistencyPopup();
  AssertAuthenticatedIdentityInActiveProfile(identity1);
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Remove |identity2| from the device.
  [[EarlGrey selectElementWithMatcher:ButtonWithIdentity(identity2)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Remove account")]
      performAction:grey_tap()];

  // Check that |identity2| isn't available anymore on the Account Settings.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(identity2.userEmail),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  AssertAuthenticatedIdentityInActiveProfile(identity1);

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
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
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SignInMenuButton()];
  [[EarlGrey selectElementWithMatcher:ButtonWithIdentity(identity1)]
      performAction:grey_tap()];
  AcceptAccountConsistencyPopup();
  AssertAuthenticatedIdentityInActiveProfile(identity1);
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [ChromeEarlGreyUI tapAccountsMenuButton:AccountsSyncButton()];

  // Forget |identity2|, allowing the UI to synchronize before and after
  // forgetting the identity.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  identity_service->ForgetIdentity(identity2, nil);
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

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

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is popped and the user signed out
// when the account is removed.
- (void)testSignOutOnRemoveAccount {
  ChromeIdentity* identity = GetFakeIdentity1();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Sign In |identity|, then open the Account Settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SignInMenuButton()];
  [[EarlGrey selectElementWithMatcher:ButtonWithIdentity(identity)]
      performAction:grey_tap()];
  AcceptAccountConsistencyPopup();
  AssertAuthenticatedIdentityInActiveProfile(identity);
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Remove |identity| from the device.
  [[EarlGrey selectElementWithMatcher:ButtonWithIdentity(identity)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Remove account")]
      performAction:grey_tap()];

  // Check that the user is signed out and the Main Settings screen is shown.
  [[EarlGrey selectElementWithMatcher:SignInMenuButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  AssertAuthenticatedIdentityInActiveProfile(nil);

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
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
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SignInMenuButton()];
  [[EarlGrey selectElementWithMatcher:ButtonWithIdentity(identity)]
      performAction:grey_tap()];
  AcceptAccountConsistencyPopup();
  AssertAuthenticatedIdentityInActiveProfile(identity);
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Open the "Disconnect Account" dialog, then tap "Cancel".
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];

  // Check that Account Settings screen is open and |identity| is signed in.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kSettingsAccountsId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  AssertAuthenticatedIdentityInActiveProfile(identity);

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

@end
