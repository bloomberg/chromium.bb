// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/mac/bind_objc_block.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder.h"
#import "ios/chrome/browser/ui/settings/privacy_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/histogram_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_assertions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#import "ios/web/public/test/http_server.h"
#import "ios/web/public/test/http_server_util.h"
#include "ios/web/public/test/response_providers/delayed_response_provider.h"
#include "ios/web/public/test/response_providers/html_response_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kTestUrl1[] =
    "http://ios/testing/data/http_server_files/memory_usage.html";
const char kURL1FirstWord[] = "Page";
const char kTestUrl2[] =
    "http://ios/testing/data/http_server_files/fullscreen.html";
const char kURL2FirstWord[] = "Rugby";
const char kClearPageScript[] = "document.body.innerHTML='';";

// The delay to use to serve slow URLs.
const CGFloat kSlowURLDelay = 3;

// The delay to wait for an element to appear before tapping on it.
const CGFloat kWaitElementTimeout = 3;

void ResetTabUsageRecorder() {
  GREYAssertTrue(chrome_test_util::ResetTabUsageRecorder(),
                 @"Fail to reset the TabUsageRecorder");
}

// Wait until |matcher| is accessible (not nil).
void Wait(id<GREYMatcher> matcher, NSString* name) {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()
                                                             error:&error];
    return error == nil;
  };
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kWaitElementTimeout, condition),
      @"Waiting for matcher %@ failed.", name);
}

// Wait until |matcher| is accessible (not nil) and tap on it.
void WaitAndTap(id<GREYMatcher> matcher, NSString* name) {
  Wait(matcher, name);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

// Creates a new main tab and load |url|. Wait until |word| is visible on the
// page.
void NewMainTabWithURL(const GURL& url, const std::string& word) {
  int number_of_tabs = chrome_test_util::GetMainTabCount();
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey loadURL:url];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebViewContainingText(word)]
      assertWithMatcher:grey_notNil()];
  chrome_test_util::AssertMainTabCount(number_of_tabs + 1);
}

// Opens 2 new tabs with different URLs.
void OpenTwoTabs() {
  chrome_test_util::CloseAllTabsInCurrentMode();

  const GURL url1 = web::test::HttpServer::MakeUrl(kTestUrl1);
  const GURL url2 = web::test::HttpServer::MakeUrl(kTestUrl2);
  NewMainTabWithURL(url1, kURL1FirstWord);
  NewMainTabWithURL(url2, kURL2FirstWord);
}

// Opens a new main tab using the UI. This method is using ad-hoc
// synchronization.
void OpenNewMainTabUsingUIUnsynced() {
  int nb_main_tab = chrome_test_util::GetMainTabCount();
  id<GREYMatcher> tool_menu_matcher =
      grey_accessibilityID(kToolbarToolsMenuButtonIdentifier);
  WaitAndTap(tool_menu_matcher, @"Tool menu");
  id<GREYMatcher> new_main_tab_button_matcher =
      grey_accessibilityID(kToolsMenuNewTabId);
  WaitAndTap(new_main_tab_button_matcher, @"New tab button");

  chrome_test_util::AssertMainTabCount(nb_main_tab + 1);
}

// Opens a new incognito tab using the UI and evicts any main tab model tabs.
void OpenNewIncognitoTabUsingUIAndEvictMainTabs() {
  int nb_incognito_tab = chrome_test_util::GetIncognitoTabCount();
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> new_incognito_tab_button_matcher =
      grey_accessibilityID(kToolsMenuNewIncognitoTabId);
  [[EarlGrey selectElementWithMatcher:new_incognito_tab_button_matcher]
      performAction:grey_tap()];
  chrome_test_util::AssertIncognitoTabCount(nb_incognito_tab + 1);
  ConditionBlock condition = ^bool {
    return chrome_test_util::IsIncognitoMode();
  };
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kWaitElementTimeout, condition),
      @"Waiting switch to incognito mode.");
  chrome_test_util::EvictOtherTabModelTabs();
}

