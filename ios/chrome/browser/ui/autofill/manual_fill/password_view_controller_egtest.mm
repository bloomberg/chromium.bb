// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <EarlGrey/GREYKeyboard.h>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
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

const char kFormElementUsername[] = "username";

const char kExampleUsername[] = "concrete username";
const char kExamplePassword[] = "concrete password";

const char kFormHTMLFile[] = "/username_password_field_form.html";

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

// Returns a matcher for the password search bar in manual fallback.
id<GREYMatcher> PasswordSearchBarMatcher() {
  return grey_accessibilityID(
      manual_fill::PasswordSearchBarAccessibilityIdentifier);
}

// Returns a matcher for the button to open password settings in manual
// fallback.
id<GREYMatcher> ManagePasswordsMatcher() {
  return grey_accessibilityID(
      manual_fill::ManagePasswordsAccessibilityIdentifier);
}

// Returns a matcher for the button to open all passwords in manual fallback.
id<GREYMatcher> OtherPasswordsMatcher() {
  return grey_accessibilityID(
      manual_fill::OtherPasswordsAccessibilityIdentifier);
}

// Returns a matcher for the example username in the list.
id<GREYMatcher> UsernameButtonMatcher() {
  return grey_buttonTitle(base::SysUTF8ToNSString(kExampleUsername));
}

// Returns a matcher for the password settings collection view.
id<GREYMatcher> PasswordSettingsMatcher() {
  return grey_accessibilityID(@"SavePasswordsCollectionViewController");
}

// Returns a matcher for the search bar in password settings.
id<GREYMatcher> PasswordSettingsSearchMatcher() {
  return grey_accessibilityID(@"SettingsSearchCellTextField");
}

// Gets the current password store.
scoped_refptr<password_manager::PasswordStore> GetPasswordStore() {
  // ServiceAccessType governs behaviour in Incognito: only modifications with
  // EXPLICIT_ACCESS, which correspond to user's explicit gesture, succeed.
  // This test does not deal with Incognito, and should not run in Incognito
  // context. Therefore IMPLICIT_ACCESS is used to let the test fail if in
  // Incognito context.
  return IOSChromePasswordStoreFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState(),
      ServiceAccessType::IMPLICIT_ACCESS);
}

// This class is used to obtain results from the PasswordStore and hence both
// check the success of store updates and ensure that store has finished
// processing.
class TestStoreConsumer : public password_manager::PasswordStoreConsumer {
 public:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> obtained) override {
    obtained_ = std::move(obtained);
  }

  const std::vector<autofill::PasswordForm>& GetStoreResults() {
    results_.clear();
    ResetObtained();
    GetPasswordStore()->GetAutofillableLogins(this);
    bool responded = base::test::ios::WaitUntilConditionOrTimeout(1.0, ^bool {
      return !AreObtainedReset();
    });
    GREYAssert(responded, @"Obtaining fillable items took too long.");
    AppendObtainedToResults();
    GetPasswordStore()->GetBlacklistLogins(this);
    responded = base::test::ios::WaitUntilConditionOrTimeout(1.0, ^bool {
      return !AreObtainedReset();
    });
    GREYAssert(responded, @"Obtaining blacklisted items took too long.");
    AppendObtainedToResults();
    return results_;
  }

 private:
  // Puts |obtained_| in a known state not corresponding to any PasswordStore
  // state.
  void ResetObtained() {
    obtained_.clear();
    obtained_.emplace_back(nullptr);
  }

  // Returns true if |obtained_| are in the reset state.
  bool AreObtainedReset() { return obtained_.size() == 1 && !obtained_[0]; }

  void AppendObtainedToResults() {
    for (const auto& source : obtained_) {
      results_.emplace_back(*source);
    }
    ResetObtained();
  }

  // Temporary cache of obtained store results.
  std::vector<std::unique_ptr<autofill::PasswordForm>> obtained_;

  // Combination of fillable and blacklisted credentials from the store.
  std::vector<autofill::PasswordForm> results_;
};

// Saves |form| to the password store and waits until the async processing is
// done.
void SaveToPasswordStore(const autofill::PasswordForm& form) {
  GetPasswordStore()->AddLogin(form);
  // Check the result and ensure PasswordStore processed this.
  TestStoreConsumer consumer;
  for (const auto& result : consumer.GetStoreResults()) {
    if (result == form)
      return;
  }
  GREYFail(@"Stored form was not found in the PasswordStore results.");
}

// Saves an example form in the store.
void SaveExamplePasswordForm() {
  autofill::PasswordForm example;
  example.username_value = base::ASCIIToUTF16(kExampleUsername);
  example.password_value = base::ASCIIToUTF16(kExamplePassword);
  example.origin = GURL("https://example.com");
  example.signon_realm = example.origin.spec();
  SaveToPasswordStore(example);
}

// Removes all credentials stored.
void ClearPasswordStore() {
  GetPasswordStore()->RemoveLoginsCreatedBetween(base::Time(), base::Time(),
                                                 base::Closure());
  TestStoreConsumer consumer;
  GREYAssert(consumer.GetStoreResults().empty(),
             @"PasswordStore was not cleared.");
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

- (void)tearDown {
  ClearPasswordStore();
  [super tearDown];
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

// Test that the Password View Controller is not present when presenting UI.
- (void)testPasswordControllerPauses {
  // TODO:(https://crbug.com/878388) Enable on iPad when supported.
  if (IsIPadIdiom())
    return;

  // For the search bar to appear in password settings at least one password is
  // needed.
  SaveExamplePasswordForm();

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:ManagePasswordsMatcher()]
      performAction:grey_tap()];

  // Tap the password search.

  [[EarlGrey selectElementWithMatcher:PasswordSettingsSearchMatcher()]
      performAction:grey_tap()];

  // Verify keyboard is shown without the password controller.
  GREYAssertTrue([GREYKeyboard isKeyboardShown], @"Keyboard Should be Shown");
  [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Test that the Password View Controller is resumed after selecting other
// password.
- (void)testPasswordControllerResumes {
  // TODO:(https://crbug.com/878388) Enable on iPad when supported.
  if (IsIPadIdiom())
    return;

  // For this test one password is needed.
  SaveExamplePasswordForm();

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElement(kFormElementUsername)];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:PasswordIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Passwords..." action.
  [[EarlGrey selectElementWithMatcher:OtherPasswordsMatcher()]
      performAction:grey_tap()];

  // Tap the password search.
  [[EarlGrey selectElementWithMatcher:PasswordSearchBarMatcher()]
      performAction:grey_tap()];
  GREYAssertTrue([GREYKeyboard isKeyboardShown], @"Keyboard Should be Shown");

  // Select a username.
  [[EarlGrey selectElementWithMatcher:UsernameButtonMatcher()]
      performAction:grey_tap()];

  // Only on iOS 12 it is certain that on iPhones the keyboard is back. On iOS
  // 11, it varies by device and version.
  if (base::ios::IsRunningOnIOS12OrLater()) {
    // Verify the password controller table view and the keyboard are visible.
    GREYAssertTrue([GREYKeyboard isKeyboardShown], @"Keyboard Should be Shown");
    [[EarlGrey selectElementWithMatcher:PasswordTableViewMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()];
  } else if (!base::ios::IsRunningOnIOS11OrLater()) {
    // On iOS 10 the keyboard is hidden.
    GREYAssertFalse([GREYKeyboard isKeyboardShown],
                    @"Keyboard Should be Hidden");
  }
}

@end
