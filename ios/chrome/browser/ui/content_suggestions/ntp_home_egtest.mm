// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/test/scoped_command_line.h"
#include "components/reading_list/core/reading_list_model.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory_util.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_provider_test_singleton.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using namespace content_suggestions;
using namespace ntp_home;
using namespace ntp_snippets;

namespace {
// Returns a matcher, which is true if the view has its width equals to |width|.
id<GREYMatcher> OmniboxWidth(CGFloat width) {
  MatchesBlock matches = ^BOOL(UIView* view) {
    return view.bounds.size.width == width;
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description
        appendText:[NSString stringWithFormat:@"Omnibox has correct width: %g",
                                              width]];
  };

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

// Returns a matcher, which is true if the view has its width equals to |width|
// plus or minus |margin|.
id<GREYMatcher> OmniboxWidthBetween(CGFloat width, CGFloat margin) {
  MatchesBlock matches = ^BOOL(UIView* view) {
    return view.bounds.size.width >= width - margin &&
           view.bounds.size.width <= width + margin;
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description
        appendText:[NSString
                       stringWithFormat:
                           @"Omnibox has correct width: %g with margin: %g",
                           width, margin]];
  };

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

// Returns the subview of |parentView| corresponding to the
// ContentSuggestionsViewController. Returns nil if it is not in its subviews.
UIView* SubviewWithCollectionViewIdentifier(UIView* parentView) {
  if (parentView.accessibilityIdentifier ==
      [ContentSuggestionsViewController collectionAccessibilityIdentifier]) {
    return parentView;
  }
  if (parentView.subviews.count == 0)
    return nil;
  for (UIView* view in parentView.subviews) {
    UIView* resultView = SubviewWithCollectionViewIdentifier(view);
    if (resultView)
      return resultView;
  }
  return nil;
}

// Returns the view corresponding to the ContentSuggestionsViewController.
// Returns nil if it is not in the view hierarchy.
UIView* CollectionView() {
  return SubviewWithCollectionViewIdentifier(
      [[UIApplication sharedApplication] keyWindow]);
}
}  // namespace

// Test case for the NTP home UI. More precisely, this tests the positions of
// the elements after interacting with the device.
@interface NTPHomeTestCase : ChromeTestCase {
  std::unique_ptr<base::test::ScopedCommandLine> _scopedCommandLine;
}

// Current non-incognito browser state.
@property(nonatomic, assign, readonly) ios::ChromeBrowserState* browserState;
// Mock provider from the singleton.
@property(nonatomic, assign, readonly) MockContentSuggestionsProvider* provider;
// Article category, used by the singleton.
@property(nonatomic, assign, readonly) Category category;

@end

@implementation NTPHomeTestCase

+ (void)setUp {
  [super setUp];
  [self closeAllTabs];
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  // Sets the ContentSuggestionsService associated with this browserState to a
  // service with no provider registered, allowing to register fake providers
  // which do not require internet connection. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState, CreateChromeContentSuggestionsService);

  ContentSuggestionsService* service =
      IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
          browserState);
  RegisterReadingListProvider(service, browserState);
  [[ContentSuggestionsTestSingleton sharedInstance]
      registerArticleProvider:service];
}

+ (void)tearDown {
  [self closeAllTabs];
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  ReadingListModelFactory::GetForBrowserState(browserState)->DeleteAllEntries();

  // Resets the Service associated with this browserState to a service with
  // default providers. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState, CreateChromeContentSuggestionsServiceWithProviders);
  [super tearDown];
}

- (void)setUp {
  // The command line is set up before [super setUp] in order to have the NTP
  // opened with the command line already setup.
  _scopedCommandLine = base::MakeUnique<base::test::ScopedCommandLine>();
  base::CommandLine* commandLine = _scopedCommandLine->GetProcessCommandLine();
  commandLine->AppendSwitch(switches::kEnableSuggestionsUI);
  self.provider->FireCategoryStatusChanged(self.category,
                                           CategoryStatus::AVAILABLE);

  ReadingListModel* readingListModel =
      ReadingListModelFactory::GetForBrowserState(self.browserState);
  readingListModel->DeleteAllEntries();
  [super setUp];
}

