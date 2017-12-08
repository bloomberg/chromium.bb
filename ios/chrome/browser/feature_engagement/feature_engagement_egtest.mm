// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/wait_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The minimum number of times Chrome must be opened in order for an In Product
// Help feature to be shown.
const int kMinChromeOpensRequired = 5;

// The timeout for the load of the feature engagement tracker.
const NSTimeInterval kWaitForTrackerLoadTimeout = 10.0;

// Matcher for the Reading List Text Badge.
id<GREYMatcher> ReadingListTextBadge() {
  return grey_accessibilityID(@"kReadingListTextBadgeAccessibilityIdentifier");
}

// Simulate a Chrome Opened event for the Feature Engagement Tracker.
void SimulateChromeOpenedEvent() {
  feature_engagement::TrackerFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->NotifyEvent(feature_engagement::events::kChromeOpened);
}

// Loads the FeatureEngagementTracker.
void LoadFeatureEngagementTracker() {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
      browserState, feature_engagement::CreateFeatureEngagementTracker);

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browserState);
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kWaitForTrackerLoadTimeout,
                                           ^{
                                             return tracker->IsInitialized();
                                           }),
      @"Engagement Tracker did not load before timeout.");
}

// Enables the Badged Reading List help to be triggered for |feature_list|.
void EnableBadgedReadingListTriggering(
    base::test::ScopedFeatureList& feature_list) {
  std::map<std::string, std::string> badged_reading_list_params;

  badged_reading_list_params["event_1"] =
      "name:chrome_opened;comparator:>=5;window:90;storage:90";
  badged_reading_list_params["event_trigger"] =
      "name:badged_reading_list_trigger;comparator:==0;window:1095;storage:"
      "1095";
  badged_reading_list_params["event_used"] =
      "name:viewed_reading_list;comparator:==0;window:90;storage:90";
  badged_reading_list_params["session_rate"] = "==0";
  badged_reading_list_params["availability"] = "any";

  feature_list.InitAndEnableFeatureWithParameters(
      feature_engagement::kIPHBadgedReadingListFeature,
      badged_reading_list_params);
}

}  // namespace

// Tests related to the triggering of In Product Help features.
@interface FeatureEngagementTestCase : ChromeTestCase
@end

@implementation FeatureEngagementTestCase

// Verifies that the Badged Reading List feature shows when triggering
// conditions are met.
- (void)testBadgedReadingListFeatureShouldShow {
  base::test::ScopedFeatureList scoped_feature_list;

  EnableBadgedReadingListTriggering(scoped_feature_list);

  // Ensure that the FeatureEngagementTracker picks up the new feature
  // configuration provided by |scoped_feature_list|.
  LoadFeatureEngagementTracker();

  // Ensure that Chrome has been launched enough times for the Badged Reading
  // List to appear.
  for (int index = 0; index < kMinChromeOpensRequired; index++) {
    SimulateChromeOpenedEvent();
  }

  [ChromeEarlGreyUI openToolsMenu];

  [[EarlGrey selectElementWithMatcher:ReadingListTextBadge()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verifies that the Badged Reading List feature does not show if Chrome has
// not opened enough times.
- (void)testBadgedReadingListFeatureTooFewChromeOpens {
  base::test::ScopedFeatureList scoped_feature_list;

  EnableBadgedReadingListTriggering(scoped_feature_list);

  // Ensure that the FeatureEngagementTracker picks up the new feature
  // configuration provided by |scoped_feature_list|.
  LoadFeatureEngagementTracker();

  // Open Chrome just one time.
  SimulateChromeOpenedEvent();

  [ChromeEarlGreyUI openToolsMenu];

  [[EarlGrey selectElementWithMatcher:ReadingListTextBadge()]
      assertWithMatcher:grey_notVisible()];
}
@end
