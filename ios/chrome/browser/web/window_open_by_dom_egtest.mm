// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_error_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/net/url_test_util.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::GetCurrentWebState;
using chrome_test_util::OmniboxText;
using web::test::HttpServer;
using web::WebViewInWebState;

namespace {
// URL of the file-based page supporting these tests.
const char kTestURL[] =
    "http://ios/testing/data/http_server_files/window_open.html";

// Returns matcher for Blocked Popup infobar.
id<GREYMatcher> PopupBlocker() {
  NSString* blockerText = base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
      IDS_IOS_POPUPS_BLOCKED_MOBILE, base::UTF8ToUTF16("1")));
  return grey_accessibilityLabel(blockerText);
}

}  // namespace

// Test case for opening child windows by DOM.
@interface WindowOpenByDOMTestCase : ChromeTestCase
@end

@implementation WindowOpenByDOMTestCase

+ (void)setUp {
  [super setUp];
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey setContentSettings:CONTENT_SETTING_ALLOW]);
  web::test::SetUpFileBasedHttpServer();
}

+ (void)tearDown {
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey setContentSettings:CONTENT_SETTING_DEFAULT]);
  [super tearDown];
}

- (void)setUp {
  [super setUp];
  // Open the test page. There should only be one tab open.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey loadURL:HttpServer::MakeUrl(kTestURL)]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:"Expected result"]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:1]);
}

// Tests that opening a link with target=_blank which then immediately closes
// itself works.
- (void)testLinkWithBlankTargetWithImmediateClose {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      tapWebStateElementWithID:
          @"webScenarioWindowOpenBlankTargetWithImmediateClose"]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:1]);
}

// Tests that sessionStorage content is available for windows opened by DOM via
// target="_blank" links.
- (void)testLinkWithBlankTargetSessionStorage {
  [ChromeEarlGrey executeJavaScript:@"sessionStorage.setItem('key', 'value');"];
  const char ID[] = "webScenarioWindowOpenSameURLWithBlankTarget";
  [[EarlGrey selectElementWithMatcher:WebViewInWebState(GetCurrentWebState())]
      performAction:web::WebViewTapElement(
                        GetCurrentWebState(),
                        [ElementSelector selectorWithElementID:ID])];

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:"Expected result"]);

  id value =
      [ChromeEarlGrey executeJavaScript:@"sessionStorage.getItem('key');"];
  GREYAssert([value isEqual:@"value"], @"sessionStorage is not shared");
}

// Tests tapping a link with target="_blank".
- (void)testLinkWithBlankTarget {
  const char ID[] = "webScenarioWindowOpenRegularLink";
  [[EarlGrey selectElementWithMatcher:WebViewInWebState(GetCurrentWebState())]
      performAction:web::WebViewTapElement(
                        GetCurrentWebState(),
                        [ElementSelector selectorWithElementID:ID])];
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);
}

// Tests executing script that clicks a link with target="_blank".
- (void)testLinkWithBlankTargetWithoutUserGesture {
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey setContentSettings:CONTENT_SETTING_BLOCK]);
  [ChromeEarlGrey
      executeJavaScript:@"document.getElementById('"
                        @"webScenarioWindowOpenRegularLink').click()"];
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:PopupBlocker()]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:1]);
}

// Tests a link with target="_blank" multiple times.
- (void)testLinkWithBlankTargetMultipleTimes {
  const char ID[] = "webScenarioWindowOpenRegularLinkMultipleTimes";
  web::WebState* test_page_web_state = GetCurrentWebState();
  id<GREYMatcher> test_page_matcher = WebViewInWebState(test_page_web_state);
  id<GREYAction> link_tap = web::WebViewTapElement(
      test_page_web_state, [ElementSelector selectorWithElementID:ID]);
  [[EarlGrey selectElementWithMatcher:test_page_matcher]
      performAction:link_tap];
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey openNewTab]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:3]);
  [ChromeEarlGrey selectTabAtIndex:0];
  [[EarlGrey selectElementWithMatcher:test_page_matcher]
      performAction:link_tap];
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:4]);
}

// Tests a window.open by assigning to window.location.
- (void)testWindowOpenAndAssignToHref {
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:
                          @"webScenarioWindowOpenTabWithAssignmentToHref"]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);
}

