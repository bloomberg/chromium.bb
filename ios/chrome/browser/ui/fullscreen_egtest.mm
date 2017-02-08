// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "base/mac/bind_objc_block.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/settings_test_util.h"
#import "ios/chrome/test/app/web_view_interaction_test_util.h"
#include "ios/chrome/test/earl_grey/chrome_assertions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_util.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#import "ios/web/public/test/http_server.h"
#import "ios/web/public/test/http_server_util.h"
#import "ios/web/public/test/response_providers/error_page_response_provider.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// TODO(crbug.com/638674): Move this to a shared location as it is a duplicate
// of ios/web/shell/test/page_state_egtest.mm.
// Returns a matcher for asserting that element's content offset matches the
// given |offset|.
id<GREYMatcher> ContentOffset(CGPoint offset) {
  MatchesBlock matches = ^BOOL(UIScrollView* element) {
    return CGPointEqualToPoint([element contentOffset], offset);
  };
  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"contentOffset"];
  };
  return grey_allOf(
      grey_kindOfClass([UIScrollView class]),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      nil);
}

// Hides the toolbar by scrolling down.
void HideToolbarUsingUI() {
  [[EarlGrey
      selectElementWithMatcher:WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
}

// Asserts that the current URL is the |expectedURL| one.
void AssertURLIs(const GURL& expectedURL) {
  NSString* description = [NSString
      stringWithFormat:@"Timeout waiting for the url to be %@",
                       base::SysUTF8ToNSString(expectedURL.GetContent())];

  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                            expectedURL.GetContent())]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return (error == nil);
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(1.0, condition), description);
}

// Asserts that the current web view containers contains |text|.
void AssertStringIsPresentOnPage(const std::string& text) {
  id<GREYMatcher> response_matcher =
      chrome_test_util::WebViewContainingText(text);
  [[EarlGrey selectElementWithMatcher:response_matcher]
      assertWithMatcher:grey_notNil()];
}

}  // namespace

#pragma mark - Tests

// Fullscreens tests for Chrome.
@interface FullscreenTestCase : ChromeTestCase

@end

@implementation FullscreenTestCase

