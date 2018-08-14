// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/ui/tools_menu/public/tools_menu_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/web_view_interaction_test_util.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;

namespace {

// Expectation of how the saved autofill profile looks like, a map from cell
// name IDs to expected contents.
struct DisplayStringIDToExpectedResult {
  int display_string_id;
  NSString* expected_result;
};

const DisplayStringIDToExpectedResult kExpectedFields[] = {
    {IDS_IOS_AUTOFILL_FULLNAME, @"John H. Doe"},
    {IDS_IOS_AUTOFILL_COMPANY_NAME, @"Underworld"},
    {IDS_IOS_AUTOFILL_ADDRESS1, @"666 Erebus St."},
    {IDS_IOS_AUTOFILL_ADDRESS2, @"Apt 8"},
    {IDS_IOS_AUTOFILL_CITY, @"Elysium"},
    {IDS_IOS_AUTOFILL_STATE, @"CA"},
    {IDS_IOS_AUTOFILL_ZIP, @"91111"},
    {IDS_IOS_AUTOFILL_PHONE, @"16502111111"},
    {IDS_IOS_AUTOFILL_EMAIL, @"johndoe@hades.com"}};

NSString* const kAddressLabel = @"John H. Doe, 666 Erebus St.";

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

}  // namespace

// Various tests for the Autofill section of the settings.
@interface AutofillSettingsTestCase : ChromeTestCase
@end

@implementation AutofillSettingsTestCase {
  // The PersonalDataManager instance for the current browser state.
  autofill::PersonalDataManager* _personalDataManager;
}

- (void)setUp {
  [super setUp];

  _personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  _personalDataManager->SetSyncingForTest(true);
}

- (void)tearDown {
  // Clear existing profiles and credit cards.
  for (const auto* profile : _personalDataManager->GetProfiles()) {
    _personalDataManager->RemoveByGUID(profile->guid());
  }
  for (const auto* creditCard : _personalDataManager->GetCreditCards()) {
    _personalDataManager->RemoveByGUID(creditCard->guid());
  }

  [super tearDown];
}

- (autofill::AutofillProfile)addAutofillProfile {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  size_t profileCount = _personalDataManager->GetProfiles().size();
  _personalDataManager->AddProfile(profile);
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout,
                 ^bool() {
                   return profileCount <
                          _personalDataManager->GetProfiles().size();
                 }),
             @"Failed to add profile.");
  return profile;
}

- (autofill::CreditCard)addCreditCard {
  autofill::CreditCard creditCard = autofill::test::GetCreditCard();  // Visa.
  size_t cardCount = _personalDataManager->GetCreditCards().size();
  _personalDataManager->AddCreditCard(creditCard);
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout,
                 ^bool() {
                   return cardCount <
                          _personalDataManager->GetCreditCards().size();
                 }),
             @"Failed to add credit card.");
  return creditCard;
}

// Helper to open the settings page for the record with |address|.
- (void)openEditAddress:(NSString*)address {
  // Go to Autofill Settings.
  [ChromeEarlGreyUI openSettingsMenu];
  NSString* label = l10n_util::GetNSString(IDS_IOS_AUTOFILL);
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(label)]
      performAction:grey_tap()];

  NSString* cellLabel = address;
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(cellLabel)]
      performAction:grey_tap()];
}

// Close the settings.
- (void)exitSettingsMenu {
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  // Wait for UI components to finish loading.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Test that the page for viewing autofill profile details is as expected.
- (void)testAutofillProfileViewPage {
  autofill::AutofillProfile profile = [self addAutofillProfile];
  [self openEditAddress:kAddressLabel];

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
  autofill::AutofillProfile profile = [self addAutofillProfile];
  [self openEditAddress:kAddressLabel];

  // Keep editing the Country field and verify that validation works.
  for (const UserTypedCountryExpectedResultPair& expectation : kCountryTests) {
    // Switch on edit mode.
    [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                            IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
        performAction:grey_tap()];

    // Replace the text field with the user-version of the country.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(GetTextFieldForID(
                                            IDS_IOS_AUTOFILL_COUNTRY))]
        performAction:grey_replaceText(expectation.user_typed_country)];

    // Switch off edit mode.
    [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
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
  autofill::AutofillProfile profile = [self addAutofillProfile];
  [self openEditAddress:kAddressLabel];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  [self exitSettingsMenu];
}

