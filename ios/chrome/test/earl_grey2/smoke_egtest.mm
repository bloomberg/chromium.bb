// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <TestLib/EarlGreyImpl/EarlGrey.h>
#import <UIKit/UIKit.h>

#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test case to verify that EarlGrey tests can be launched and perform basic
// UI interactions.
@interface SmokeTestCase : ChromeTestCase
@end

@implementation SmokeTestCase

// Tests that a tab can be opened.
- (void)testOpenTab {
  // Open tools menu.
  [ChromeEarlGreyUI openToolsMenu];

  // Open new tab.
  // TODO(crbug.com/917114): Calling the string directly is temporary while we
  // roll out a solution to access constants across the code base for EG2.
  id<GREYMatcher> newTabButtonMatcher =
      grey_accessibilityID(@"kToolsMenuNewTabId");
  [ChromeEarlGreyUI tapToolsMenuButton:newTabButtonMatcher];

  // Wait until tab opened and test if there're 2 tabs in total.
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that helpers from chrome_matchers.h are available for use in tests.
- (void)testTapToolsMenu {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];

  // Tap a second time to close the menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
}

// Tests that helpers from chrome_actions.h are available for use in tests.
- (void)testToggleSettingsSwitch {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuPasswordsButton()];

  // Toggle the passwords switch off and on.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"savePasswordsItem_switch")]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"savePasswordsItem_switch")]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Close the settings menu.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that helpers from chrome_earl_grey.h are available for use in tests.
- (void)testClearBrowsingHistory {
  [ChromeEarlGrey clearBrowsingHistory];
}

// Tests that string resources are loaded into the ResourceBundle and available
// for use in tests.
- (void)testAppResourcesArePresent {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];

  NSString* settingsLabel = l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(settingsLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap a second time to close the menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
}

// Tests that helpers in chrome_earl_grey_ui.h are available for use in tests.
- (void)testReload {
  [ChromeEarlGreyUI reload];
}

// Tests navigation-related converted helpers in chrome_earl_grey.h.
- (void)testURLNavigation {
  [ChromeEarlGrey loadURL:GURL("chrome://terms")];
  [ChromeEarlGrey reload];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey goForward];
}

// Tests tab open/close-related converted helpers in chrome_earl_grey.h.
- (void)testTabOpeningAndClosing {
  [ChromeEarlGrey closeAllTabsInCurrentMode];
  [ChromeEarlGrey closeAllIncognitoTabs];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  [ChromeEarlGrey closeAllTabsInCurrentMode];
  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey waitForMainTabCount:0];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  [ChromeEarlGrey openNewTab];
}

// Tests bookmark converted helpers in chrome_earl_grey.h.
- (void)testBookmarkHelpers {
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
}

// Tests helpers involving fake sync servers and autofill profiles in
// chrome_earl_grey.h
- (void)testAutofillProfileSyncToFakeServer {
  std::string fakeGUID = "b67e5ca1e09345d0aecfc2155c1f6b11";
  std::string profileName = "testAutofillProfileSyncToFakeServer";

  [ChromeEarlGrey clearAutofillProfileWithGUID:fakeGUID];
  GREYAssertTrue(![ChromeEarlGrey isAutofillProfilePresentWithGUID:fakeGUID
                                               autofillProfileName:profileName],
                 @"Autofill profile should not be present.");
  [ChromeEarlGrey addAutofillProfileToFakeSyncServerWithGUID:fakeGUID
                                         autofillProfileName:profileName];
}

// Tests waitForSufficientlyVisibleElementWithMatcher in chrome_earl_grey.h
- (void)testWaitForSufficientlyVisibleElementWithMatcher {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NewTabPageOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
}

// Tests sync server converted helpers in chrome_earl_grey.h.
- (void)testSyncServerHelpers {
  [ChromeEarlGrey startSync];
  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:10.0];
  [ChromeEarlGrey clearSyncServerData];
}

// Tests executeJavaScript:error: in chrome_earl_grey.h
- (void)testExecuteJavaScript {
  id actualResult = [ChromeEarlGrey executeJavaScript:@"0"];
  GREYAssertEqualObjects(@0, actualResult,
                         @"Actual JavaScript execution result: %@",
                         actualResult);
}

// Tests typed URL converted helpers in chrome_earl_grey.h.
- (void)testTypedURLHelpers {
  const GURL mockURL("http://not-a-real-site.test/");

  [ChromeEarlGrey addHistoryServiceTypedURL:mockURL];
  [ChromeEarlGrey deleteHistoryServiceTypedURL:mockURL];
}