// Closes a tab in the current tab model. Synchronize on tab number afterwards.
void CloseTabAtIndexAndSync(NSUInteger i) {
  NSUInteger nb_main_tab = chrome_test_util::GetMainTabCount();
  chrome_test_util::CloseTabAtIndex(i);
  ConditionBlock condition = ^{
    return chrome_test_util::GetMainTabCount() == (nb_main_tab - 1);
  };
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kWaitElementTimeout, condition),
      @"Waiting for tab to close");
}

// Closes the tabs switcher.
void CloseTabSwitcher() {
  id<GREYMatcher> matcher = chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_TAB_STRIP_LEAVE_TAB_SWITCHER);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

// Swithches to normal mode using swith button (iPad) or stack view (iPhone).
// Assumes current mode is Incognito.
void SwitchToNormalMode() {
  GREYAssertTrue(chrome_test_util::IsIncognitoMode(),
                 @"Switching to normal mode is only allowed from Incognito.");
  if (IsIPadIdiom()) {
    // Enter the tab switcher.
    id<GREYMatcher> tabSwitcherEnterButton = grey_accessibilityLabel(
        l10n_util::GetNSStringWithFixup(IDS_IOS_TAB_STRIP_ENTER_TAB_SWITCHER));
    [[EarlGrey selectElementWithMatcher:tabSwitcherEnterButton]
        performAction:grey_tap()];

    // Select the non incognito panel.
    id<GREYMatcher> tabSwitcherHeaderPanelButton =
        grey_accessibilityLabel(l10n_util::GetNSStringWithFixup(
            IDS_IOS_TAB_SWITCHER_HEADER_NON_INCOGNITO_TABS));
    [[EarlGrey selectElementWithMatcher:tabSwitcherHeaderPanelButton]
        performAction:grey_tap()];

    // Leave the tab switcher.
    CloseTabSwitcher();
  } else {
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_IOS_TOOLBAR_SHOW_TABS)] performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB)]
        performAction:grey_swipeSlowInDirection(kGREYDirectionRight)];
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_IOS_TOOLBAR_SHOW_TABS)] performAction:grey_tap()];
  }
  ConditionBlock condition = ^bool {
    return !chrome_test_util::IsIncognitoMode();
  };
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kWaitElementTimeout, condition),
      @"Waiting switch to normal mode.");
}

// Check that the error page is visible.
void CheckErrorPageIsVisible() {
  // The DNS error page is static HTML content.
  NSString* const kError =
      l10n_util::GetNSString(IDS_ERRORPAGES_HEADING_NOT_AVAILABLE);
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::StaticHtmlViewContainingText(
                                   kError)] assertWithMatcher:grey_notNil()];
}

// Open the settings submenu. Assumes that settings menu is visible.
void OpenSettingsSubMenuUnsynced(int submenu) {
  id<GREYMatcher> settings_button_matcher =
      grey_text(l10n_util::GetNSString(submenu));
  [[[EarlGrey selectElementWithMatcher:settings_button_matcher]
         usingSearchAction:grey_swipeSlowInDirection(kGREYDirectionUp)
      onElementWithMatcher:grey_accessibilityID(kSettingsCollectionViewId)]
      performAction:grey_tap()];
}

// Open the settings menu. Wait for the settings menu to appear.
void OpenSettingsMenuUnsynced() {
  id<GREYMatcher> tool_menu_matcher =
      grey_accessibilityID(kToolbarToolsMenuButtonIdentifier);
  WaitAndTap(tool_menu_matcher, @"Tool menu");

  id<GREYMatcher> settings_button_matcher =
      grey_accessibilityID(kToolsMenuSettingsId);

  WaitAndTap(settings_button_matcher, @"Settings menu");
  Wait(grey_accessibilityID(kSettingsCollectionViewId), @"Setting view");
}

// Select the tab with title |title| using UI (tab strip on iPad, stack view on
// iPhone).
void SelectTabUsingUI(NSString* title) {
  if (IsCompact()) {
    WaitAndTap(chrome_test_util::ButtonWithAccessibilityLabelId(
                   IDS_IOS_TOOLBAR_SHOW_TABS),
               @"Tab switcher");
  }
  WaitAndTap(grey_text(title),
             [NSString stringWithFormat:@"tab with title %@", title]);
}
}  // namespace

