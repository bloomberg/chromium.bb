// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <TargetConditionals.h>

#include "base/callback.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
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
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsMenuButton;

namespace {

// How many points to scroll at a time when searching for an element. Setting it
// too low means searching takes too long and the test might time out. Setting
// it too high could result in scrolling way past the searched element.
constexpr int kScrollAmount = 150;

// Matcher for the Save Passwords cell on the main Settings screen.
id<GREYMatcher> PasswordsButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_SAVE_PASSWORDS);
}

// Matcher for a password entry for |username|.
id<GREYMatcher> Entry(NSString* username) {
  return ButtonWithAccessibilityLabel(username);
}

// Matcher for the Edit button in Save Passwords view.
id<GREYMatcher> EditButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON);
}

// Copy buttons have unique accessibility labels, but the visible text is the
// same for multiple types of copied items (just "Copy"). Therefore the
// matchers here check the relative position of the Copy buttons to their
// respective section headers as well. The scheme of the vertical order is:
//   Site header
//   Copy (site) button
//   Username header
//   Copy (username) button
//   Password header
//   Copy (password) button

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

GREYLayoutConstraint* Above() {
  return [GREYLayoutConstraint
      layoutConstraintWithAttribute:kGREYLayoutAttributeBottom
                          relatedBy:kGREYLayoutRelationLessThanOrEqual
               toReferenceAttribute:kGREYLayoutAttributeTop
                         multiplier:1.0
                           constant:0.0];
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
      grey_interactable(), grey_layout(@[ Below() ], SiteHeader()),
      grey_layout(@[ Above() ], UsernameHeader()),
      grey_layout(@[ Above() ], PasswordHeader()), nullptr);
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
      grey_interactable(), grey_layout(@[ Below() ], SiteHeader()),
      grey_layout(@[ Below() ], UsernameHeader()),
      grey_layout(@[ Above() ], PasswordHeader()), nullptr);
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
      grey_interactable(), grey_layout(@[ Below() ], SiteHeader()),
      grey_layout(@[ Below() ], UsernameHeader()),
      grey_layout(@[ Below() ], PasswordHeader()), nullptr);
}

// Matcher for the Copy site button in Password Details view.
id<GREYMatcher> DeleteButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_DELETE_BUTTON)),
      grey_interactable(), grey_layout(@[ Below() ], SiteHeader()),
      grey_layout(@[ Below() ], UsernameHeader()),
      grey_layout(@[ Below() ], PasswordHeader()), nullptr);
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

// Return pref for saving passwords back to the passed value and restores the
// experimental flag for viewing passwords.
- (void)passwordsTearDown:(BOOL)defaultPasswordManagementSetting
                         :(NSString*)oldExperiment {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* preferences = browserState->GetPrefs();
  preferences->SetBoolean(
      password_manager::prefs::kPasswordManagerSavingEnabled,
      defaultPasswordManagementSetting);

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:oldExperiment forKey:@"EnableViewCopyPasswords"];
}

// Sets the preference to allow saving passwords and activates the flag to use
// the new UI for viewing passwords in settings. Also, ensures that original
// state is restored after the test ends.
- (void)scopedEnablePasswordManagementAndViewingUI {
  // Retrieve the original preference state.
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* preferences = browserState->GetPrefs();
  bool defaultPasswordManagerSavingPref = preferences->GetBoolean(
      password_manager::prefs::kPasswordManagerSavingEnabled);

  // Retrieve the experiment setting.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* oldSetting = [defaults stringForKey:@"EnableViewCopyPasswords"];

  // Ensure restoring that on tear-down.
  __weak PasswordsSettingsTestCase* weakSelf = self;
  [self setTearDownHandler:^{
    [weakSelf passwordsTearDown:defaultPasswordManagerSavingPref:oldSetting];
  }];

  // Enable saving.
  preferences->SetBoolean(
      password_manager::prefs::kPasswordManagerSavingEnabled, true);

  // Enable viewing passwords in settings.
  [defaults setObject:@"Enabled" forKey:@"EnableViewCopyPasswords"];
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
  // Open settings and verify data in the view controller.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:SettingsMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasswordsButton()]
      performAction:grey_tap()];
}

// Tap back arrow, to get one level higher in settings.
- (void)tapBackArrow {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"ic_arrow_back"),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];
}

// Tap Edit in any settings view.
- (void)tapEdit {
  [[EarlGrey selectElementWithMatcher:EditButton()] performAction:grey_tap()];
}

