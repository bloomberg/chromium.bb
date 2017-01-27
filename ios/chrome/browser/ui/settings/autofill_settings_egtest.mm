// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/mac/bind_objc_block.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;

namespace {

// Expectation of how the saved autofill profile looks like, a map from cell
// name IDs to expected contents.
struct DisplayStringIDToExpectedResult {
  int display_string_id;
  NSString* expected_result;
};

const DisplayStringIDToExpectedResult kExpectedFields[] = {
    {IDS_IOS_AUTOFILL_FULLNAME, @"George Washington"},
    {IDS_IOS_AUTOFILL_COMPANY_NAME, @""},
    {IDS_IOS_AUTOFILL_ADDRESS1, @"1600 Pennsylvania Ave NW"},
    {IDS_IOS_AUTOFILL_ADDRESS2, @""},
    {IDS_IOS_AUTOFILL_CITY, @"Washington"},
    {IDS_IOS_AUTOFILL_STATE, @"DC"},
    {IDS_IOS_AUTOFILL_ZIP, @"20500"},
    {IDS_IOS_AUTOFILL_PHONE, @""},
    {IDS_IOS_AUTOFILL_EMAIL, @""}};

// Expectation of how user-typed country names should be canonicalized.
struct UserTypedCountryExpectedResultPair {
  NSString* user_typed_country;
  NSString* expected_result;
};

const UserTypedCountryExpectedResultPair kCountryTests[] = {
    {@"Brasil", @"Brazil"},
    {@"China", @"China"},
    {@"DEUTSCHLAND", @"Germany"},
    {@"GREAT BRITAIN", @"United Kingdom"},
    {@"IN", @"India"},
    {@"JaPaN", @"Japan"},
    {@"JP", @"Japan"},
    {@"Nigeria", @"Nigeria"},
    {@"TW", @"Taiwan"},
    {@"U.S.A.", @"United States"},
    {@"UK", @"United Kingdom"},
    {@"USA", @"United States"},
    {@"Nonexistia", @""},
};

// Given a resource ID of a category of an address profile, it returns a
// NSString consisting of the resource string concatenated with "_textField".
// This is the a11y ID of the text field corresponding to the category in the
// edit dialog of the address profile.
NSString* GetTextFieldForID(int categoryId) {
  return [NSString
      stringWithFormat:@"%@_textField", l10n_util::GetNSString(categoryId)];
}

// Call this in the "Edit Address" view to clear the Country value. The case
// of the value being empty is handled gracefully.
void ClearCountryValue() {
  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      performAction:grey_tap()];

  // The test only can tap "Select All" if there is a text to select. Type "a"
  // to ensure that.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(GetTextFieldForID(
                                          IDS_IOS_AUTOFILL_COUNTRY))]
      performAction:grey_typeText(@"a")];
  // Remove the country value by select all + cut.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(GetTextFieldForID(
                                          IDS_IOS_AUTOFILL_COUNTRY))]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"Select All"),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitStaticText),
                                   nil)] performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(@"Cut"),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitStaticText),
                                          nil)] performAction:grey_tap()];

  // Switch off edit mode.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)]
      performAction:grey_tap()];
}

}  // namespace

// Various tests for the Autofill section of the settings.
@interface AutofillSettingsTestCase : ChromeTestCase
@end

@implementation AutofillSettingsTestCase

// Helper to load a page with an address form and submit it.
- (void)loadAndSubmitTheForm {
  web::test::SetUpFileBasedHttpServer();
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/autofill_smoke_test.html");

  [ChromeEarlGrey loadURL:URL];

  // Autofill one of the forms.
  chrome_test_util::TapWebViewElementWithId("fill_profile_president");
  chrome_test_util::TapWebViewElementWithId("submit_profile");
}

// Helper to open the settings page for the record with |address|.
- (void)openEditAddress:(NSString*)address {
  // Open settings and verify data in the view controller.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kToolsMenuSettingsId)]
      performAction:grey_tap()];
  NSString* label = l10n_util::GetNSString(IDS_IOS_AUTOFILL);
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(label)]
      performAction:grey_tap()];

  // Tap on the 'George Washington' result.
  NSString* cellLabel = address;
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(cellLabel)]
      performAction:grey_tap()];
}

// Close the settings.
- (void)exitSettingsMenu {
  NSString* backButtonA11yId = @"ic_arrow_back";
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(backButtonA11yId),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(backButtonA11yId),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)]
      performAction:grey_tap()];
  // Wait for UI components to finish loading.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Test that submitting a form ensures saving the data as an autofill profile.
- (void)testAutofillProfileSaving {
  [self loadAndSubmitTheForm];
  [self openEditAddress:@"George Washington, 1600 Pennsylvania Ave NW"];

  // Check that all fields and values match the expectations.
  for (const DisplayStringIDToExpectedResult& expectation : kExpectedFields) {
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityLabel([NSString
                stringWithFormat:@"%@, %@", l10n_util::GetNSString(
                                                expectation.display_string_id),
                                 expectation.expected_result])]
        assertWithMatcher:grey_notNil()];
  }

  [self exitSettingsMenu];
}

// Test that editing country names is followed by validating the value and
// replacing it with a canonical one.
- (void)testAutofillProfileEditing {
  [self loadAndSubmitTheForm];
  [self openEditAddress:@"George Washington, 1600 Pennsylvania Ave NW"];

  // Keep editing the Country field and verify that validation works.
  for (const UserTypedCountryExpectedResultPair& expectation : kCountryTests) {
    ClearCountryValue();

    // Switch on edit mode.
    [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                            IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
        performAction:grey_tap()];

    // Type the user-version of the country.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(GetTextFieldForID(
                                            IDS_IOS_AUTOFILL_COUNTRY))]
        performAction:grey_typeText(expectation.user_typed_country)];

    // Switch off edit mode.
    [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                            IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)]
        performAction:grey_tap()];

    // Verify that the country value was changed to canonical.
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityLabel([NSString
                stringWithFormat:@"%@, %@", l10n_util::GetNSString(
                                                IDS_IOS_AUTOFILL_COUNTRY),
                                 expectation.expected_result])]
        assertWithMatcher:grey_notNil()];
  }

  [self exitSettingsMenu];
}

// Test that the page for viewing autofill profile details is accessible.
- (void)testAccessibilityOnAutofillProfileViewPage {
  [self loadAndSubmitTheForm];
  [self openEditAddress:@"George Washington, 1600 Pennsylvania Ave NW"];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  [self exitSettingsMenu];
}

// Test that the page for editing autofill profile details is accessible.
- (void)testAccessibilityOnAutofillProfileEditPage {
  [self loadAndSubmitTheForm];
  [self openEditAddress:@"George Washington, 1600 Pennsylvania Ave NW"];
  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  [self exitSettingsMenu];
}

@end