// Test for the TabUsageRecorder class.
@interface TabUsageRecorderTestCase : ChromeTestCase
@end

@implementation TabUsageRecorderTestCase

- (void)tearDown {
  [[GREYConfiguration sharedInstance]
          setValue:@(YES)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
  [super tearDown];
}

// Tests that the recorder actual recorde tab state.
- (void)testTabSwitchRecorder {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  ResetTabUsageRecorder();
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // Open two tabs with urls.
  OpenTwoTabs();
  chrome_test_util::AssertMainTabCount(2);
  // Switch between the two tabs.  Both are currently in memory.
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);

  // Verify that one in-memory tab switch has been recorded.
  // histogramTester.ExpectTotalCount(kSelectedTabHistogramName, 1,
  // failureBlock);
  histogramTester.ExpectUniqueSample(
      kSelectedTabHistogramName, TabUsageRecorder::IN_MEMORY, 1, failureBlock);

  // Evict the tab.
  OpenNewIncognitoTabUsingUIAndEvictMainTabs();
  GREYAssertTrue(chrome_test_util::IsIncognitoMode(),
                 @"Failed to switch to incognito mode");

  // Switch back to the normal tabs. Should be on tab one.
  SwitchToNormalMode();
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                          kURL1FirstWord)]
      assertWithMatcher:grey_notNil()];

  histogramTester.ExpectTotalCount(kSelectedTabHistogramName, 2, failureBlock);
  histogramTester.ExpectBucketCount(kSelectedTabHistogramName,
                                    TabUsageRecorder::EVICTED, 1, failureBlock);
}

// Verifies the UMA metric for page loads before a tab eviction by loading
// some tabs, forcing a tab eviction, then checking the histogram.
- (void)testPageLoadCountBeforeEvictedTab {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  ResetTabUsageRecorder();
  const GURL url1 = web::test::HttpServer::MakeUrl(kTestUrl1);
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // This test opens three tabs.
  const int numberOfTabs = 3;
  chrome_test_util::CloseAllTabsInCurrentMode();
  // Open three tabs with http:// urls.
  for (NSUInteger i = 0; i < numberOfTabs; i++) {
    chrome_test_util::OpenNewTab();
    [ChromeEarlGrey loadURL:url1];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                            kURL1FirstWord)]
        assertWithMatcher:grey_notNil()];
  }
  chrome_test_util::AssertMainTabCount(numberOfTabs);

  // Switch between the tabs. They are currently in memory.
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);

  // Verify that no page-load count has been recorded.
  histogramTester.ExpectTotalCount(kPageLoadsBeforeEvictedTabSelected, 0,
                                   failureBlock);

  // Reload each tab.
  for (NSUInteger i = 0; i < numberOfTabs; i++) {
    chrome_test_util::SelectTabAtIndexInCurrentMode(i);
    // Clear the page so that we can check when page reload is complete.
    __block bool finished = false;
    chrome_test_util::GetCurrentWebState()->ExecuteJavaScript(
        base::UTF8ToUTF16(kClearPageScript),
        base::BindBlockArc(^(const base::Value*) {
          finished = true;
        }));

    GREYAssert(testing::WaitUntilConditionOrTimeout(1.0,
                                                    ^{
                                                      return finished;
                                                    }),
               @"JavaScript to reload each tab did not finish");
    [ChromeEarlGreyUI reload];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                            kURL1FirstWord)]
        assertWithMatcher:grey_notNil()];
  }

  // Evict the tab. Create a dummy tab so that switching back to normal mode
  // does not trigger a reload immediatly.
  chrome_test_util::OpenNewTab();
  OpenNewIncognitoTabUsingUIAndEvictMainTabs();
  chrome_test_util::AssertIncognitoTabCount(1);

  // Switch back to the normal tabs. Should be on tab one.
  SwitchToNormalMode();
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                          kURL1FirstWord)]
      assertWithMatcher:grey_notNil()];

  // Verify that one page-load count has been recorded. It should contain two
  // page loads for each tab created.
  histogramTester.ExpectTotalCount(kPageLoadsBeforeEvictedTabSelected, 1,
                                   failureBlock);

  std::unique_ptr<base::HistogramSamples> samples =
      histogramTester.GetHistogramSamplesSinceCreation(
          kPageLoadsBeforeEvictedTabSelected);
  int sampleSum = samples ? samples->sum() : 0;
  GREYAssertEqual(sampleSum, numberOfTabs * 2,
                  @"Expected page loads is %d, actual %d.", numberOfTabs * 2,
                  sampleSum);
}

