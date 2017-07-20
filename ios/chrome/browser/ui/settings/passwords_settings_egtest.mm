// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <TargetConditionals.h>

#include "base/callback.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/settings/password_details_collection_view_controller_for_testing.h"
#import "ios/chrome/browser/ui/settings/reauthentication_module.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// This test complements
// password_details_collection_view_controller_unittest.mm. Very simple
// integration tests and features which are not currently unittestable should
// go here, the rest into the unittest.
// This test only uses the new UI which allows viewing passwords.
// TODO(crbug.com/159166): Remove the above sentence once the new UI is the
// default one.

using autofill::PasswordForm;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::SettingsMenuBackButton;

namespace {

// How many points to scroll at a time when searching for an element. Setting it
// too low means searching takes too long and the test might time out. Setting
// it too high could result in scrolling way past the searched element.
constexpr int kScrollAmount = 150;

// Returns the GREYElementInteraction* for the cell on the password list with
// the given |username|. It scrolls down if necessary to ensure that the matched
// cell is interactable. The result can be used to perform user actions or
// checks.
GREYElementInteraction* GetInteractionForPasswordEntry(NSString* username) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   ButtonWithAccessibilityLabel(username),
                                   grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"SavePasswordsCollectionViewController")];
}

// Returns the GREYElementInteraction* for the item on the detail view
// identified with the given |matcher|. It scrolls down if necessary to ensure
// that the matched cell is interactable. The result can be used to perform
// user actions or checks.
GREYElementInteraction* GetInteractionForPasswordDetailItem(
    id<GREYMatcher> matcher) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")];
}

id<GREYMatcher> SiteHeader() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

id<GREYMatcher> UsernameHeader() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

id<GREYMatcher> PasswordHeader() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

id<GREYMatcher> FederationHeader() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_FEDERATION)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

GREYLayoutConstraint* Below() {
  return [GREYLayoutConstraint
      layoutConstraintWithAttribute:kGREYLayoutAttributeTop
                          relatedBy:kGREYLayoutRelationGreaterThanOrEqual
               toReferenceAttribute:kGREYLayoutAttributeBottom
                         multiplier:1.0
                           constant:0.0];
}

// Matcher for the Copy site button in Password Details view.
id<GREYMatcher> CopySiteButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabel(
          [NSString stringWithFormat:@"%@: %@",
                                     l10n_util::GetNSString(
                                         IDS_IOS_SHOW_PASSWORD_VIEW_SITE),
                                     l10n_util::GetNSString(
                                         IDS_IOS_SETTINGS_SITE_COPY_BUTTON)]),
      grey_interactable(), nullptr);
}

// Matcher for the Copy username button in Password Details view.
id<GREYMatcher> CopyUsernameButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabel([NSString
          stringWithFormat:@"%@: %@",
                           l10n_util::GetNSString(
                               IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME),
                           l10n_util::GetNSString(
                               IDS_IOS_SETTINGS_USERNAME_COPY_BUTTON)]),
      grey_interactable(), nullptr);
}

// Matcher for the Copy password button in Password Details view.
id<GREYMatcher> CopyPasswordButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabel([NSString
          stringWithFormat:@"%@: %@",
                           l10n_util::GetNSString(
                               IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD),
                           l10n_util::GetNSString(
                               IDS_IOS_SETTINGS_PASSWORD_COPY_BUTTON)]),
      grey_interactable(), nullptr);
}

// Matcher for the Show password button in Password Details view.
id<GREYMatcher> ShowPasswordButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON)),
                    grey_interactable(), nullptr);
}

// Matcher for the Delete button in Password Details view.
id<GREYMatcher> DeleteButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_DELETE_BUTTON)),
                    grey_interactable(), nullptr);
}

// This is similar to grey_ancestor, but only limited to the immediate parent.
id<GREYMatcher> MatchParentWith(id<GREYMatcher> parentMatcher) {
  MatchesBlock matches = ^BOOL(id element) {
    id parent = [element isKindOfClass:[UIView class]]
                    ? [element superview]
                    : [element accessibilityContainer];
    return (parent && [parentMatcher matches:parent]);
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description appendText:[NSString stringWithFormat:@"parentThatMatches(%@)",
                                                       parentMatcher]];
  };
  return grey_allOf(
      grey_anyOf(grey_kindOfClass([UIView class]),
                 grey_respondsToSelector(@selector(accessibilityContainer)),
                 nil),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      nil);
}