// Tests accessibility util converted helper in chrome_earl_grey.h.
- (void)testAccessibilityUtil {
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests enabling/disabling features through [AppLaunchManager
// ensureAppLaunchedWithFeaturesEnabled]
- (void)testAppLaunchManagerLaunchWithFeatures {
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithFeaturesEnabled:{kTestFeature}
                                  disabled:{}
                            relaunchPolicy:NoForceRelaunchAndResetState];

  GREYAssertTrue([ChromeEarlGrey isTestFeatureEnabled],
                 @"kTestFeature should be enabled");

  GREYAssertEqual([ChromeEarlGrey mainTabCount], 1U,
                  @"Exactly one new tab should be opened.");
}

// Tests enabling variations and trigger variations through [AppLaunchManager
// ensureAppLaunchedWithLaunchConfiguration:]
- (void)testAppLaunchManagerLaunchWithVariations {
  AppLaunchConfiguration config;
  config.variations_enabled = {111111, 222222};
  config.trigger_variations_enabled = {999999, 777777};
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  GREYAssertTrue([ChromeEarlGrey isTriggerVariationEnabled:999999],
                 @"Trigger variation 123456 should be enabled");
  GREYAssertTrue([ChromeEarlGrey isTriggerVariationEnabled:777777],
                 @"Trigger variation 123456 should be enabled");
  GREYAssertTrue([ChromeEarlGrey isVariationEnabled:111111],
                 @"Variation 987654 should be enabled");
  GREYAssertTrue([ChromeEarlGrey isVariationEnabled:222222],
                 @"Variation 987654 should be enabled");

  GREYAssertEqual([ChromeEarlGrey mainTabCount], 1U,
                  @"Exactly one new tab should be opened.");
}

// Tests AppLaunchManager can pass an arbitrary arg to host app.
- (void)testAppLaunchManagerLaunchWithArbitraryArgs {
  AppLaunchConfiguration config;
  config.additional_args = {"-switch1", "--switch2", "--switch3=somevalue"};
  config.relaunch_policy = ForceRelaunchByKilling;

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  GREYAssertTrue([ChromeEarlGrey appHasLaunchSwitch:"switch1"],
                 @"switch1 should be in app launch switches.");
  GREYAssertTrue([ChromeEarlGrey appHasLaunchSwitch:"switch2"],
                 @"switch2 should be in app launch switches.");
  GREYAssertTrue([ChromeEarlGrey appHasLaunchSwitch:"switch3"],
                 @"switch3 should be in app launch switches.");

  GREYAssertFalse([ChromeEarlGrey appHasLaunchSwitch:"switch4"],
                  @"switch4 should not be in app launch switches.");
}

// Tests gracefully kill through AppLaunchManager.
- (void)testAppLaunchManagerForceRelaunchByCleanShutdown {
// TODO(crbug.com/1067821): ForceRelaunchByCleanShutdown only compiles and works
// on simulator.
#if TARGET_IPHONE_SIMULATOR
  [ChromeEarlGrey openNewTab];
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithFeaturesEnabled:{}
      disabled:{}
      relaunchPolicy:ForceRelaunchByCleanShutdown];
  [[EarlGrey selectElementWithMatcher:grey_text(@"Restore")]
      assertWithMatcher:grey_notVisible()];
#endif
}

// Tests hard kill(crash) through AppLaunchManager.
- (void)testAppLaunchManagerForceRelaunchByKilling {
  [ChromeEarlGrey openNewTab];
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithFeaturesEnabled:{}
      disabled:{}
      relaunchPolicy:ForceRelaunchByKilling];
  [[EarlGrey selectElementWithMatcher:grey_text(@"Restore")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests running resets after relaunch through AppLaunchManager.
- (void)testAppLaunchManagerNoForceRelaunchAndResetState {
  [self stopHTTPServer];
  [self disableMockAuthentication];
  [ChromeEarlGrey openNewTab];
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithFeaturesEnabled:{kTestFeature}
                                  disabled:{}
                            relaunchPolicy:NoForceRelaunchAndResetState];
  [ChromeEarlGrey waitForMainTabCount:1];
  // |stopHTTPServer| and |disableMockAuthentication| DCHECK the flags are in
  // correct states, which can serve as assertion that proper resets are run.
  [self stopHTTPServer];
  [self disableMockAuthentication];
}

// Tests no force relaunch.
- (void)testAppLaunchManagerNoForceRelaunchAndKeepState {
  [self stopHTTPServer];
  [self disableMockAuthentication];
  [ChromeEarlGrey openNewTab];
  // No relauch when feature list isn't changed.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithFeaturesEnabled:{}
      disabled:{}
      relaunchPolicy:NoForceRelaunchAndKeepState];
  [ChromeEarlGrey waitForMainTabCount:2];
  [[EarlGrey selectElementWithMatcher:grey_text(@"Restore")]
      assertWithMatcher:grey_notVisible()];
}

// Tests backgrounding app and moving app back through AppLaunchManager.
- (void)testAppLaunchManagerBackgroundAndForegroundApp {
  [ChromeEarlGrey openNewTab];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests isCompactWidth method in chrome_earl_grey.h.
- (void)testisCompactWidth {
  BOOL expectedIsCompactWidth =
      [[[[GREY_REMOTE_CLASS_IN_APP(UIApplication) sharedApplication] keyWindow]
          traitCollection] horizontalSizeClass] ==
      UIUserInterfaceSizeClassCompact;
  GREYAssertTrue([ChromeEarlGrey isCompactWidth] == expectedIsCompactWidth,
                 @"isCompactWidth should return %@",
                 expectedIsCompactWidth ? @"YES" : @"NO");
}

// Tests helpers that retrieve prefs and local state values.
- (void)testGetPrefs {
  // The actual pref names and values below are irrelevant, but the calls
  // themselves should return data without crashing or asserting.
  [ChromeEarlGrey localStateBooleanPref:prefs::kLastSessionExitedCleanly];
  [ChromeEarlGrey localStateIntegerPref:prefs::kBrowserStatesNumCreated];
  [ChromeEarlGrey localStateStringPref:prefs::kBrowserStateLastUsed];

  [ChromeEarlGrey userBooleanPref:prefs::kIosBookmarkPromoAlreadySeen];
  [ChromeEarlGrey userIntegerPref:prefs::kIosBookmarkCachedTopMostRow];
  [ChromeEarlGrey userStringPref:prefs::kDefaultCharset];
}

@end