// Tests that tabs reloaded on cold start are reported as
// EVICTED_DUE_TO_COLD_START.
- (void)testColdLaunchReloadCount {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  ResetTabUsageRecorder();

  // Open two tabs with urls.
  OpenTwoTabs();
  chrome_test_util::AssertMainTabCount(2);
  // Set the normal tabs as 'cold start' tabs.
  GREYAssertTrue(chrome_test_util::SetCurrentTabsToBeColdStartTabs(),
                 @"Fail to state tabs as cold start tabs");

  // Open two incognito tabs with urls, clearing normal tabs from memory.
  OpenNewIncognitoTabUsingUIAndEvictMainTabs();
  OpenNewIncognitoTabUsingUIAndEvictMainTabs();
  chrome_test_util::AssertIncognitoTabCount(2);

  // Switch back to the normal tabs.
  SwitchToNormalMode();
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                          kURL2FirstWord)]
      assertWithMatcher:grey_notNil()];

  // Select the other one so it also reloads.
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                          kURL1FirstWord)]
      assertWithMatcher:grey_notNil()];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  // Make sure that one of the 2 tab loads (excluding the selected tab) is
  // counted as a cold start eviction.
  histogramTester.ExpectBucketCount(kSelectedTabHistogramName,
                                    TabUsageRecorder::EVICTED_DUE_TO_COLD_START,
                                    1, failureBlock);

  histogramTester.ExpectBucketCount(
      kSelectedTabHistogramName, TabUsageRecorder::IN_MEMORY, 0, failureBlock);
  // Re-select the same tab and make sure it is not counted again as evicted.
  chrome_test_util::SelectTabAtIndexInCurrentMode(1);
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                          kURL1FirstWord)]
      assertWithMatcher:grey_notNil()];
  histogramTester.ExpectBucketCount(kSelectedTabHistogramName,
                                    TabUsageRecorder::EVICTED_DUE_TO_COLD_START,
                                    1, failureBlock);

  histogramTester.ExpectBucketCount(
      kSelectedTabHistogramName, TabUsageRecorder::IN_MEMORY, 2, failureBlock);
}

// Tests that tabs reloads after backgrounding and eviction.
- (void)testBackgroundingReloadCount {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  ResetTabUsageRecorder();
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // Open two tabs with urls.
  OpenTwoTabs();
  chrome_test_util::AssertMainTabCount(2);

  // Simulate going into the background.
  GREYAssertTrue(chrome_test_util::SimulateTabsBackgrounding(),
                 @"Fail to simulate tab backgrounding.");

  // Open incognito and clear normal tabs from memory.
  OpenNewIncognitoTabUsingUIAndEvictMainTabs();
  GREYAssertTrue(chrome_test_util::IsIncognitoMode(),
                 @"Failed to switch to incognito mode");
  histogramTester.ExpectTotalCount(kEvictedTabReloadTime, 0, failureBlock);

  // Switch back to the normal tabs.
  SwitchToNormalMode();
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                          kURL2FirstWord)]
      assertWithMatcher:grey_notNil()];

  const GURL url1 = web::test::HttpServer::MakeUrl(kTestUrl1);
  const GURL url2 = web::test::HttpServer::MakeUrl(kTestUrl2);
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(url2.GetContent())]
      assertWithMatcher:grey_notNil()];
  histogramTester.ExpectTotalCount(kEvictedTabReloadTime, 1, failureBlock);

  chrome_test_util::SelectTabAtIndexInCurrentMode(0);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                          kURL1FirstWord)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(url1.GetContent())]
      assertWithMatcher:grey_notNil()];
  histogramTester.ExpectTotalCount(kEvictedTabReloadTime, 2, failureBlock);
}