// Matches the pop-up (call-out) menu item with accessibility label equal to the
// translated string identified by |label|.
id<GREYMatcher> PopUpMenuItemWithLabel(int label) {
  // This is a hack relying on UIKit's internal structure. There are multiple
  // items with the label the test is looking for, because the menu items likely
  // have the same labels as the buttons for the same function. There is no easy
  // way to identify elements which are part of the pop-up, because the
  // associated classes are internal to UIKit. However, the pop-up items are
  // composed of a button-type element (without accessibility traits of a
  // button) owning a label, both with the same accessibility labels. This is
  // differentiating the pop-up items from the other buttons.
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(label)),
      MatchParentWith(grey_accessibilityLabel(l10n_util::GetNSString(label))),
      nullptr);
}

}  // namespace

@interface MockReauthenticationModule : NSObject<ReauthenticationProtocol>

@property(nonatomic, assign) BOOL shouldSucceed;

@end

@implementation MockReauthenticationModule

@synthesize shouldSucceed = _shouldSucceed;

- (BOOL)canAttemptReauth {
  return YES;
}

- (void)attemptReauthWithLocalizedReason:(NSString*)localizedReason
                                 handler:(void (^)(BOOL success))
                                             showCopyPasswordsHandler {
  showCopyPasswordsHandler(_shouldSucceed);
}

@end

namespace {

// Replace the reauthentication module in
// PasswordDetailsCollectionViewController with a fake one to avoid being
// blocked with a reauth prompt, and return the fake reauthentication module.
MockReauthenticationModule* SetUpAndReturnMockReauthenticationModule() {
  MockReauthenticationModule* mock_reauthentication_module =
      [[MockReauthenticationModule alloc] init];
  SettingsNavigationController* settings_navigation_controller =
      base::mac::ObjCCastStrict<SettingsNavigationController>(
          top_view_controller::TopPresentedViewController());
  PasswordDetailsCollectionViewController*
      password_details_collection_view_controller =
          base::mac::ObjCCastStrict<PasswordDetailsCollectionViewController>(
              settings_navigation_controller.topViewController);
  [password_details_collection_view_controller
      setReauthenticationModule:mock_reauthentication_module];
  return mock_reauthentication_module;
}

}  // namespace

// Various tests for the Save Passwords section of the settings.
@interface PasswordsSettingsTestCase : ChromeTestCase
@end

@implementation PasswordsSettingsTestCase

- (void)tearDown {
  // Snackbars triggered by tests stay up for a limited time even if the
  // settings get closed. Ensure that they are closed to avoid interference with
  // other tests.
  [MDCSnackbarManager
      dismissAndCallCompletionBlocksWithCategory:@"PasswordsSnackbarCategory"];
  [super tearDown];
}

- (scoped_refptr<password_manager::PasswordStore>)passwordStore {
  // ServiceAccessType governs behaviour in Incognito: only modifications with
  // EXPLICIT_ACCESS, which correspond to user's explicit gesture, succeed.
  // This test does not deal with Incognito, so the value of the argument is
  // irrelevant.
  return IOSChromePasswordStoreFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState(),
      ServiceAccessType::EXPLICIT_ACCESS);
}

// Saves |form| to the password store and waits until the async processing is
// done.
- (void)savePasswordFormToStore:(const PasswordForm&)form {
  [self passwordStore]->AddLogin(form);
  // Allow the PasswordStore to process this on the DB thread.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Saves an example form in the store.
- (void)saveExamplePasswordForm {
  PasswordForm example;
  example.username_value = base::ASCIIToUTF16("concrete username");
  example.password_value = base::ASCIIToUTF16("concrete password");
  example.origin = GURL("https://example.com");
  example.signon_realm = example.origin.spec();
  [self savePasswordFormToStore:example];
}

// Removes all credentials stored.
- (void)clearPasswordStore {
  [self passwordStore]->RemoveLoginsCreatedBetween(base::Time(), base::Time(),
                                                   base::Closure());
  // Allow the PasswordStore to process this on the DB thread.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Opens the passwords page from the NTP. It requires no menus to be open.
- (void)openPasswordSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuPasswordsButton()];
}

// Tap Edit in any settings view.
- (void)tapEdit {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      performAction:grey_tap()];
}