// Tests that opening a window and calling window.location.assign works.
- (void)testWindowOpenAndCallLocationAssign {
  // Open a child tab.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenAndCallLocationAssign"]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);

  // Ensure that the resulting tab is updated as expected.
  const GURL targetURL =
      HttpServer::MakeUrl(std::string(kTestURL) + "#assigned");
  const std::string targetOmniboxText =
      net::GetContentAndFragmentForUrl(targetURL);
  [[EarlGrey selectElementWithMatcher:OmniboxText(targetOmniboxText)]
      assertWithMatcher:grey_notNil()];
}

// Tests that opening a window, reading its title, and updating its location
// completes and causes a navigation. (Reduced test case from actual site.)
- (void)testWindowOpenAndSetLocation {
  // Open a child tab.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenAndSetLocation"]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);

  // Ensure that the resulting tab is updated as expected.
  const GURL targetURL =
      HttpServer::MakeUrl(std::string(kTestURL) + "#updated");
  const std::string targetOmniboxText =
      net::GetContentAndFragmentForUrl(targetURL);
  [[EarlGrey selectElementWithMatcher:OmniboxText(targetOmniboxText)]
      assertWithMatcher:grey_notNil()];
}

// Tests a button that invokes window.open() with "_blank" target parameter.
- (void)testWindowOpenWithBlankTarget {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithBlankTarget"]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);
}

// Tests that opening a window with target=_blank which closes itself after 1
// second delay.
- (void)testLinkWithBlankTargetWithDelayedClose {
  const char ID[] = "webScenarioWindowOpenWithDelayedClose";
  [[EarlGrey selectElementWithMatcher:WebViewInWebState(GetCurrentWebState())]
      performAction:web::WebViewTapElement(
                        GetCurrentWebState(),
                        [ElementSelector selectorWithElementID:ID])];
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(1));
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:1]);
}

// Tests a window.open used in a <button onClick> element.
- (void)testWindowOpenWithButtonOnClick {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithButtonOnClick"]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);
}

// Tests a button that invokes window.open with an empty target parameter.
- (void)testWindowOpenWithEmptyTarget {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithEmptyTarget"]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);
}

// Tests that the correct URL is displayed for a child window opened with the
// script window.open('', '').location.replace('about:blank#hash').
// This is a regression test for crbug.com/866142.
- (void)testLocationReplaceInWindowOpenWithEmptyTarget {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      tapWebStateElementWithID:
          @"webScenarioLocationReplaceInWindowOpenWithEmptyTarget"]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);
  // WebKit doesn't parse 'about:blank#hash' as about:blank with URL fragment.
  // Instead, it percent encodes '#hash' and considers 'blank%23hash' as the
  // resource identifier. Nevertheless, the '#' is significant in triggering the
  // edge case in the bug. TODO(crbug.com/885249): Change back to '#'.
  const GURL URL("about:blank%23hash");
  [[EarlGrey selectElementWithMatcher:OmniboxText("about:blank%23hash")]
      assertWithMatcher:grey_notNil()];
}

// Tests a link with JavaScript in the href.
+ (void)testWindowOpenWithJavaScriptInHref {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithJavaScriptInHref"]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);
}

// Tests a window.open by running Meta-Refresh.
- (void)testWindowOpenWithMetaRefresh {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithMetaRefresh"]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);
}

// Tests that a link with an onclick that opens a tab and calls preventDefault
// opens the tab, but doesn't navigate the main tab.
- (void)testWindowOpenWithPreventDefaultLink {
  // Open a child tab.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioWindowOpenWithPreventDefaultLink"]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);

  // Ensure that the starting tab hasn't navigated.
  [ChromeEarlGrey closeCurrentTab];
  const GURL URL = HttpServer::MakeUrl(kTestURL);
  [[EarlGrey selectElementWithMatcher:OmniboxText(URL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that closing the current window using DOM fails.
- (void)testCloseWindowNotOpenByDOM {
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:@"webScenarioWindowClose"]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:1]);
}

// Tests that popup blocking works when a popup is injected into a window before
// its initial load is committed.
- (void)testBlockPopupInjectedIntoOpenedWindow {
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey setContentSettings:CONTENT_SETTING_BLOCK]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      tapWebStateElementWithID:@"webScenarioOpenWindowAndInjectPopup"]);
  [[EarlGrey selectElementWithMatcher:PopupBlocker()]
      assertWithMatcher:grey_notNil()];
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2]);
}

@end