// Verify correct recording of metrics when the reloading of an evicted tab
// succeeds.
- (void)testEvictedTabReloadSuccess {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  GURL URL = web::test::HttpServer::MakeUrl(kTestUrl1);
  NewMainTabWithURL(URL, kURL1FirstWord);
  OpenNewIncognitoTabUsingUIAndEvictMainTabs();
  SwitchToNormalMode();
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                          kURL1FirstWord)]
      assertWithMatcher:grey_notNil()];

  histogramTester.ExpectUniqueSample(kEvictedTabReloadSuccessRate,
                                     TabUsageRecorder::LOAD_SUCCESS, 1,
                                     failureBlock);
  histogramTester.ExpectUniqueSample(kDidUserWaitForEvictedTabReload,
                                     TabUsageRecorder::USER_WAITED, 1,
                                     failureBlock);
  histogramTester.ExpectTotalCount(kEvictedTabReloadTime, 1, failureBlock);
}

// Verify correct recording of metrics when the reloading of an evicted tab
// fails.
- (void)testEvictedTabReloadFailure {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // This URL is purposely invalid so it triggers a navigation error.
  GURL invalidURL(kTestUrl1);

  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey loadURL:invalidURL];
  CheckErrorPageIsVisible();
  OpenNewIncognitoTabUsingUIAndEvictMainTabs();

  SwitchToNormalMode();
  CheckErrorPageIsVisible();

  histogramTester.ExpectUniqueSample(kEvictedTabReloadSuccessRate,
                                     TabUsageRecorder::LOAD_FAILURE, 1,
                                     failureBlock);
  histogramTester.ExpectUniqueSample(kDidUserWaitForEvictedTabReload,
                                     TabUsageRecorder::USER_WAITED, 1,
                                     failureBlock);
  histogramTester.ExpectTotalCount(kEvictedTabReloadTime, 0, failureBlock);
}

// Test that USER_DID_NOT_WAIT is reported if the user does not wait for the
// reload to be complete after eviction.
- (void)testEvictedTabSlowReload {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";

  web::test::SetUpHttpServer(base::MakeUnique<web::DelayedResponseProvider>(
      base::MakeUnique<HtmlResponseProvider>(responses), kSlowURLDelay));

  chrome_test_util::HistogramTester histogramTester;
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // A blank tab needed to switch to it after reloading.
  chrome_test_util::OpenNewTab();
  chrome_test_util::OpenNewTab();
  chrome_test_util::LoadUrl(slowURL);
  OpenNewIncognitoTabUsingUIAndEvictMainTabs();

  web::test::SetUpHttpServer(base::MakeUnique<web::DelayedResponseProvider>(
      base::MakeUnique<HtmlResponseProvider>(responses), kSlowURLDelay));

  SwitchToNormalMode();

  GREYAssert(
      [[GREYCondition conditionWithName:@"Wait for tab to restart loading."
                                  block:^BOOL() {
                                    return chrome_test_util::IsLoading();
                                  }] waitWithTimeout:kWaitElementTimeout],
      @"Tab did not start loading.");

  // This method is not synced on EarlGrey.
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);

  // Do not test the kEvictedTabReloadSuccessRate, as the timing of the two
  // page loads cannot be guaranteed.  The test would be flaky.
  histogramTester.ExpectBucketCount(kDidUserWaitForEvictedTabReload,
                                    TabUsageRecorder::USER_DID_NOT_WAIT, 1,
                                    failureBlock);
}