- (void)tearDown {
  self.provider->FireCategoryStatusChanged(
      self.category, CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED);
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                           errorOrNil:nil];
  _scopedCommandLine.reset();
  [super tearDown];
}

#pragma mark - Properties

- (ios::ChromeBrowserState*)browserState {
  return chrome_test_util::GetOriginalBrowserState();
}

- (MockContentSuggestionsProvider*)provider {
  return [[ContentSuggestionsTestSingleton sharedInstance] provider];
}

- (Category)category {
  return Category::FromKnownCategory(KnownCategories::ARTICLES);
}

#pragma mark - Tests

// Tests that all items are accessible on the home page.
- (void)testAccessibility {
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that the fake omnibox width is correctly updated after a rotation.
- (void)testOmniboxWidthRotation {
  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  CGFloat collectionWidth = CollectionView().bounds.size.width;
  GREYAssertTrue(collectionWidth > 0, @"The collection width is nil.");
  CGFloat fakeOmniboxWidth = searchFieldWidth(collectionWidth);

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          FakeOmniboxAccessibilityID())]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                           errorOrNil:nil];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  CGFloat collectionWidthAfterRotation = CollectionView().bounds.size.width;
  GREYAssertNotEqual(collectionWidth, collectionWidthAfterRotation,
                     @"The collection width has not changed.");
  fakeOmniboxWidth = searchFieldWidth(collectionWidthAfterRotation);

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          FakeOmniboxAccessibilityID())]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];
}

// Tests that the fake omnibox width is correctly updated after a rotation done
// while the settings screen is shown.
- (void)testOmniboxWidthRotationBehindSettings {
  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  CGFloat collectionWidth = CollectionView().bounds.size.width;
  GREYAssertTrue(collectionWidth > 0, @"The collection width is nil.");
  CGFloat fakeOmniboxWidth = searchFieldWidth(collectionWidth);

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          FakeOmniboxAccessibilityID())]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];

  [ChromeEarlGreyUI openSettingsMenu];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                           errorOrNil:nil];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)]
      performAction:grey_tap()];

  CGFloat collectionWidthAfterRotation = CollectionView().bounds.size.width;
  GREYAssertNotEqual(collectionWidth, collectionWidthAfterRotation,
                     @"The collection width has not changed.");
  fakeOmniboxWidth = searchFieldWidth(collectionWidthAfterRotation);

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          FakeOmniboxAccessibilityID())]
      assertWithMatcher:OmniboxWidth(fakeOmniboxWidth)];
}

// Tests that the fake omnibox width is correctly updated after a rotation done
// while the fake omnibox is pinned to the top.
- (void)testOmniboxPinnedWidthRotation {
  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   [ContentSuggestionsViewController
                                       collectionAccessibilityIdentifier])]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  CGFloat collectionWidth = CollectionView().bounds.size.width;
  GREYAssertTrue(collectionWidth > 0, @"The collection width is nil.");

  // The fake omnibox might be slightly bigger than the screen in order to cover
  // it for all screen scale.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          FakeOmniboxAccessibilityID())]
      assertWithMatcher:OmniboxWidthBetween(collectionWidth + 1, 1)];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                           errorOrNil:nil];

  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  CGFloat collectionWidthAfterRotation = CollectionView().bounds.size.width;
  GREYAssertNotEqual(collectionWidth, collectionWidthAfterRotation,
                     @"The collection width has not changed.");

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          FakeOmniboxAccessibilityID())]
      assertWithMatcher:OmniboxWidthBetween(collectionWidthAfterRotation + 1,
                                            1)];
}

@end
