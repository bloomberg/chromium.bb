// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#include "components/unified_consent/feature.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_cell.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_error_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/testing/earl_grey/matchers.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BookmarksNavigationBarDoneButton;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsDoneButton;

namespace {

// Wait until |matcher| is accessible (not nil)
void WaitForMatcher(id<GREYMatcher> matcher) {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()
                                                             error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Waiting for matcher %@ failed.", matcher);
}

// Returns a matcher for |userEmail| in IdentityChooserViewController.
id<GREYMatcher> identityChooserButtonMatcherWithEmail(NSString* userEmail) {
  if (base::FeatureList::IsEnabled(unified_consent::kUnifiedConsent)) {
    return grey_allOf(grey_accessibilityID(userEmail),
                      grey_kindOfClass([IdentityChooserCell class]),
                      grey_sufficientlyVisible(), nil);
  }
  return chrome_test_util::ButtonWithAccessibilityLabel(userEmail);
}

// Returns a matcher for "ADD ACCOUNT" in IdentityChooserViewController.
id<GREYMatcher> addIdentityButtonInIdentityChooser() {
  DCHECK(base::FeatureList::IsEnabled(unified_consent::kUnifiedConsent));
  return chrome_test_util::ButtonWithAccessibilityLabel(
      l10n_util::GetNSStringWithFixup(
          IDS_IOS_ACCOUNT_UNIFIED_CONSENT_ADD_ACCOUNT));
}

}  // namespace

@interface SigninInteractionControllerTestCase : ChromeTestCase
@end

@implementation SigninInteractionControllerTestCase

// Tests that opening the sign-in screen from the Settings and signing in works
// correctly when there is already an identity on the device.
- (void)testSignInOneUser {
  // Set up a fake identity.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [SigninEarlGreyUI signinWithIdentity:identity];

  // Check |identity| is signed-in.
  NSError* signedInError =
      [SigninEarlGreyUtils checkSignedInWithIdentity:identity];
  GREYAssertNil(signedInError, signedInError.localizedDescription);
}

// Tests the "ADD ACCOUNT" button in the identity chooser view controller.
- (void)testAddAccountAutomatcially {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  // Tap on "ADD ACCOUNT".
  [[EarlGrey selectElementWithMatcher:addIdentityButtonInIdentityChooser()]
      performAction:grey_tap()];
  // Check for the fake SSO screen.
  WaitForMatcher(grey_accessibilityID(kFakeAddAccountViewIdentifier));
  // Close the SSO view controller.
  id<GREYMatcher> matcher =
      chrome_test_util::ButtonWithAccessibilityLabel(@"Cancel");
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  // Make sure the SSO view controller is fully removed before ending the test.
  // The tear down needs to remove other view controllers, and it cannot be done
  // during the animation of the SSO view controler.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Tests to remove the last identity in the identity chooser.
- (void)testRemoveLastAccount {
  // Set up a fake identity.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Open the identity chooser.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  WaitForMatcher(identityChooserButtonMatcherWithEmail(identity.userEmail));

  // Remove the fake identity.
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->RemoveIdentity(identity);
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Check that the identity has been removed.
  [[EarlGrey selectElementWithMatcher:identityChooserButtonMatcherWithEmail(
                                          identity.userEmail)]
      assertWithMatcher:grey_notVisible()];
}

@end