// Tap Done in any settings view.
- (void)tapDone {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Verifies the UI elements are accessible on the Passwords page.
// TODO(crbug.com/159166): This differs from testAccessibilityOnPasswords in
// settings_egtest.mm in that here this tests the new UI (for viewing
// passwords), where in settings_egtest.mm the default (old) UI is tested.
// Once the new is the default, just remove the test in settings_egtest.mm.
- (void)testAccessibilityOnPasswords {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  [self tapEdit];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self tapDone];

  // Inspect "password details" view.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to copy a password provide appropriate feedback,
// both when reauthentication succeeds and when it fails.
- (void)testCopyPasswordToast {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  MockReauthenticationModule* mock_reauthentication_module =
      SetUpAndReturnMockReauthenticationModule();

  // Check the snackbar in case of successful reauthentication.
  mock_reauthentication_module.shouldSucceed = YES;
  [GetInteractionForPasswordDetailItem(CopyPasswordButton())
      performAction:grey_tap()];

  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  // Check the snackbar in case of failed reauthentication.
  mock_reauthentication_module.shouldSucceed = NO;
  [GetInteractionForPasswordDetailItem(CopyPasswordButton())
      performAction:grey_tap()];

  snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_NOT_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to copy a username provide appropriate feedback.
- (void)testCopyUsernameToast {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(CopyUsernameButton())
      performAction:grey_tap()];
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to copy a site URL provide appropriate feedback.
- (void)testCopySiteToast {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(CopySiteButton())
      performAction:grey_tap()];
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that deleting a password from password details view goes back to the
// list-of-passwords view.
- (void)testDeletion {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  // Save form to be deleted later.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      performAction:grey_tap()];

  // Tap the alert's Delete... button to confirm. Check sufficient visibility in
  // addition to interactability to differentiate against the above
  // DeleteButton()-matching element, which is covered by the alert enought to
  // prevent being sufficiently visible, but not enough to prevent
  // interactability.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   ButtonWithAccessibilityLabel(
                                       l10n_util::GetNSString(
                                           IDS_IOS_CONFIRM_PASSWORD_DELETION)),
                                   grey_interactable(),
                                   grey_sufficientlyVisible(), nullptr)]
      performAction:grey_tap()];

  // Check that the current view is now the list view, by locating the header
  // of the list of passwords.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_SETTINGS_PASSWORDS_SAVED_HEADING)),
                            grey_accessibilityTrait(UIAccessibilityTraitHeader),
                            nullptr)] assertWithMatcher:grey_notNil()];

  // Also verify that the removed password is no longer in the list.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that deleting a password from password details can be cancelled.