// Test that the USER_DID_NOT_WAIT metric is logged when the user opens an NTP
// while the evicted tab is still reloading.
- (void)testEvictedTabReloadSwitchToNTP {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";

  web::test::SetUpHttpServer(base::MakeUnique<HtmlResponseProvider>(responses));

  chrome_test_util::HistogramTester histogramTester;
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  NewMainTabWithURL(slowURL, "Slow");

  OpenNewIncognitoTabUsingUIAndEvictMainTabs();
  web::test::SetUpHttpServer(base::MakeUnique<web::DelayedResponseProvider>(
      base::MakeUnique<HtmlResponseProvider>(responses), kSlowURLDelay));

  SwitchToNormalMode();

  // TODO(crbug.com/640977): EarlGrey synchronize on some animations when a
  // page is loading. Need to handle synchronization manually for this test.
  [[GREYConfiguration sharedInstance]
          setValue:@(NO)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
  OpenNewMainTabUsingUIUnsynced();
  [[GREYConfiguration sharedInstance]
          setValue:@(YES)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
  histogramTester.ExpectBucketCount(kDidUserWaitForEvictedTabReload,
                                    TabUsageRecorder::USER_DID_NOT_WAIT, 1,
                                    failureBlock);
}

// Test that the USER_DID_NOT_WAIT metric is not logged when the user opens
// and closes the settings UI while the evicted tab is still reloading.
- (void)testEvictedTabReloadSettingsAndBack {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";

  web::test::SetUpHttpServer(base::MakeUnique<web::DelayedResponseProvider>(
      base::MakeUnique<HtmlResponseProvider>(responses), kSlowURLDelay));

  chrome_test_util::HistogramTester histogramTester;
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  NewMainTabWithURL(slowURL, responses[slowURL]);

  OpenNewIncognitoTabUsingUIAndEvictMainTabs();

  SwitchToNormalMode();
  // TODO(crbug.com/640977): EarlGrey synchronize on some animations when a
  // page is loading. Need to handle synchronization manually for this test.
  [[GREYConfiguration sharedInstance]
          setValue:@(NO)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
  OpenSettingsMenuUnsynced();
  OpenSettingsSubMenuUnsynced(IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY);
  Wait(grey_accessibilityID(kPrivacyCollectionViewId),
       @"Privacy settings view.");

  WaitAndTap(grey_accessibilityLabel(
                 l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)),
             @"Close settings");
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                          responses[slowURL])]
      assertWithMatcher:grey_notNil()];

  [[GREYConfiguration sharedInstance]
          setValue:@(YES)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];

  histogramTester.ExpectBucketCount(kDidUserWaitForEvictedTabReload,
                                    TabUsageRecorder::USER_DID_NOT_WAIT, 0,
                                    failureBlock);
  histogramTester.ExpectBucketCount(kDidUserWaitForEvictedTabReload,
                                    TabUsageRecorder::USER_WAITED, 1,
                                    failureBlock);
}

// Tests that leaving Chrome while an evicted tab is reloading triggers the
// recording of the USER_LEFT_CHROME metric.
- (void)testEvictedTabReloadBackgrounded {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";

  web::test::SetUpHttpServer(base::MakeUnique<HtmlResponseProvider>(responses));

  chrome_test_util::HistogramTester histogramTester;
  chrome_test_util::OpenNewTab();
  chrome_test_util::LoadUrl(slowURL);

  OpenNewIncognitoTabUsingUIAndEvictMainTabs();

  web::test::SetUpHttpServer(base::MakeUnique<web::DelayedResponseProvider>(
      base::MakeUnique<HtmlResponseProvider>(responses), kSlowURLDelay));
  SwitchToNormalMode();

  // TODO(crbug.com/640977): EarlGrey synchronize on some animations when a
  // page is loading. Need to handle synchronization manually for this test.
  [[GREYConfiguration sharedInstance]
          setValue:@(NO)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
  id<GREYMatcher> toolMenuMatcher =
      grey_accessibilityID(kToolbarToolsMenuButtonIdentifier);
  Wait(toolMenuMatcher, @"Tool Menu");

  GREYAssertTrue(chrome_test_util::SimulateTabsBackgrounding(),
                 @"Failed to simulate tab backgrounding.");
  [[GREYConfiguration sharedInstance]
          setValue:@(YES)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  histogramTester.ExpectBucketCount(kDidUserWaitForEvictedTabReload,
                                    TabUsageRecorder::USER_LEFT_CHROME, 1,
                                    failureBlock);
}

// Tests that backgrounding a tab that was not evicted while it is loading does
// not record the USER_LEFT_CHROME metric.
- (void)testLiveTabReloadBackgrounded {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";

  web::test::SetUpHttpServer(base::MakeUnique<web::DelayedResponseProvider>(
      base::MakeUnique<HtmlResponseProvider>(responses), kSlowURLDelay));

  chrome_test_util::HistogramTester histogramTester;

  // We need two tabs to be able to switch.
  chrome_test_util::OpenNewTab();
  [[GREYConfiguration sharedInstance]
          setValue:@(NO)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
  chrome_test_util::LoadUrl(slowURL);

  // Ensure loading starts but is not finished.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSeconds(1));
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);
  [[GREYConfiguration sharedInstance]
          setValue:@(YES)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  histogramTester.ExpectBucketCount(kDidUserWaitForEvictedTabReload,
                                    TabUsageRecorder::USER_LEFT_CHROME, 0,
                                    failureBlock);
}

// Tests that redirecting pages are not reloaded after eviction.
- (void)testPageRedirect {
  GURL redirectURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/redirect_refresh.html");
  GURL destinationURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  ResetTabUsageRecorder();

  NewMainTabWithURL(redirectURL, "arrived");

  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  NSUInteger tabIndex = chrome_test_util::GetMainTabCount() - 1;
  chrome_test_util::OpenNewTab();
  OpenNewIncognitoTabUsingUIAndEvictMainTabs();
  SwitchToNormalMode();
  chrome_test_util::SelectTabAtIndexInCurrentMode(tabIndex);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                          "arrived")]
      assertWithMatcher:grey_notNil()];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  // Verify that one page-load count has been recorded.  It should contain a
  // sum of 1 - one sample with 1 page load.
  histogramTester.ExpectTotalCount(kPageLoadsBeforeEvictedTabSelected, 1,
                                   failureBlock);

  std::unique_ptr<base::HistogramSamples> samples =
      histogramTester.GetHistogramSamplesSinceCreation(
          kPageLoadsBeforeEvictedTabSelected);
  int sampleSum = samples->sum();
  GREYAssertEqual(sampleSum, 1, @"Expected page loads is %d, actual is %d.", 1,
                  sampleSum);
}

