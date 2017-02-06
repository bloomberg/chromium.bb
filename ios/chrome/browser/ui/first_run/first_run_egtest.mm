// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller+Testing.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#import "ios/chrome/browser/geolocation/test_location_manager.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/first_run/first_run_chrome_signin_view_controller.h"
#include "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns a fake identity.
ChromeIdentity* GetFakeIdentity() {
  return [FakeChromeIdentity identityWithEmail:@"foo@gmail.com"
                                        gaiaID:@"fooID"
                                          name:@"Fake Foo"];
}

// Taps the button with accessibility labelId |message_id|.
void TapButtonWithLabelId(int message_id) {
  id<GREYMatcher> matcher = chrome_test_util::ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(message_id));
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

// Asserts that |identity| is actually signed in to the active profile.
void AssertAuthenticatedIdentityInActiveProfile(ChromeIdentity* identity) {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  AccountInfo info =
      ios::SigninManagerFactory::GetForBrowserState(browser_state)
          ->GetAuthenticatedAccountInfo();

  GREYAssertEqual(base::SysNSStringToUTF8(identity.gaiaID), info.gaia,
                  @"Unexpected Gaia ID of the signed in user [expected = "
                  @"\"%@\", actual = \"%s\"]",
                  identity.gaiaID, info.gaia.c_str());
}
}

@interface MainController (ExposedForTesting)
- (void)showFirstRunUI;
@end

// Tests first run settings and navigation.
@interface FirstRunTestCase : ChromeTestCase
@end

@implementation FirstRunTestCase

- (void)setUp {
  [super setUp];

  BooleanPrefMember metricsEnabledPref;
  metricsEnabledPref.Init(metrics::prefs::kMetricsReportingEnabled,
                          GetApplicationContext()->GetLocalState());
  metricsEnabledPref.SetValue(NO);
  IntegerPrefMember defaultOptInPref;
  defaultOptInPref.Init(metrics::prefs::kMetricsDefaultOptIn,
                        GetApplicationContext()->GetLocalState());
  defaultOptInPref.SetValue(metrics::EnableMetricsDefault::DEFAULT_UNKNOWN);

  TestLocationManager* locationManager = [[TestLocationManager alloc] init];
  [locationManager setLocationServicesEnabled:NO];
  [[OmniboxGeolocationController sharedInstance]
      setLocationManager:locationManager];
}

+ (void)tearDown {
  IntegerPrefMember defaultOptInPref;
  defaultOptInPref.Init(metrics::prefs::kMetricsDefaultOptIn,
                        GetApplicationContext()->GetLocalState());
  defaultOptInPref.SetValue(metrics::EnableMetricsDefault::DEFAULT_UNKNOWN);

  [[OmniboxGeolocationController sharedInstance] setLocationManager:nil];

  [super tearDown];
}

// Navigates to the terms of service and back.
- (void)testTermsAndConditions {
  [chrome_test_util::GetMainController() showFirstRunUI];

  id<GREYMatcher> termsOfServiceLink =
      grey_accessibilityLabel(@"Terms of Service");
  [[EarlGrey selectElementWithMatcher:termsOfServiceLink]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_text(@"Chromium Terms of Service")]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"back_bar_button")]
      performAction:grey_tap()];

  // Ensure we went back to the First Run screen.
  [[EarlGrey selectElementWithMatcher:termsOfServiceLink]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Toggle the UMA checkbox.
- (void)testToggleMetricsOn {
  [chrome_test_util::GetMainController() showFirstRunUI];

  id<GREYMatcher> metrics =
      grey_accessibilityID(kUMAMetricsButtonAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:metrics] performAction:grey_tap()];

  id<GREYMatcher> optInAccept = chrome_test_util::ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_FIRSTRUN_OPT_IN_ACCEPT_BUTTON));
  [[EarlGrey selectElementWithMatcher:optInAccept] performAction:grey_tap()];

  BOOL metricsOptIn = GetApplicationContext()->GetLocalState()->GetBoolean(
      metrics::prefs::kMetricsReportingEnabled);
  GREYAssert(
      metricsOptIn != [WelcomeToChromeViewController defaultStatsCheckboxValue],
      @"Metrics reporting pref is incorrect.");
}

// Dismisses the first run screens.
- (void)testDismissFirstRun {
  [chrome_test_util::GetMainController() showFirstRunUI];

  id<GREYMatcher> optInAccept = chrome_test_util::ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_FIRSTRUN_OPT_IN_ACCEPT_BUTTON));
  [[EarlGrey selectElementWithMatcher:optInAccept] performAction:grey_tap()];

  PrefService* preferences = GetApplicationContext()->GetLocalState();
  GREYAssert(
      preferences->GetBoolean(metrics::prefs::kMetricsReportingEnabled) ==
          [WelcomeToChromeViewController defaultStatsCheckboxValue],
      @"Metrics reporting does not match.");

  id<GREYMatcher> skipSignIn =
      grey_accessibilityID(kSignInSkipButtonAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:skipSignIn] performAction:grey_tap()];

  id<GREYMatcher> newTab =
      grey_kindOfClass(NSClassFromString(@"NewTabPageView"));
  [[EarlGrey selectElementWithMatcher:newTab]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Signs in to an account and then taps the Undo button to sign out.
- (void)testSignInAndUndo {
  ChromeIdentity* identity = GetFakeIdentity();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Launch First Run and accept tems of services.
  [chrome_test_util::GetMainController() showFirstRunUI];
  TapButtonWithLabelId(IDS_IOS_FIRSTRUN_OPT_IN_ACCEPT_BUTTON);

  // Sign In |identity|.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity);

  // Undo the sign-in and dismiss the Sign In screen.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_UNDO_BUTTON);
  TapButtonWithLabelId(IDS_IOS_FIRSTRUN_ACCOUNT_CONSISTENCY_SKIP_BUTTON);

  // |identity| shouldn't be signed in.
  AssertAuthenticatedIdentityInActiveProfile(nil);
}

// Signs in to an account and then taps the Advanced link to go to settings.
- (void)testSignInAndTapSettingsLink {
  ChromeIdentity* identity = GetFakeIdentity();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Launch First Run and accept tems of services.
  [chrome_test_util::GetMainController() showFirstRunUI];
  TapButtonWithLabelId(IDS_IOS_FIRSTRUN_OPT_IN_ACCEPT_BUTTON);

  // Sign In |identity|.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity);

  // Tap Settings link.
  id<GREYMatcher> settings_link_matcher = grey_allOf(
      grey_accessibilityLabel(@"Settings"), grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:settings_link_matcher]
      performAction:grey_tap()];

  // Check Sync hasn't started yet, allowing the user to change somes settings.
  SyncSetupService* sync_service = SyncSetupServiceFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState());
  GREYAssertFalse(sync_service->HasFinishedInitialSetup(),
                  @"Sync shouldn't have finished its original setup yet");

  // Close Settings, user is still signed in and sync is now starting.
  TapButtonWithLabelId(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
  AssertAuthenticatedIdentityInActiveProfile(identity);
  GREYAssertTrue(sync_service->HasFinishedInitialSetup(),
                 @"Sync should have finished its original setup");
}

@end