// Tap Done in any settings view.
- (void)tapDone {
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Verifies the UI elements are accessible on the Passwords page.
// TODO(crbug.com/159166): This differs from testAccessibilityOnPasswords in
// settings_egtest.mm in that here this tests the new UI (for viewing
// passwords), where in settings_egtest.mm the default (old) UI is tested.
// Once the new is the default, just remove the test in settings_egtest.mm.
- (void)testAccessibilityOnPasswords {
  [self scopedEnablePasswordManagementAndViewingUI];

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  [self tapEdit];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self tapDone];

  // Inspect "password details" view.
  [[EarlGrey
      selectElementWithMatcher:Entry(@"https://example.com, concrete username")]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self tapBackArrow];

  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to copy a password provide appropriate feedback,
// both when reauthentication succeeds and when it fails.
- (void)testCopyPasswordToast {
  [self scopedEnablePasswordManagementAndViewingUI];

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [[EarlGrey
      selectElementWithMatcher:Entry(@"https://example.com, concrete username")]
      performAction:grey_tap()];

  MockReauthenticationModule* mock_reauthentication_module =
      SetUpAndReturnMockReauthenticationModule();

  // Check the snackbar in case of successful reauthentication.
  mock_reauthentication_module.shouldSucceed = YES;
  [[[EarlGrey selectElementWithMatcher:CopyPasswordButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];

  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  // Check the snackbar in case of failed reauthentication.
  mock_reauthentication_module.shouldSucceed = NO;
  [[[EarlGrey selectElementWithMatcher:CopyPasswordButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];

  snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_NOT_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [self tapBackArrow];
  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to copy a username provide appropriate feedback.
- (void)testCopyUsernameToast {
  [self scopedEnablePasswordManagementAndViewingUI];

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [[EarlGrey
      selectElementWithMatcher:Entry(@"https://example.com, concrete username")]
      performAction:grey_tap()];

  // Check the snackbar.
  [[[EarlGrey selectElementWithMatcher:CopyUsernameButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];

  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [self tapBackArrow];
  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to copy a site URL provide appropriate feedback.
- (void)testCopySiteToast {
  [self scopedEnablePasswordManagementAndViewingUI];

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [[EarlGrey
      selectElementWithMatcher:Entry(@"https://example.com, concrete username")]
      performAction:grey_tap()];

  // Check the snackbar.
  [[[EarlGrey selectElementWithMatcher:CopySiteButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];

  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [self tapBackArrow];
  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that deleting a password from password details view goes back to the
// list-of-passwords view.
- (void)testDeletion {
  [self scopedEnablePasswordManagementAndViewingUI];

  // Save form to be deleted later.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [[EarlGrey
      selectElementWithMatcher:Entry(@"https://example.com, concrete username")]
      performAction:grey_tap()];

  // Tap the Delete... button.
  [[[EarlGrey selectElementWithMatcher:DeleteButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];

  // Tap the alert's Delete... button to confirm.
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
                                IDS_PASSWORD_MANAGER_SHOW_PASSWORDS_TAB_TITLE)),
                            grey_accessibilityTrait(UIAccessibilityTraitHeader),
                            nullptr)] assertWithMatcher:grey_notNil()];

  // Also verify that the removed password is no longer in the list.
  [[EarlGrey
      selectElementWithMatcher:Entry(@"https://example.com, concrete username")]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that deleting a password from password details can be cancelled.
- (void)testCancelDeletion {
  [self scopedEnablePasswordManagementAndViewingUI];

  // Save form to be deleted later.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [[EarlGrey
      selectElementWithMatcher:Entry(@"https://example.com, concrete username")]
      performAction:grey_tap()];

  // Tap the Delete... button.
  [[[EarlGrey selectElementWithMatcher:DeleteButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];

  // Tap the alert's Cancel button to cancel.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   ButtonWithAccessibilityLabel(
                                       l10n_util::GetNSString(
                                           IDS_IOS_CANCEL_PASSWORD_DELETION)),
                                   grey_interactable(),
                                   grey_sufficientlyVisible(), nullptr)]
      performAction:grey_tap()];

  // Check that the current view is still the detail view, by locating the Copy
  // button.
  [[EarlGrey selectElementWithMatcher:CopyPasswordButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Go back to the list view and verify that the password is still in the
  // list.
  [self tapBackArrow];
  [[EarlGrey
      selectElementWithMatcher:Entry(@"https://example.com, concrete username")]
      assertWithMatcher:grey_sufficientlyVisible()];

  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that if the list view is in edit mode, then the details password view
// is not accessible on tapping the entries.
- (void)testEditMode {
  [self scopedEnablePasswordManagementAndViewingUI];

  // Save a form to have something to tap on.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [self tapEdit];

  [[EarlGrey
      selectElementWithMatcher:Entry(@"https://example.com, concrete username")]
      performAction:grey_tap()];

  // Check that the current view is not the detail view, by failing to locate
  // the Copy button.
  [[EarlGrey selectElementWithMatcher:CopyPasswordButton()]
      assertWithMatcher:grey_nil()];

  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that blacklisted credentials only have the Site section.
- (void)testBlacklisted {
  [self scopedEnablePasswordManagementAndViewingUI];

  PasswordForm blacklisted;
  blacklisted.origin = GURL("https://example.com");
  blacklisted.signon_realm = blacklisted.origin.spec();
  blacklisted.blacklisted_by_user = true;
  [self savePasswordFormToStore:blacklisted];

  [self openPasswordSettings];

  [[EarlGrey selectElementWithMatcher:Entry(@"https://example.com")]
      performAction:grey_tap()];

  // Check that the Site section is there as well as the Delete button.
  [[EarlGrey selectElementWithMatcher:SiteHeader()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Not using DeleteButton() matcher here, because that also encodes the
  // relative position against the password section, which is missing in this
  // case.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_SETTINGS_PASSWORD_DELETE_BUTTON))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the rest is not present.
  [[EarlGrey selectElementWithMatcher:UsernameHeader()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:PasswordHeader()]
      assertWithMatcher:grey_nil()];

  [self tapBackArrow];
  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to copy the site via the context menu item provide an
// appropriate feedback.
- (void)testCopySiteMenuItem {
  [self scopedEnablePasswordManagementAndViewingUI];

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [[EarlGrey
      selectElementWithMatcher:Entry(@"https://example.com, concrete username")]
      performAction:grey_tap()];

  // Tap the site cell to display the context menu.
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"https://example.com/")]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];

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

  [self tapBackArrow];
  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to copy the username via the context menu item provide
// an appropriate feedback.
- (void)testCopyUsernameMenuItem {
  [self scopedEnablePasswordManagementAndViewingUI];

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [[EarlGrey
      selectElementWithMatcher:Entry(@"https://example.com, concrete username")]
      performAction:grey_tap()];

  // Tap the username cell to display the context menu.
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"concrete username")]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];

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

  [self tapBackArrow];
  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to copy the password via the context menu item provide
// an appropriate feedback.
- (void)testCopyPasswordMenuItem {
  [self scopedEnablePasswordManagementAndViewingUI];

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [[EarlGrey
      selectElementWithMatcher:Entry(@"https://example.com, concrete username")]
      performAction:grey_tap()];

  // Tap the password cell to display the context menu.
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"●●●●●●●●●●●●●●●●●")]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];

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

  [self tapBackArrow];
  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that attempts to show and hide the password via the context menu item
// provide an appropriate feedback.
- (void)testShowHidePasswordMenuItem {
  [self scopedEnablePasswordManagementAndViewingUI];

  // Saving a form is needed for using the "password details" view.
  [self saveExamplePasswordForm];

  [self openPasswordSettings];

  [[EarlGrey
      selectElementWithMatcher:Entry(@"https://example.com, concrete username")]
      performAction:grey_tap()];

  // Tap the password cell to display the context menu.
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"●●●●●●●●●●●●●●●●●")]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];

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
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"concrete password")]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];

  // Tap the context menu item for hiding.
  [[EarlGrey
      selectElementWithMatcher:PopUpMenuItemWithLabel(
                                   IDS_IOS_SETTINGS_PASSWORD_HIDE_MENU_ITEM)]
      performAction:grey_tap()];

  // Check that the password is masked again.
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"●●●●●●●●●●●●●●●●●")]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      assertWithMatcher:grey_sufficientlyVisible()];

  [self tapBackArrow];
  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

