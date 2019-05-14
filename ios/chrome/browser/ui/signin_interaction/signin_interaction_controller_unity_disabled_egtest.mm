// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/auto_reset.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/unified_consent/feature.h"
#import "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_controller_egtest_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_error_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsDoneButton;

// Sign-in interaction tests that requires Unified Consent to be disabled.
@interface SigninInteractionControllerUnityDisabledTestCase : ChromeTestCase
@end

@implementation SigninInteractionControllerUnityDisabledTestCase

- (void)setUp {
  [super setUp];

  CHECK(!unified_consent::IsUnifiedConsentFeatureEnabled())
      << "This test suite must be run with Unified Consent feature disabled.";
}

// Tests that switching from a managed account to a non-managed account works
// correctly and displays the expected warnings.
- (void)testSignInSwitchManagedAccount {
  // Set up the fake identities.
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* managed_identity = [SigninEarlGreyUtils fakeManagedIdentity];
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  identity_service->AddIdentity(managed_identity);
  identity_service->AddIdentity(identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [SigninEarlGreyUI selectIdentityWithEmail:managed_identity.userEmail];

  // Accept warning for signing into a managed identity, with synchronization
  // off due to an infinite spinner.
  SetEarlGreySynchronizationEnabled(NO);
  WaitForMatcher(chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON));
  TapButtonWithLabelId(IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON);
  SetEarlGreySynchronizationEnabled(YES);

  [SigninEarlGreyUI confirmSigninConfirmationDialog];
  CHROME_EG_ASSERT_NO_ERROR(
      [SigninEarlGreyUtils checkSignedInWithIdentity:managed_identity]);

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

  CHROME_EG_ASSERT_NO_ERROR(
      [SigninEarlGreyUtils checkSignedInWithIdentity:identity]);

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests signing in with one account, switching sync account to a second and
// choosing to keep the browsing data separate during the switch.
- (void)testSignInSwitchAccountsAndKeepDataSeparate {
  // Set up the fake identities.
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* identity1 = [SigninEarlGreyUtils fakeIdentity1];
  ChromeIdentity* identity2 = [SigninEarlGreyUtils fakeIdentity2];
  identity_service->AddIdentity(identity1);
  identity_service->AddIdentity(identity2);

  [SigninEarlGreyUI signinWithIdentity:identity1];
  [ChromeEarlGreyUI openSettingsMenu];

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
  id<GREYMatcher> matcher = grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_OPTIONS_IMPORT_DATA_CONTINUE_BUTTON),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];

  // Check the signed-in user did change.
  CHROME_EG_ASSERT_NO_ERROR(
      [SigninEarlGreyUtils checkSignedInWithIdentity:identity2]);

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests signing in with one account, switching sync account to a second and
// choosing to import the browsing data during the switch.
- (void)testSignInSwitchAccountsAndImportData {
  // Set up the fake identities.
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* identity1 = [SigninEarlGreyUtils fakeIdentity1];
  ChromeIdentity* identity2 = [SigninEarlGreyUtils fakeIdentity2];
  identity_service->AddIdentity(identity1);
  identity_service->AddIdentity(identity2);

  // Sign in to |identity1|.
  [SigninEarlGreyUI signinWithIdentity:identity1];
  [ChromeEarlGreyUI openSettingsMenu];

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
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                                IDS_IOS_OPTIONS_IMPORT_DATA_CONTINUE_BUTTON),
                            grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];

  // Check the signed-in user did change.
  CHROME_EG_ASSERT_NO_ERROR(
      [SigninEarlGreyUtils checkSignedInWithIdentity:identity2]);

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Opens the add account screen and then cancels it by opening a new tab.
// Ensures that the add account screen is correctly dismissed. crbug.com/462200
- (void)testSignInCancelAddAccount {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

  // Open Add Account screen.
  id<GREYMatcher> add_account_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          unified_consent::IsUnifiedConsentFeatureEnabled()
              ? IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_ADD_ACCOUNT
              : IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_ADD_ACCOUNT_BUTTON);
  [[EarlGrey selectElementWithMatcher:add_account_matcher]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Open new tab to cancel sign-in.
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL("about:blank")];
  [chrome_test_util::DispatcherForActiveBrowserViewController()
      openURLInNewTab:command];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  if (unified_consent::IsUnifiedConsentFeatureEnabled())
    [SigninEarlGreyUI selectIdentityWithEmail:identity.userEmail];
  VerifyChromeSigninViewVisible();

  // Close sign-in screen and Settings.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

@end
