// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "base/test/ios/wait_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "ios/chrome/test/app/settings_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_assertions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"

using chrome_test_util::AssertMainTabCount;
using chrome_test_util::OmniboxText;
using chrome_test_util::TapWebViewElementWithId;
using chrome_test_util::WebViewContainingText;
using web::test::HttpServer;

namespace {
const char kTestURL[] =
    "http://ios/testing/data/http_server_files/window_open.html";
}  // namespace

// Test case for opening child windows by DOM.
@interface WindowOpenByDOMTestCase : ChromeTestCase
@end

@implementation WindowOpenByDOMTestCase

+ (void)setUp {
  [super setUp];
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_ALLOW);
  web::test::SetUpFileBasedHttpServer();
}

+ (void)tearDown {
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_DEFAULT);
  [super tearDown];
}

- (void)setUp {
  [super setUp];
  // Open the test page. There should only be one tab open.
  [ChromeEarlGrey loadURL:HttpServer::MakeUrl(kTestURL)];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText("Expected result")]
      assertWithMatcher:grey_notNil()];
  AssertMainTabCount(1);
}

// Tests that opening a link with target=_blank which then immediately closes
// itself works.
- (void)testLinkWithBlankTargetWithImmediateClose {
  TapWebViewElementWithId("webScenarioWindowOpenBlankTargetWithImmediateClose");
  AssertMainTabCount(1);
}

// Tests that sessionStorage content is available for windows opened by DOM via
// target="_blank" links.
- (void)testLinkWithBlankTargetSessionStorage {
  using chrome_test_util::ExecuteJavaScript;

  NSError* error = nil;
  ExecuteJavaScript(@"sessionStorage.setItem('key', 'value');", &error);
  GREYAssert(!error, @"Error during script execution: %@", error);

  TapWebViewElementWithId("webScenarioWindowOpenSamePageWithBlankTarget");
  AssertMainTabCount(2);
  [[EarlGrey selectElementWithMatcher:WebViewContainingText("Expected result")]
      assertWithMatcher:grey_notNil()];

  id value = ExecuteJavaScript(@"sessionStorage.getItem('key');", &error);
  GREYAssert(!error, @"Error during script execution: %@", error);
  GREYAssert([value isEqual:@"value"], @"sessionStorage is not shared");
}

// Tests a link with target="_blank".
- (void)testLinkWithBlankTarget {
  TapWebViewElementWithId("webScenarioWindowOpenRegularLink");
  AssertMainTabCount(2);
}

// Tests a link with target="_blank" multiple times.
- (void)testLinkWithBlankTargetMultipleTimes {
  TapWebViewElementWithId("webScenarioWindowOpenRegularLinkMultipleTimes");
  AssertMainTabCount(2);
  chrome_test_util::OpenNewTab();
  AssertMainTabCount(3);
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);
  TapWebViewElementWithId("webScenarioWindowOpenRegularLinkMultipleTimes");
  AssertMainTabCount(4);
}

// Tests a window.open by assigning to window.location.
- (void)testWindowOpenAndAssignToHref {
  TapWebViewElementWithId("webScenarioWindowOpenTabWithAssignmentToHref");
  AssertMainTabCount(2);
}

// Tests that opening a window and calling window.location.assign works.
- (void)testWindowOpenAndCallLocationAssign {
  // Open a child tab.
  TapWebViewElementWithId("webScenarioWindowOpenAndCallLocationAssign");
  AssertMainTabCount(2);

  // Ensure that the resulting tab is updated as expected.
  const GURL targetURL =
      HttpServer::MakeUrl(std::string(kTestURL) + "#assigned");
  [[EarlGrey selectElementWithMatcher:OmniboxText(targetURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that opening a window, reading its title, and updating its location
// completes and causes a navigation. (Reduced test case from actual site.)
- (void)testWindowOpenAndSetLocation {
  // Open a child tab.
  TapWebViewElementWithId("webScenarioWindowOpenAndSetLocation");
  AssertMainTabCount(2);

  // Ensure that the resulting tab is updated as expected.
  const GURL targetURL =
      HttpServer::MakeUrl(std::string(kTestURL) + "#updated");
  [[EarlGrey selectElementWithMatcher:OmniboxText(targetURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests a button that invokes window.open() with "_blank" target parameter.
- (void)testWindowOpenWithBlankTarget {
  TapWebViewElementWithId("webScenarioWindowOpenWithBlankTarget");
  AssertMainTabCount(2);
}

// Tests that opening a window with target=_blank which closes itself after 1
// second delay.
- (void)testLinkWithBlankTargetWithDelayedClose {
  TapWebViewElementWithId("webScenarioWindowOpenWithDelayedClose");
  AssertMainTabCount(2);
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(1));
  AssertMainTabCount(1);
}

// Tests a window.open used in a <button onClick> element.
- (void)testWindowOpenWithButtonOnClick {
  TapWebViewElementWithId("webScenarioWindowOpenWithButtonOnClick");
  AssertMainTabCount(2);
}

// Tests a button that invokes window.open with an empty target parameter.
- (void)testWindowOpenWithEmptyTarget {
  TapWebViewElementWithId("webScenarioWindowOpenWithEmptyTarget");
  AssertMainTabCount(2);
}

// Tests a link with JavaScript in the href.
+ (void)testWindowOpenWithJavaScriptInHref {
  TapWebViewElementWithId("webScenarioWindowOpenWithJavaScriptInHref");
  AssertMainTabCount(2);
}

// Tests a window.open by running Meta-Refresh.
- (void)testWindowOpenWithMetaRefresh {
  TapWebViewElementWithId("webScenarioWindowOpenWithMetaRefresh");
  AssertMainTabCount(2);
}

// Tests that a link with an onclick that opens a tab and calls preventDefault
// opens the tab, but doesn't navigate the main tab.
- (void)testWindowOpenWithPreventDefaultLink {
  // Open a child tab.
  TapWebViewElementWithId("webScenarioWindowOpenWithPreventDefaultLink");
  AssertMainTabCount(2);

  // Ensure that the starting tab hasn't navigated.
  chrome_test_util::CloseCurrentTab();
  const GURL URL = HttpServer::MakeUrl(kTestURL);
  [[EarlGrey selectElementWithMatcher:OmniboxText(URL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that closing the current window using DOM fails.
- (void)testCloseWindowNotOpenByDOM {
  TapWebViewElementWithId("webScenarioWindowClose");
  AssertMainTabCount(1);
}

@end