// Checks that federated credentials have no password but show the federation.
- (void)testFederated {
  [self scopedEnablePasswordManagementAndViewingUI];

  PasswordForm federated;
  federated.username_value = base::ASCIIToUTF16("federated username");
  federated.origin = GURL("https://example.com");
  federated.signon_realm = federated.origin.spec();
  federated.federation_origin =
      url::Origin(GURL("https://famous.provider.net"));
  [self savePasswordFormToStore:federated];

  [self openPasswordSettings];

  [[EarlGrey
      selectElementWithMatcher:Entry(
                                   @"https://example.com, federated username")]
      performAction:grey_tap()];

  // Check that the Site, Username, Federation and Delete Saved Password
  // sections are there. (No scrolling for the first two, which should be high
  // enough to always start visible.)
  [[EarlGrey selectElementWithMatcher:SiteHeader()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:UsernameHeader()]
      assertWithMatcher:grey_sufficientlyVisible()];
  // For federation check both the section header and content.
  [[[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityTrait(UIAccessibilityTraitHeader),
                     grey_accessibilityLabel(l10n_util::GetNSString(
                         IDS_IOS_SHOW_PASSWORD_VIEW_FEDERATION)),
                     nullptr)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[[EarlGrey selectElementWithMatcher:grey_text(@"famous.provider.net")]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Not using DeleteButton() matcher here, because that also encodes the
  // relative position against the password section, which is missing in this
  // case.
  [[[EarlGrey selectElementWithMatcher:
                  ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                      IDS_IOS_SETTINGS_PASSWORD_DELETE_BUTTON))]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the password is not present.
  [[EarlGrey selectElementWithMatcher:PasswordHeader()]
      assertWithMatcher:grey_nil()];

  [self tapBackArrow];
  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

@end
