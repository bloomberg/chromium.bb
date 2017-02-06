// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#import <map>
#import <string>

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kURLOfTestPage[] = "http://testPage";
const char kHTMLOfTestPage[] =
    "<head><title>TestPageTitle</title></head><body>hello</body>";
NSString* const kTitleOfTestPage = @"TestPageTitle";

// Makes sure at least one tab is opened and opens the recent tab panel.
void OpenRecentTabsPanel() {
  // At least one tab is needed to be able to open the recent tabs panel.
  if (chrome_test_util::GetMainTabCount() == 0)
    chrome_test_util::OpenNewTab();

  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> open_recent_tabs_button_matcher =
      grey_accessibilityID(kToolsMenuOtherDevicesId);
  [[EarlGrey selectElementWithMatcher:open_recent_tabs_button_matcher]
      performAction:grey_tap()];
}

// Closes the recent tabs panel, on iPhone.
void CloseRecentTabsPanelOnIphone() {
  DCHECK(!IsIPadIdiom());

  id<GREYMatcher> exit_button_matcher = grey_accessibilityID(@"Exit");
  [[EarlGrey selectElementWithMatcher:exit_button_matcher]
      performAction:grey_tap()];
}

// Returns the matcher for the entry of the page in the recent tabs panel.
id<GREYMatcher> TitleOfTestPage() {
  return grey_allOf(
      chrome_test_util::StaticTextWithAccessibilityLabel(kTitleOfTestPage),
      grey_sufficientlyVisible(), nil);
}

// Returns the matcher for the Recently closed label.
id<GREYMatcher> RecentlyClosedLabelMatcher() {
  return chrome_test_util::StaticTextWithAccessibilityLabelId(
      IDS_IOS_RECENT_TABS_RECENTLY_CLOSED);
}

}  // namespace

// Earl grey integration tests for Recent Tabs Panel Controller.
@interface RecentTabsPanelControllerTestCase : ChromeTestCase
@end

@implementation RecentTabsPanelControllerTestCase

- (void)setUp {
  [ChromeEarlGrey clearBrowsingHistory];
  [super setUp];
  web::test::SetUpSimpleHttpServer(std::map<GURL, std::string>{{
      web::test::HttpServer::MakeUrl(kURLOfTestPage),
      std::string(kHTMLOfTestPage),
  }});
}

- (void)tearDown {
  if (IsIPadIdiom()) {
    chrome_test_util::OpenNewTab();
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:RecentlyClosedLabelMatcher()]
        assertWithMatcher:grey_notNil()
                    error:&error];
    // If the Recent Tabs panel is shown, then switch back to the Most Visited
    // panel so that tabs opened in other tests will show the Most Visited panel
    // instead of the Recent Tabs panel.
    if (!error) {
      [[EarlGrey selectElementWithMatcher:RecentlyClosedLabelMatcher()]
          performAction:grey_swipeFastInDirection(kGREYDirectionRight)];
    }
    chrome_test_util::CloseCurrentTab();
  }
}

// Tests that a closed tab appears in the Recent Tabs panel, and that tapping
// the entry in the Recent Tabs panel re-opens the closed tab.
- (void)testClosedTabAppearsInRecentTabsPanel {
  const GURL testPageURL = web::test::HttpServer::MakeUrl(kURLOfTestPage);

  // Open the test page in a new tab.
  [ChromeEarlGrey loadURL:testPageURL];
  id<GREYMatcher> webViewMatcher =
      chrome_test_util::WebViewContainingText("hello");
  [[EarlGrey selectElementWithMatcher:webViewMatcher]
      assertWithMatcher:grey_notNil()];

  // Open the Recent Tabs panel, check that the test page is not
  // present.
  OpenRecentTabsPanel();
  [[EarlGrey selectElementWithMatcher:TitleOfTestPage()]
      assertWithMatcher:grey_nil()];

  // Get rid of the Recent Tabs Panel.
  if (IsIPadIdiom()) {
    // On iPad, the Recent Tabs panel is a new page in the navigation history.
    // Go back to the previous page to restore the test page.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
        performAction:grey_tap()];
    [ChromeEarlGrey waitForPageToFinishLoading];
  } else {
    // On iPhone, the Recent Tabs panel is shown in a modal view.
    // Close that modal.
    CloseRecentTabsPanelOnIphone();
    // Wait until the recent tabs panel is dismissed.
    [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  }

  // Close the tab containing the test page.
  chrome_test_util::CloseCurrentTab();

  // Open the Recent Tabs panel and check that the test page is present.
  OpenRecentTabsPanel();
  [[EarlGrey selectElementWithMatcher:TitleOfTestPage()]
      assertWithMatcher:grey_notNil()];

  // Tap on the entry for the test page in the Recent Tabs panel and check that
  // a tab containing the test page was opened.
  [[EarlGrey selectElementWithMatcher:TitleOfTestPage()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(
                            testPageURL.GetContent())];
}

@end