- (void)testCancelDeletion {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  // Save form to be deleted later.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      performAction:grey_tap()];

  // Tap the alert's Cancel button to cancel.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   ButtonWithAccessibilityLabel(
                                       l10n_util::GetNSString(
                                           IDS_IOS_CANCEL_PASSWORD_DELETION)),
                                   grey_interactable(), nullptr)]
      performAction:grey_tap()];

  // Check that the current view is still the detail view, by locating the Copy
  // button.
  [[EarlGrey selectElementWithMatcher:CopyPasswordButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Go back to the list view and verify that the password is still in the
  // list.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that if the list view is in edit mode, then the details password view
// is not accessible on tapping the entries.
- (void)testEditMode {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  // Save a form to have something to tap on.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [self tapEdit];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Check that the current view is not the detail view, by failing to locate
  // the Copy button.
  [[EarlGrey selectElementWithMatcher:CopyPasswordButton()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to copy the site via the context menu item provide an
// appropriate feedback.
- (void)testCopySiteMenuItem {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Tap the site cell to display the context menu.
  [GetInteractionForPasswordDetailItem(grey_accessibilityLabel(
      @"https://example.com/")) performAction:grey_tap()];

  // Tap the context menu item for copying.
  [[EarlGrey selectElementWithMatcher:PopUpMenuItemWithLabel(
                                          IDS_IOS_SETTINGS_SITE_COPY_MENU_ITEM)]
      performAction:grey_tap()];

  // Check the snackbar.
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to copy the username via the context menu item provide
// an appropriate feedback.
- (void)testCopyUsernameMenuItem {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Tap the username cell to display the context menu.
  [GetInteractionForPasswordDetailItem(
      grey_accessibilityLabel(@"concrete username")) performAction:grey_tap()];

  // Tap the context menu item for copying.
  [[EarlGrey
      selectElementWithMatcher:PopUpMenuItemWithLabel(
                                   IDS_IOS_SETTINGS_USERNAME_COPY_MENU_ITEM)]
      performAction:grey_tap()];

  // Check the snackbar.
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to copy the password via the context menu item provide
// an appropriate feedback.
- (void)testCopyPasswordMenuItem {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Tap the password cell to display the context menu.
  [GetInteractionForPasswordDetailItem(
      grey_accessibilityLabel(@"●●●●●●●●●●●●●●●●●")) performAction:grey_tap()];

  // Make sure to capture the reauthentication module in a variable until the
  // end of the test, otherwise it might get deleted too soon and break the
  // functionality of copying and viewing passwords.
  MockReauthenticationModule* mock_reauthentication_module =
      SetUpAndReturnMockReauthenticationModule();
  mock_reauthentication_module.shouldSucceed = YES;

  // Tap the context menu item for copying.
  [[EarlGrey
      selectElementWithMatcher:PopUpMenuItemWithLabel(
                                   IDS_IOS_SETTINGS_PASSWORD_COPY_MENU_ITEM)]
      performAction:grey_tap()];

  // Check the snackbar.
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to show and hide the password via the context menu item
// provide an appropriate feedback.
- (void)testShowHidePasswordMenuItem {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Tap the password cell to display the context menu.
  [GetInteractionForPasswordDetailItem(
      grey_accessibilityLabel(@"●●●●●●●●●●●●●●●●●")) performAction:grey_tap()];

  // Make sure to capture the reauthentication module in a variable until the
  // end of the test, otherwise it might get deleted too soon and break the
  // functionality of copying and viewing passwords.
  MockReauthenticationModule* mock_reauthentication_module =
      SetUpAndReturnMockReauthenticationModule();
  mock_reauthentication_module.shouldSucceed = YES;

  // Tap the context menu item for showing.
  [[EarlGrey
      selectElementWithMatcher:PopUpMenuItemWithLabel(
                                   IDS_IOS_SETTINGS_PASSWORD_SHOW_MENU_ITEM)]
      performAction:grey_tap()];

  // Tap the password cell to display the context menu again, and to check that
  // the password was unmasked.
  [GetInteractionForPasswordDetailItem(
      grey_accessibilityLabel(@"concrete password")) performAction:grey_tap()];

  // Tap the context menu item for hiding.
  [[EarlGrey
      selectElementWithMatcher:PopUpMenuItemWithLabel(
                                   IDS_IOS_SETTINGS_PASSWORD_HIDE_MENU_ITEM)]
      performAction:grey_tap()];

  // Check that the password is masked again.
  [GetInteractionForPasswordDetailItem(grey_accessibilityLabel(
      @"●●●●●●●●●●●●●●●●●")) assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that federated credentials have no password but show the federation.
- (void)testFederated {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  PasswordForm federated;
  federated.username_value = base::ASCIIToUTF16("federated username");
  federated.origin = GURL("https://example.com");
  federated.signon_realm = federated.origin.spec();
  federated.federation_origin =
      url::Origin(GURL("https://famous.provider.net"));
  [self savePasswordFormToStore:federated];

  [self openPasswordSettings];

  [GetInteractionForPasswordEntry(@"example.com, federated username")
      performAction:grey_tap()];

  // Check that the Site, Username, Federation and Delete Saved Password
  // sections are there.
  [GetInteractionForPasswordDetailItem(SiteHeader())
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordDetailItem(UsernameHeader())
      assertWithMatcher:grey_notNil()];
  // For federation check both the section header and content.
  [GetInteractionForPasswordDetailItem(FederationHeader())
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordDetailItem(grey_text(@"famous.provider.net"))
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordDetailItem(DeleteButton())
      assertWithMatcher:grey_notNil()];

  // Check that the password is not present.
  [GetInteractionForPasswordDetailItem(PasswordHeader())
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks the order of the elements in the detail view layout for a
// non-federated, non-blacklisted credential.
- (void)testLayoutNormal {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(SiteHeader())
      assertWithMatcher:grey_notNil()];
  id<GREYMatcher> siteCell = grey_accessibilityLabel(@"https://example.com/");
  [GetInteractionForPasswordDetailItem(siteCell)
      assertWithMatcher:grey_layout(@[ Below() ], SiteHeader())];
  [GetInteractionForPasswordDetailItem(CopySiteButton())
      assertWithMatcher:grey_layout(@[ Below() ], siteCell)];

  [GetInteractionForPasswordDetailItem(UsernameHeader())
      assertWithMatcher:grey_layout(@[ Below() ], CopySiteButton())];
  id<GREYMatcher> usernameCell = grey_accessibilityLabel(@"concrete username");
  [GetInteractionForPasswordDetailItem(usernameCell)
      assertWithMatcher:grey_layout(@[ Below() ], UsernameHeader())];
  [GetInteractionForPasswordDetailItem(CopyUsernameButton())
      assertWithMatcher:grey_layout(@[ Below() ], usernameCell)];

  [GetInteractionForPasswordDetailItem(PasswordHeader())
      assertWithMatcher:grey_layout(@[ Below() ], CopyUsernameButton())];
  id<GREYMatcher> passwordCell = grey_accessibilityLabel(@"●●●●●●●●●●●●●●●●●");
  [GetInteractionForPasswordDetailItem(passwordCell)
      assertWithMatcher:grey_layout(@[ Below() ], PasswordHeader())];
  [GetInteractionForPasswordDetailItem(CopyPasswordButton())
      assertWithMatcher:grey_layout(@[ Below() ], passwordCell)];
  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      assertWithMatcher:grey_layout(@[ Below() ], CopyPasswordButton())];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      assertWithMatcher:grey_layout(@[ Below() ], ShowPasswordButton())];

  // Check that the federation block is not present. Match directly to also
  // catch the case where the block would be present but not currently visible
  // due to the scrolling state.
  [[EarlGrey selectElementWithMatcher:FederationHeader()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks the order of the elements in the detail view layout for a blacklisted
// credential.
- (void)testLayoutBlacklisted {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  PasswordForm blacklisted;
  blacklisted.origin = GURL("https://example.com");
  blacklisted.signon_realm = blacklisted.origin.spec();
  blacklisted.blacklisted_by_user = true;
  [self savePasswordFormToStore:blacklisted];

  [self openPasswordSettings];

  [GetInteractionForPasswordEntry(@"example.com") performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(SiteHeader())
      assertWithMatcher:grey_notNil()];
  id<GREYMatcher> siteCell = grey_accessibilityLabel(@"https://example.com/");
  [GetInteractionForPasswordDetailItem(siteCell)
      assertWithMatcher:grey_layout(@[ Below() ], SiteHeader())];
  [GetInteractionForPasswordDetailItem(CopySiteButton())
      assertWithMatcher:grey_layout(@[ Below() ], siteCell)];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      assertWithMatcher:grey_layout(@[ Below() ], CopySiteButton())];

  // Check that the other blocks are not present. Match directly to also catch
  // the case where those blocks would be present but not currently visible due
  // to the scrolling state.
  [[EarlGrey selectElementWithMatcher:UsernameHeader()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:PasswordHeader()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:FederationHeader()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks the order of the elements in the detail view layout for a federated
// credential.
- (void)testLayoutFederated {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kViewPasswords);

  PasswordForm federated;
  federated.username_value = base::ASCIIToUTF16("federated username");
  federated.origin = GURL("https://example.com");
  federated.signon_realm = federated.origin.spec();
  federated.federation_origin =
      url::Origin(GURL("https://famous.provider.net"));
  [self savePasswordFormToStore:federated];

  [self openPasswordSettings];

  [GetInteractionForPasswordEntry(@"example.com, federated username")
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(SiteHeader())
      assertWithMatcher:grey_notNil()];
  id<GREYMatcher> siteCell = grey_accessibilityLabel(@"https://example.com/");
  [GetInteractionForPasswordDetailItem(siteCell)
      assertWithMatcher:grey_layout(@[ Below() ], SiteHeader())];
  [GetInteractionForPasswordDetailItem(CopySiteButton())
      assertWithMatcher:grey_layout(@[ Below() ], siteCell)];

  [GetInteractionForPasswordDetailItem(UsernameHeader())
      assertWithMatcher:grey_layout(@[ Below() ], CopySiteButton())];
  id<GREYMatcher> usernameCell = grey_accessibilityLabel(@"federated username");
  [GetInteractionForPasswordDetailItem(usernameCell)
      assertWithMatcher:grey_layout(@[ Below() ], UsernameHeader())];
  [GetInteractionForPasswordDetailItem(CopyUsernameButton())
      assertWithMatcher:grey_layout(@[ Below() ], usernameCell)];

  [GetInteractionForPasswordDetailItem(FederationHeader())
      assertWithMatcher:grey_layout(@[ Below() ], CopyUsernameButton())];
  id<GREYMatcher> federationCell = grey_text(@"famous.provider.net");
  [GetInteractionForPasswordDetailItem(federationCell)
      assertWithMatcher:grey_layout(@[ Below() ], FederationHeader())];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      assertWithMatcher:grey_layout(@[ Below() ], federationCell)];

  // Check that the password is not present. Match directly to also catch the
  // case where the password header would be present but not currently visible
  // due to the scrolling state.
  [[EarlGrey selectElementWithMatcher:PasswordHeader()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [self tapDone];
  [self clearPasswordStore];
}

@end