// Test that the page for editing autofill profile details is accessible.
- (void)testAccessibilityOnAutofillProfileEditPage {
  autofill::AutofillProfile profile = [self addAutofillProfile];
  [self openEditAddress:kAddressLabel];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  [self exitSettingsMenu];
}

// Checks that if the autofill profiles and credit cards list view is in edit
// mode, the Autofill, address, and credit card switches are disabled.
- (void)testListViewEditMode {
  autofill::AutofillProfile profile = [self addAutofillProfile];
  autofill::CreditCard creditCard = [self addCreditCard];

  // Go to Autofill Settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                   l10n_util::GetNSString(IDS_IOS_AUTOFILL))]
      performAction:grey_tap()];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      performAction:grey_tap()];

  // Check the Autofill, address, and credit card switches are disabled.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                          @"autofillItem_switch", YES, NO)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                          @"addressItem_switch", YES, NO)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                          @"cardItem_switch", YES, NO)]
      assertWithMatcher:grey_notNil()];
}

// Checks that the autofill address switch can be toggled on/off independently
// and the list of autofill profiles is not affected by it.
- (void)testToggleAutofillAddressSwitch {
  autofill::AutofillProfile profile = [self addAutofillProfile];

  // Go to Autofill Settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                   l10n_util::GetNSString(IDS_IOS_AUTOFILL))]
      performAction:grey_tap()];

  // Toggle the Autofill address switch off.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                          @"addressItem_switch", YES, YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Expect Autofill profiles to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"John H. Doe, 666 Erebus St.")]
      assertWithMatcher:grey_notNil()];

  // Toggle the Autofill address switch back on.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                          @"addressItem_switch", NO, YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Expect Autofill profiles to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"John H. Doe, 666 Erebus St.")]
      assertWithMatcher:grey_notNil()];
}

// Checks that the autofill credit card switch can be toggled on/off
// independently and the list of autofill credit cards is not affected by it.
- (void)testToggleAutofillCreditCardSwitch {
  autofill::CreditCard creditCard = [self addCreditCard];

  // Go to Autofill Settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                   l10n_util::GetNSString(IDS_IOS_AUTOFILL))]
      performAction:grey_tap()];

  // Toggle the Autofill credit card switch off.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                          @"cardItem_switch", YES, YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Expect Autofill credit cards to remain visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(
                     @"Test User, Visa  ‪• • • • 1111‬")]
      assertWithMatcher:grey_notNil()];

  // Toggle the Autofill credit card switch back on.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                          @"cardItem_switch", NO, YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Expect Autofill credit cards to remain visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(
                     @"Test User, Visa  ‪• • • • 1111‬")]
      assertWithMatcher:grey_notNil()];
}

// Tests that toggling the Autofill switch on and off disables and enables the
// Autofill address and credit card switches respectively and that the list of
// autofill addresses and credit cards is not affected by it.
- (void)testToggleAutofillSwitches {
  autofill::AutofillProfile profile = [self addAutofillProfile];
  autofill::CreditCard creditCard = [self addCreditCard];

  // Go to Autofill Settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                   l10n_util::GetNSString(IDS_IOS_AUTOFILL))]
      performAction:grey_tap()];

  // Toggle the Autofill switch off and back on.
  for (BOOL expectedState : {YES, NO}) {
    // Toggle the Autofill switch.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                            @"autofillItem_switch",
                                            expectedState, YES)]
        performAction:chrome_test_util::TurnSettingsSwitchOn(!expectedState)];

    // Expect address and credit card switches to be off when Autofill toggle is
    // off and on when Autofill toggle is on.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                            @"addressItem_switch",
                                            !expectedState, YES)]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                     @"cardItem_switch", !expectedState, YES)]
        assertWithMatcher:grey_notNil()];

    // Expect Autofill addresses and credit cards to remain visible.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                            @"John H. Doe, 666 Erebus St.")]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityLabel(
                       @"Test User, Visa  ‪• • • • 1111‬")]
        assertWithMatcher:grey_notNil()];
  }
}

@end