// Tests that navigations are correctly reported in
// Tab.PageLoadsSinceLastSwitchToEvictedTab histogram.
- (void)testLinkClickNavigation {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL initialURL =
      web::test::HttpServer::MakeUrl("http://scenarioTestLinkClickNavigation");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  responses[initialURL] = base::StringPrintf(
      "<body><a style='margin-left:50px' href='%s' id='link'>link</a></body>",
      destinationURL.spec().c_str());
  responses[destinationURL] = "Whee!";
  web::test::SetUpHttpServer(base::MakeUnique<HtmlResponseProvider>(responses));
  chrome_test_util::HistogramTester histogramTester;
  ResetTabUsageRecorder();

  // Open a tab with a link to click.
  NewMainTabWithURL(initialURL, "link");
  // Click the link.
  chrome_test_util::TapWebViewElementWithId("link");

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebViewContainingText("Whee")]
      assertWithMatcher:grey_notNil()];

  NSUInteger tabIndex = chrome_test_util::GetMainTabCount() - 1;
  chrome_test_util::OpenNewTab();
  OpenNewIncognitoTabUsingUIAndEvictMainTabs();
  SwitchToNormalMode();
  chrome_test_util::SelectTabAtIndexInCurrentMode(tabIndex);
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebViewContainingText("Whee")]
      assertWithMatcher:grey_notNil()];

  // Verify that the page-load count has been recorded.  It should contain a
  // sum of 2 - one sample with 2 page loads.
  std::unique_ptr<base::HistogramSamples> samples =
      histogramTester.GetHistogramSamplesSinceCreation(
          kPageLoadsBeforeEvictedTabSelected);
  int sampleSum = samples->sum();
  GREYAssertEqual(sampleSum, 2, @"Expected page loads is %d, actual %d.", 2,
                  sampleSum);

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  // Verify that only one evicted tab was selected.  This is to make sure the
  // link click did not generate an evicted-tab-reload count.
  histogramTester.ExpectBucketCount(kSelectedTabHistogramName,
                                    TabUsageRecorder::EVICTED, 1, failureBlock);
}