// Verifies that the content offset of the web view is set up at the correct
// initial value when initially displaying a PDF.
- (void)testLongPDFInitialState {
  web::test::SetUpFileBasedHttpServer();
  GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/two_pages.pdf");
  [ChromeEarlGrey loadURL:URL];

  chrome_test_util::AssertToolbarVisible();
  // Initial y scroll position is -56 on iPhone and -95 on iPad, to make room
  // for the toolbar.
  // TODO(crbug.com/618887) Replace use of specific values when API which
  // generates these values is exposed.
  CGFloat yOffset = IsIPadIdiom() ? -95.0 : -56.0;
  [[EarlGrey
      selectElementWithMatcher:web::WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      assertWithMatcher:ContentOffset(CGPointMake(0, yOffset))];
}

// Verifies that the toolbar properly appears/disappears when scrolling up/down
// on a PDF that is short in length and wide in width.
- (void)testSmallWidePDFScroll {
  web::test::SetUpFileBasedHttpServer();
  GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/single_page_wide.pdf");
  [ChromeEarlGrey loadURL:URL];

  // Test that the toolbar is still visible after a user swipes down.
  [[EarlGrey
      selectElementWithMatcher:WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  chrome_test_util::AssertToolbarVisible();

  // Test that the toolbar is no longer visible after a user swipes up.
  HideToolbarUsingUI();
  chrome_test_util::AssertToolbarNotVisible();
}

// Verifies that the toolbar properly appears/disappears when scrolling up/down
// on a PDF that is long in length and wide in width.
- (void)testLongPDFScroll {
  web::test::SetUpFileBasedHttpServer();
  GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/two_pages.pdf");
  [ChromeEarlGrey loadURL:URL];

  // Test that the toolbar is hidden after a user swipes up.
  HideToolbarUsingUI();
  chrome_test_util::AssertToolbarNotVisible();

  // Test that the toolbar is visible after a user swipes down.
  [[EarlGrey
      selectElementWithMatcher:WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  chrome_test_util::AssertToolbarVisible();

  // Test that the toolbar is hidden after a user swipes up.
  HideToolbarUsingUI();
  chrome_test_util::AssertToolbarNotVisible();
}

// Tests that link clicks from a chrome:// to chrome:// link result in the
// header being shown even if was not previously shown.
- (void)testChromeToChromeURLKeepsHeaderOnScreen {
  const GURL kChromeAboutURL("chrome://chrome-urls");
  [ChromeEarlGrey loadURL:kChromeAboutURL];

  AssertStringIsPresentOnPage("chrome://version");

  // Hide the toolbar. The page is not long enough to dismiss the toolbar using
  // the UI so we have to zoom in.
  const char script[] =
      "(function(){"
      "var metas = document.getElementsByTagName('meta');"
      "for (var i=0; i<metas.length; i++) {"
      "  if (metas[i].getAttribute('name') == 'viewport') {"
      "    metas[i].setAttribute('content', 'width=10');"
      "    return;"
      "  }"
      "}"
      "document.body.innerHTML += \"<meta name='viewport' content='width=10'>\""
      "})()";

  __block bool finished = false;
  chrome_test_util::GetCurrentWebState()->ExecuteJavaScript(
      base::UTF8ToUTF16(script), base::BindBlockArc(^(const base::Value*) {
        finished = true;
      }));

  GREYAssert(testing::WaitUntilConditionOrTimeout(1.0,
                                                  ^{
                                                    return finished;
                                                  }),
             @"JavaScript to hide the toolbar did not complete");

  // Scroll up to be sure the toolbar can be dismissed by scrolling down.
  [[EarlGrey
      selectElementWithMatcher:WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Scroll to hide the UI.
  HideToolbarUsingUI();
  chrome_test_util::AssertToolbarNotVisible();

  // Test that the toolbar is visible when moving from one chrome:// link to
  // another chrome:// link.
  chrome_test_util::TapWebViewElementWithId("version");
  chrome_test_util::AssertToolbarVisible();
}

// Tests hiding and showing of the header with a user scroll on a long page.
- (void)testHideHeaderUserScrollLongPage {
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://tallpage");

  // A page long enough to ensure that the toolbar goes away on scrolling.
  responses[URL] = "<p style='height:200em'>a</p><p>b</p>";
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  chrome_test_util::AssertToolbarVisible();
  // Simulate a user scroll down.
  HideToolbarUsingUI();
  chrome_test_util::AssertToolbarNotVisible();
  // Simulate a user scroll up.
  [[EarlGrey
      selectElementWithMatcher:WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  chrome_test_util::AssertToolbarVisible();
}

// Tests that reloading of a page shows the header even if it was not shown
// previously.
- (void)testShowHeaderOnReload {
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");
  // This is a tall page -- necessary to make sure scrolling can hide away the
  // toolbar safely-- and with a link to reload itself.
  responses[URL] =
      "<p style='height:200em'>Tall page</p>"
      "<a onclick='window.location.reload();' id='link'>link</a>";
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  AssertStringIsPresentOnPage("Tall page");

  // Hide the toolbar.
  HideToolbarUsingUI();
  chrome_test_util::AssertToolbarNotVisible();

  chrome_test_util::TapWebViewElementWithId("link");

  // Main test is here: Make sure the header is still visible!
  chrome_test_util::AssertToolbarVisible();
}

// Test to make sure the header is shown when a Tab opened by the current Tab is
// closed even if the toolbar was not present previously.
- (void)testShowHeaderWhenChildTabCloses {
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  // JavaScript to open a window using window.open.
  std::string javaScript =
      base::StringPrintf("window.open(\"%s\");", destinationURL.spec().c_str());

  // A long page with a link to execute JavaScript.
  responses[URL] = base::StringPrintf(
      "<p style='height:200em'>whatever</p>"
      "<a onclick='%s' id='link1'>link1</a>",
      javaScript.c_str());
  // A long page with some simple text and link to close itself using
  // window.close.
  javaScript = "window.close()";
  responses[destinationURL] = base::StringPrintf(
      "<p style='height:200em'>whatever</p><a onclick='%s' "
      "id='link2'>link2</a>",
      javaScript.c_str());

  web::test::SetUpSimpleHttpServer(responses);
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_ALLOW);

  [ChromeEarlGrey loadURL:URL];
  AssertStringIsPresentOnPage("link1");
  chrome_test_util::AssertMainTabCount(1);

  // Hide the toolbar.
  HideToolbarUsingUI();
  chrome_test_util::AssertToolbarNotVisible();

  // Open new window.
  chrome_test_util::TapWebViewElementWithId("link1");

  // Check that a new Tab was created.
  AssertStringIsPresentOnPage("link2");
  chrome_test_util::AssertMainTabCount(2);

  AssertURLIs(destinationURL);

  // Hide the toolbar.
  HideToolbarUsingUI();
  chrome_test_util::AssertToolbarNotVisible();

  // Close the tab.
  chrome_test_util::TapWebViewElementWithId("link2");
  AssertStringIsPresentOnPage("link1");

  // Make sure the toolbar is on the screen.
  chrome_test_util::AssertMainTabCount(1);
  chrome_test_util::AssertToolbarVisible();
}

// Tests that the header is shown when a regular page (non-native page) is
// loaded from a page where the header was not see before.
// Also tests that auto-hide works correctly on new page loads.
- (void)testShowHeaderOnRegularPageLoad {
  std::map<GURL, std::string> responses;
  const GURL originURL = web::test::HttpServer::MakeUrl("http://origin");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");

  const std::string manyLines =
      "<p style='height:100em'>a</p><p>End of lines</p>";

  // A long page representing many lines and a link to the destination URL page.
  responses[originURL] = manyLines + "<a href='" + destinationURL.spec() +
                         "' id='link1'>link1</a>";
  // A long page representing many lines and a link to go back.
  responses[destinationURL] = manyLines +
                              "<a href='javascript:void(0)' "
                              "onclick='window.history.back()' "
                              "id='link2'>link2</a>";
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:originURL];

  AssertStringIsPresentOnPage("link1");
  // Dismiss the toolbar.
  HideToolbarUsingUI();
  chrome_test_util::AssertToolbarNotVisible();

  // Navigate to the other page.
  chrome_test_util::TapWebViewElementWithId("link1");
  AssertStringIsPresentOnPage("link2");

  // Make sure toolbar is shown since a new load has started.
  chrome_test_util::AssertToolbarVisible();

  // Dismiss the toolbar.
  HideToolbarUsingUI();
  chrome_test_util::AssertToolbarNotVisible();

  // Go back.
  chrome_test_util::TapWebViewElementWithId("link2");

  // Make sure the toolbar has loaded now that a new page has loaded.
  chrome_test_util::AssertToolbarVisible();
}

// Tests that the header is shown when a native page is loaded from a page where
// the header was not seen before.
- (void)testShowHeaderOnNativePageLoad {
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");

  // A long page representing many lines and a link to go back.
  std::string manyLines =
      "<p style='height:100em'>a</p>"
      "<a onclick='window.history.back()' id='link'>link</a>";
  responses[URL] = manyLines;
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  AssertStringIsPresentOnPage("link");

  // Dismiss the toolbar.
  HideToolbarUsingUI();
  chrome_test_util::AssertToolbarNotVisible();

  // Go back to NTP, which is a native view.
  chrome_test_util::TapWebViewElementWithId("link");

  // Make sure the toolbar is visible now that a new page has loaded.
  chrome_test_util::AssertToolbarVisible();
}

// Tests that the header is shown when loading an error page in a native view
// even if fullscreen was enabled previously.
- (void)testShowHeaderOnErrorPage {
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");
  // A long page with some simple text -- a long page is necessary so that
  // enough content is present to ensure that the toolbar can be hidden safely.
  responses[URL] = base::StringPrintf(
      "<p style='height:100em'>a</p>"
      "<a href=\"%s\" id=\"link\">bad link</a>",
      ErrorPageResponseProvider::GetDnsFailureUrl().spec().c_str());
  std::unique_ptr<web::DataResponseProvider> provider(
      new ErrorPageResponseProvider(responses));
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:URL];
  HideToolbarUsingUI();
  chrome_test_util::AssertToolbarNotVisible();

  chrome_test_util::TapWebViewElementWithId("link");
  AssertURLIs(ErrorPageResponseProvider::GetDnsFailureUrl());
  chrome_test_util::AssertToolbarVisible();
}

@end
