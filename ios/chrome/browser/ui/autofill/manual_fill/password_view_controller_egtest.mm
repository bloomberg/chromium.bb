// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "base/test/ios/wait_util.h"
#include "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"
#import "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kFormHTMLFile[] = "/username_password_field_form.html";
const char kFormElementUsername[] = "username";

// Returns a matcher for the password icon in the keyboard accessory bar.
id<GREYMatcher> PasswordIconMatcher() {
  return grey_accessibilityID(
      manual_fill::AccessoryPasswordAccessibilityIdentifier);
}

// Returns a matcher for the password table view in manual fallback.
id<GREYMatcher> PasswordTableViewMatcher() {
  return grey_accessibilityID(
      manual_fill::PasswordTableViewAccessibilityIdentifier);
}

// Returns a matcher for the button to open password settings in manual
// fallback.
id<GREYMatcher> ManagePasswordsMatcher() {
  return grey_accessibilityID(
      manual_fill::ManagePasswordsAccessibilityIdentifier);
}

// Returns a matcher for the password settings collection view.
id<GREYMatcher> PasswordSettingsMatcher() {
  return grey_accessibilityID(@"SavePasswordsCollectionViewController");
}

}  // namespace

// Integration Tests for Mannual Fallback Passwords View Controller.
@interface PasswordViewControllerTestCase : ChromeTestCase
@end

@implementation PasswordViewControllerTestCase

- (void)setUp {
  [super setUp];
  GREYAssert(autofill::features::IsPasswordManualFallbackEnabled(),
             @"Manual Fallback must be enabled for this Test Case");
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebViewContainingText:"hello!"];
}

// Test that the passwords view controller appears on screen.
- (void)testPasswordsViewControllerIsPresented {
  // TODO:(https://crbug.com/878388) Enable on iPad when supported.
  if (IsIPadIdiom())
    return;

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Test that the passwords view controller contains the "Manage Passwords..."
// action.
- (void)testPasswordsViewControllerContainsManagePasswordsAction {
  // TODO:(https://crbug.com/878388) Enable on iPad when supported.
  if (IsIPadIdiom())
    return;

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller contains the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManagePasswordsMatcher()]
      assertWithMatcher:grey_interactable()];
}

// Test that the "Manage Passwords..." action works.
- (void)testManagePasswordsActionOpensPasswordSettings {
  // TODO:(https://crbug.com/878388) Enable on iPad when supported.
  if (IsIPadIdiom())
    return;

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManagePasswordsMatcher()]
      performAction:grey_tap()];

  // Verify the password settings opened.
  [[EarlGrey selectElementWithMatcher:PasswordSettingsMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
