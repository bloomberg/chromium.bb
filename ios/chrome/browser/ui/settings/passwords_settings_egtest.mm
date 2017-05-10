// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
      grey_interactable(),
      ButtonWithAccessibilityLabel(
          [NSString stringWithFormat:@"%@: %@",
                                     l10n_util::GetNSString(
                                         IDS_IOS_SHOW_PASSWORD_VIEW_SITE),
                                     l10n_util::GetNSString(
                                         IDS_IOS_SETTINGS_SITE_COPY_BUTTON)]),
      grey_layout(@[ Below() ], SiteHeader()),
      grey_layout(@[ Above() ], UsernameHeader()),
      grey_layout(@[ Above() ], PasswordHeader()), nullptr);
}

// Matcher for the Copy username button in Password Details view.
id<GREYMatcher> CopyUsernameButton() {
  return grey_allOf(
      grey_interactable(),
      ButtonWithAccessibilityLabel([NSString
          stringWithFormat:@"%@: %@",
                           l10n_util::GetNSString(
                               IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME),
                           l10n_util::GetNSString(
                               IDS_IOS_SETTINGS_USERNAME_COPY_BUTTON)]),
      grey_layout(@[ Below() ], SiteHeader()),
      grey_layout(@[ Below() ], UsernameHeader()),
      grey_layout(@[ Above() ], PasswordHeader()), nullptr);
}

// Matcher for the Copy password button in Password Details view.
id<GREYMatcher> CopyPasswordButton() {
  return grey_allOf(
      grey_interactable(),
      ButtonWithAccessibilityLabel([NSString
          stringWithFormat:@"%@: %@",
                           l10n_util::GetNSString(
                               IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD),
                           l10n_util::GetNSString(
                               IDS_IOS_SETTINGS_PASSWORD_COPY_BUTTON)]),
      grey_layout(@[ Below() ], SiteHeader()),
      grey_layout(@[ Below() ], UsernameHeader()),
      grey_layout(@[ Below() ], PasswordHeader()), nullptr);
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

// Saves an example form in the store.
- (void)saveExamplePasswordForm {
  PasswordForm example;
  example.username_value = base::ASCIIToUTF16("user");
  example.password_value = base::ASCIIToUTF16("password");
  example.origin = GURL("https://example.com");
  example.signon_realm = example.origin.spec();

  [self passwordStore]->AddLogin(example);
  // Allow the PasswordStore to process this on the DB thread.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
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

  // Wait for UI components to finish loading.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Tap back arrow, to get one level higher in settings.
- (void)tapBackArrow {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"ic_arrow_back"),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];

  // Wait for UI components to finish loading.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Tap Edit in any settings view.
- (void)tapEdit {
  [[EarlGrey selectElementWithMatcher:EditButton()] performAction:grey_tap()];

  // Wait for UI components to finish loading.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Tap Done in any settings view.
- (void)tapDone {
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Wait for UI components to finish loading.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
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
  [[EarlGrey selectElementWithMatcher:Entry(@"https://example.com, user")]
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

  [[EarlGrey selectElementWithMatcher:Entry(@"https://example.com, user")]
      performAction:grey_tap()];

  // Get the PasswordDetailsCollectionViewController and replace the
  // reauthentication module with a fake one to avoid being blocked with a
  // reauth prompt.
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

  // Check the snackbar in case of successful reauthentication.
  mock_reauthentication_module.shouldSucceed = YES;
  [[[EarlGrey selectElementWithMatcher:CopyPasswordButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];
  // Wait until the fade-out animation completes.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Check the snackbar in case of failed reauthentication.
  mock_reauthentication_module.shouldSucceed = NO;
  [[[EarlGrey selectElementWithMatcher:CopyPasswordButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_NOT_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];
  // Wait until the fade-out animation completes.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

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

  [[EarlGrey selectElementWithMatcher:Entry(@"https://example.com, user")]
      performAction:grey_tap()];

  // Check the snackbar.
  [[[EarlGrey selectElementWithMatcher:CopyUsernameButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];
  // Wait until the fade-out animation completes.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

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

  [[EarlGrey selectElementWithMatcher:Entry(@"https://example.com, user")]
      performAction:grey_tap()];

  // Check the snackbar.
  [[[EarlGrey selectElementWithMatcher:CopySiteButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];
  // Wait until the fade-out animation completes.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  [self tapBackArrow];
  [self tapBackArrow];
  [self tapDone];
  [self clearPasswordStore];
}

@end