// Tests that opening links in a new tab will not evict the source tab.
- (void)testOpenLinkInNewTab {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL initialURL =
      web::test::HttpServer::MakeUrl("http://scenarioTestOpenLinkInNewTab");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  // Make the link that cover the whole page so that long pressing the web view
  // will trigger the link context menu.
  responses[initialURL] = base::StringPrintf(
      "<body style='width:auto; height:auto;'><a href='%s' "
      "id='link'><div style='width:100%%; "
      "height:100%%;'>link</div></a></body>",
      destinationURL.spec().c_str());
  responses[destinationURL] = "Whee!";
  web::test::SetUpHttpServer(base::MakeUnique<HtmlResponseProvider>(responses));
  chrome_test_util::HistogramTester histogramTester;
  ResetTabUsageRecorder();

  // Open a tab with a link to click.
  NewMainTabWithURL(initialURL, "link");

  int numberOfTabs = chrome_test_util::GetMainTabCount();
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebViewContainingText("link")]
      performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB))]
      performAction:grey_tap()];
  chrome_test_util::AssertMainTabCount(numberOfTabs + 1);

  SelectTabUsingUI(base::SysUTF8ToNSString(destinationURL.GetContent()));

  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebViewContainingText("Whee")]
      assertWithMatcher:grey_notNil()];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  histogramTester.ExpectTotalCount(kSelectedTabHistogramName, 1, failureBlock);
  histogramTester.ExpectBucketCount(
      kSelectedTabHistogramName, TabUsageRecorder::IN_MEMORY, 1, failureBlock);
}

// Tests that opening tabs from external app will not cause tab eviction.
- (void)testOpenFromApp {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  ResetTabUsageRecorder();

  chrome_test_util::OpenNewTab();
  GURL url(kTestUrl1);

  chrome_test_util::OpenChromeFromExternalApp(url);

  // Add a delay to ensure the tab has fully opened.  Because the check below
  // is for zero metrics recorded, it adds no flakiness.  However, this pause
  // makes the step more likely to fail in failure cases.  I.e. without it, this
  // test would sometimes pass even when it should fail.
  base::test::ios::SpinRunLoopWithMaxDelay(
      base::TimeDelta::FromMilliseconds(500));

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  // Verify that zero Tab.StatusWhenSwitchedBackToForeground metrics were
  // recorded.  Tabs created at the time the user switches to them should not
  // be counted in this metric.
  histogramTester.ExpectTotalCount(kSelectedTabHistogramName, 0, failureBlock);
}

// Verify that evicted tabs that are deleted are removed from the evicted tabs
// map.
- (void)testTabDeletion {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  ResetTabUsageRecorder();
  // Add an autorelease pool to delete the closed tabs before the end of the
  // test.
  @autoreleasepool {
    // Open two tabs with urls.
    OpenTwoTabs();
    // Set the normal tabs as 'cold start' tabs.
    chrome_test_util::SetCurrentTabsToBeColdStartTabs();
    // One more tab.
    const GURL url1 = web::test::HttpServer::MakeUrl(kTestUrl1);
    NewMainTabWithURL(url1, kURL1FirstWord);

    GREYAssertEqual(chrome_test_util::GetMainTabCount(), 3,
                    @"Check number of normal tabs");
    // The cold start tab which was not active will still be evicted.
    GREYAssertEqual(chrome_test_util::GetEvictedMainTabCount(), 1,
                    @"Check number of evicted tabs");

    // Close two of the three open tabs without selecting them first.
    // This should delete the tab objects, even though they're still being
    // tracked
    // by the tab usage recorder in its |evicted_tabs_| map.
    CloseTabAtIndexAndSync(1);

    GREYAssertEqual(chrome_test_util::GetMainTabCount(), 2,
                    @"Check number of normal tabs");
    CloseTabAtIndexAndSync(0);
    GREYAssertEqual(chrome_test_util::GetMainTabCount(), 1,
                    @"Check number of normal tabs");
    [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  }
  // The deleted tabs are purged during foregrounding and backgrounding.
  chrome_test_util::SimulateTabsBackgrounding();
  // Make sure |evicted_tabs_| purged the deleted tabs.
  int evicted = chrome_test_util::GetEvictedMainTabCount();
  GREYAssertEqual(evicted, 0, @"Check number of evicted tabs");
}

@end
