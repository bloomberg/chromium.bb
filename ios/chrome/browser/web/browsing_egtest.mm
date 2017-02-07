// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>
#include <map>
#include <memory>
#include <string>

#include "base/ios/ios_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_assertions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#include "ios/web/public/test/response_providers/data_response_provider.h"
#include "net/http/http_response_headers.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using chrome_test_util::OmniboxText;
using chrome_test_util::WebViewContainingText;

namespace {

// URL used for the reload test.
const char kReloadTestUrl[] = "http://mock/reloadTest";

// Returns the number of serviced requests in HTTP body.
class ReloadResponseProvider : public web::DataResponseProvider {
 public:
  ReloadResponseProvider() : request_number_(0) {}

  // URL used for the reload test.
  static GURL GetReloadTestUrl() {
    return web::test::HttpServer::MakeUrl(kReloadTestUrl);
  }

  bool CanHandleRequest(const Request& request) override {
    return request.url == ReloadResponseProvider::GetReloadTestUrl();
  }

  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    DCHECK_EQ(ReloadResponseProvider::GetReloadTestUrl(), request.url);
    *headers = GetDefaultResponseHeaders();
    *response_body = GetResponseBody(request_number_++);
  }

  // static
  static std::string GetResponseBody(int request_number) {
    return base::StringPrintf("Load request %d", request_number);
  }

 private:
  int request_number_;  // Count of requests received by the response provider.
};

// ScopedBlockPopupsPref modifies the block popups preference and resets the
// preference to its original value when this object goes out of scope.
// TODO(crbug.com/638674): Evaluate if this can move to shared code
class ScopedBlockPopupsPref {
 public:
  ScopedBlockPopupsPref(ContentSetting setting) {
    original_setting_ = GetPrefValue();
    SetPrefValue(setting);
  }
  ~ScopedBlockPopupsPref() { SetPrefValue(original_setting_); }

 private:
  // Gets the current value of the preference.
  ContentSetting GetPrefValue() {
    ContentSetting popupSetting =
        ios::HostContentSettingsMapFactory::GetForBrowserState(
            chrome_test_util::GetOriginalBrowserState())
            ->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS, NULL);
    return popupSetting;
  }

  // Sets the preference to the given value.
  void SetPrefValue(ContentSetting setting) {
    DCHECK(setting == CONTENT_SETTING_BLOCK ||
           setting == CONTENT_SETTING_ALLOW);
    ios::ChromeBrowserState* state =
        chrome_test_util::GetOriginalBrowserState();
    ios::HostContentSettingsMapFactory::GetForBrowserState(state)
        ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS, setting);
  }

  // Saves the original pref setting so that it can be restored when the scoper
  // is destroyed.
  ContentSetting original_setting_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBlockPopupsPref);
};
}  // namespace

// Tests web browsing scenarios.
@interface BrowsingTestCase : ChromeTestCase
@end

@implementation BrowsingTestCase

// Matcher for the title of the current tab (on tablet only).
id<GREYMatcher> TabWithTitle(const std::string& tab_title) {
  id<GREYMatcher> notPartOfOmnibox =
      grey_not(grey_ancestor(chrome_test_util::Omnibox()));
  return grey_allOf(grey_accessibilityLabel(base::SysUTF8ToNSString(tab_title)),
                    notPartOfOmnibox, nil);
}

// Matcher for a Go button that is interactable.
id<GREYMatcher> GoButtonMatcher() {
  return grey_allOf(grey_accessibilityID(@"Go"), grey_interactable(), nil);
}

// Tests that page successfully reloads.
- (void)testReload {
  // Set up test HTTP server responses.
  std::unique_ptr<web::DataResponseProvider> provider(
      new ReloadResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  GURL URL = ReloadResponseProvider::GetReloadTestUrl();
  [ChromeEarlGrey loadURL:URL];
  std::string expectedBodyBeforeReload(
      ReloadResponseProvider::GetResponseBody(0 /* request number */));
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText(expectedBodyBeforeReload)]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGreyUI reload];
  std::string expectedBodyAfterReload(
      ReloadResponseProvider::GetResponseBody(1 /* request_number */));
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText(expectedBodyAfterReload)]
      assertWithMatcher:grey_notNil()];
}

// Tests that a tab's title is based on the URL when no other information is
// available.
- (void)testBrowsingTabTitleSetFromURL {
  if (!IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Tab Title not displayed on handset.");
  }

  web::test::SetUpFileBasedHttpServer();

  const GURL destinationURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  [ChromeEarlGrey loadURL:destinationURL];

  // Add 3 for the "://" which is not considered part of the scheme
  std::string URLWithoutScheme =
      destinationURL.spec().substr(destinationURL.scheme().length() + 3);

  [[EarlGrey selectElementWithMatcher:TabWithTitle(URLWithoutScheme)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that after a PDF is loaded, the title appears in the tab bar on iPad.
- (void)testPDFLoadTitle {
  if (!IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Tab Title not displayed on handset.");
  }

  web::test::SetUpFileBasedHttpServer();

  const GURL destinationURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/testpage.pdf");
  [ChromeEarlGrey loadURL:destinationURL];

  // Add 3 for the "://" which is not considered part of the scheme
  std::string URLWithoutScheme =
      destinationURL.spec().substr(destinationURL.scheme().length() + 3);

  [[EarlGrey selectElementWithMatcher:TabWithTitle(URLWithoutScheme)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tab title is set to the specified title from a JavaScript.
- (void)testBrowsingTabTitleSetFromScript {
  if (!IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Tab Title not displayed on handset.");
  }

  const char* kPageTitle = "Some title";
  const GURL URL = GURL(base::StringPrintf(
      "data:text/html;charset=utf-8,<script>document.title = "
      "\"%s\"</script>",
      kPageTitle));
  [ChromeEarlGrey loadURL:URL];

  [[EarlGrey selectElementWithMatcher:TabWithTitle(kPageTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests clicking a link with target="_blank" and "event.stopPropagation()"
// opens a new tab.
- (void)testBrowsingStopPropagation {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://stopPropagation");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  // This is a page with a link to |kDestination|.
  responses[URL] = base::StringPrintf(
      "<a id='link' href='%s' target='_blank' "
      "onclick='event.stopPropagation()'>link</a>",
      destinationURL.spec().c_str());
  // This is the destination page; it just contains some text.
  responses[destinationURL] = "You've arrived!";
  web::test::SetUpSimpleHttpServer(responses);

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW);

  [ChromeEarlGrey loadURL:URL];
  chrome_test_util::AssertMainTabCount(1);

  chrome_test_util::TapWebViewElementWithId("link");

  chrome_test_util::AssertMainTabCount(2);

  // Verify the new tab was opened with the expected URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests clicking a relative link with target="_blank" and
// "event.stopPropagation()" opens a new tab.
- (void)testBrowsingStopPropagationRelativePath {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://stopPropRel");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://stopPropRel/#test");
  // This is page with a relative link to "#test".
  responses[URL] =
      "<a id='link' href='#test' target='_blank' "
      "onclick='event.stopPropagation()'>link</a>";
  // This is the page that should be showing at the end of the test.
  responses[destinationURL] = "You've arrived!";
  web::test::SetUpSimpleHttpServer(responses);

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW);

  [ChromeEarlGrey loadURL:URL];
  chrome_test_util::AssertMainTabCount(1);

  chrome_test_util::TapWebViewElementWithId("link");

  chrome_test_util::AssertMainTabCount(2);

  // Verify the new tab was opened with the expected URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that clicking a link with URL changed by onclick uses the href of the
// anchor tag instead of the one specified in JavaScript. Also verifies a new
// tab is opened by target '_blank'.
// TODO(crbug.com/688223): WKWebView does not open a new window as expected by
// this test.
- (void)DISABLED_testBrowsingPreventDefaultWithLinkOpenedByJavascript {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://preventDefaultWithLinkOpenedByJavascript");
  const GURL anchorURL =
      web::test::HttpServer::MakeUrl("http://anchorDestination");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://javaScriptDestination");
  // This is a page with a link where the href and JavaScript are setting the
  // destination to two different URLs so the test can verify which one the
  // browser uses.
  responses[URL] = base::StringPrintf(
      "<a id='link' href='%s' target='_blank' "
      "onclick='window.location.href=\"%s\"; "
      "event.stopPropagation()' id='link'>link</a>",
      anchorURL.spec().c_str(), destinationURL.spec().c_str());
  responses[anchorURL] = "anchor destination";

  web::test::SetUpSimpleHttpServer(responses);

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW);

  [ChromeEarlGrey loadURL:URL];
  chrome_test_util::AssertMainTabCount(1);

  chrome_test_util::TapWebViewElementWithId("link");

  chrome_test_util::AssertMainTabCount(2);

  // Verify the new tab was opened with the expected URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(anchorURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests tapping a link that navigates to a page that immediately navigates
// again via document.location.href.
- (void)testBrowsingWindowDataLinkScriptRedirect {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://windowDataLinkScriptRedirect");
  const GURL intermediateURL =
      web::test::HttpServer::MakeUrl("http://intermediate");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  // This is a page with a link to the intermediate page.
  responses[URL] =
      base::StringPrintf("<a id='link' href='%s' target='_blank'>link</a>",
                         intermediateURL.spec().c_str());
  // This intermediate page uses JavaScript to immediately navigate to the
  // destination page.
  responses[intermediateURL] =
      base::StringPrintf("<script>document.location.href=\"%s\"</script>",
                         destinationURL.spec().c_str());
  // This is the page that should be showing at the end of the test.
  responses[destinationURL] = "You've arrived!";

  web::test::SetUpSimpleHttpServer(responses);

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW);

  [ChromeEarlGrey loadURL:URL];
  chrome_test_util::AssertMainTabCount(1);

  chrome_test_util::TapWebViewElementWithId("link");

  chrome_test_util::AssertMainTabCount(2);

  // Verify the new tab was opened with the expected URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that pressing the button on a POST-based form changes the page and that
// the back button works as expected afterwards.
- (void)testBrowsingPostEntryWithButton {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://postEntryWithButton");
  const GURL destinationURL = web::test::HttpServer::MakeUrl("http://foo");
  // This is a page with a button that posts to the destination.
  responses[URL] = base::StringPrintf(
      "<form action='%s' method='post'>"
      "<input value='button' type='submit' id='button'></form>",
      destinationURL.spec().c_str());
  // This is the page that should be showing at the end of the test.
  responses[destinationURL] = "bar!";
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  chrome_test_util::TapWebViewElementWithId("button");

  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go back and verify the browser navigates to the original URL.
  [self goBack];
  [[EarlGrey selectElementWithMatcher:OmniboxText(URL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that a link with a JavaScript-based navigation changes the page and
// that the back button works as expected afterwards.
- (void)testBrowsingJavaScriptBasedNavigation {
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");
  const GURL destURL = web::test::HttpServer::MakeUrl("http://destination");
  // Page containing a link with onclick attribute that sets window.location
  // to the destination URL.
  responses[URL] = base::StringPrintf(
      "<a href='#' onclick=\"window.location='%s';\" id='link'>Link</a>",
      destURL.spec().c_str());
  // Page with some text.
  responses[destURL] = "You've arrived!";
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  chrome_test_util::TapWebViewElementWithId("link");

  [[EarlGrey selectElementWithMatcher:OmniboxText(destURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  [self goBack];
  [[EarlGrey selectElementWithMatcher:OmniboxText(URL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// TODO(crbug.com/638674): Evaluate if this can move to shared code
// Navigates back to the previous webpage.
- (void)goBack {
  base::scoped_nsobject<GenericChromeCommand> backCommand(
      [[GenericChromeCommand alloc] initWithTag:IDC_BACK]);
  chrome_test_util::RunCommandWithActiveViewController(backCommand);

  [ChromeEarlGrey waitForPageToFinishLoading];
}

// Navigates forward to a previous webpage.
// TODO(crbug.com/638674): Evaluate if this can move to shared code
- (void)goForward {
  base::scoped_nsobject<GenericChromeCommand> forwardCommand(
      [[GenericChromeCommand alloc] initWithTag:IDC_FORWARD]);
  chrome_test_util::RunCommandWithActiveViewController(forwardCommand);

  [ChromeEarlGrey waitForPageToFinishLoading];
}

// Tests that a link with WebUI URL does not trigger a load. WebUI pages may
// have increased power and using the same web process (which may potentially
// be controlled by an attacker) is dangerous.
- (void)testTapLinkWithWebUIURL {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL(web::test::HttpServer::MakeUrl("http://pageWithWebUILink"));
  const char kPageHTML[] =
      "<script>"
      "  function printMsg() {"
      "    document.body.appendChild(document.createTextNode('Hello world!'));"
      "  }"
      "</script>"
      "<a href='chrome://version' id='link' onclick='printMsg()'>Version</a>";
  responses[URL] = kPageHTML;
  web::test::SetUpSimpleHttpServer(responses);

  // Assert that test is starting with one tab.
  chrome_test_util::AssertMainTabCount(1U);
  chrome_test_util::AssertIncognitoTabCount(0U);

  [ChromeEarlGrey loadURL:URL];

  // Tap on chrome://version link.
  [ChromeEarlGrey tapWebViewElementWithID:@"link"];

  // Verify that page did not change by checking its URL and message printed by
  // onclick event.
  [[EarlGrey selectElementWithMatcher:OmniboxText("chrome://version")]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText("Hello world!")]
      assertWithMatcher:grey_notNil()];

  // Verify that no new tabs were open which could load chrome://version.
  chrome_test_util::AssertMainTabCount(1U);
}

// Tests that pressing the button on a POST-based form with same-page action
// does not change the page and that the back button works as expected
// afterwards.
- (void)testBrowsingPostToSamePage {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL firstURL = web::test::HttpServer::MakeUrl("http://first");
  const GURL formURL = web::test::HttpServer::MakeUrl("http://form");
  // This is just a page with some text.
  responses[firstURL] = "foo";
  // This is a page with at button that posts to the current URL.
  responses[formURL] =
      "<form method='post'>"
      "<input value='button' type='submit' id='button'></form>";
  web::test::SetUpSimpleHttpServer(responses);

  // Open the first URL so it's in history.
  [ChromeEarlGrey loadURL:firstURL];

  // Open the second URL, tap the button, and verify the browser navigates to
  // the expected URL.
  [ChromeEarlGrey loadURL:formURL];
  chrome_test_util::TapWebViewElementWithId("button");
  [[EarlGrey selectElementWithMatcher:OmniboxText(formURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go back once and verify the browser navigates to the form URL.
  [self goBack];
  [[EarlGrey selectElementWithMatcher:OmniboxText(formURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go back a second time and verify the browser navigates to the first URL.
  [self goBack];
  [[EarlGrey selectElementWithMatcher:OmniboxText(firstURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that evaluating user JavaScript that causes navigation correctly
// modifies history.
- (void)testBrowsingUserJavaScriptNavigation {
  // TODO(crbug.com/640220): Keyboard entry inside the omnibox fails only on
  // iPad running iOS 10.
  if (IsIPadIdiom() && base::ios::IsRunningOnIOS10OrLater())
    return;

  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL startURL = web::test::HttpServer::MakeUrl("http://startpage");
  responses[startURL] = "<html><body><p>Ready to begin.</p></body></html>";
  const GURL targetURL = web::test::HttpServer::MakeUrl("http://targetpage");
  responses[targetURL] = "<html><body><p>You've arrived!</p></body></html>";
  web::test::SetUpSimpleHttpServer(responses);

  // Load the first page and run JS (using the codepath that user-entered JS in
  // the omnibox would take, not page-triggered) that should navigate.
  [ChromeEarlGrey loadURL:startURL];

  NSString* script =
      [NSString stringWithFormat:@"javascript:window.location='%s'",
                                 targetURL.spec().c_str()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(script)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Go")]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForPageToFinishLoading];

  [[EarlGrey selectElementWithMatcher:OmniboxText(targetURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  [self goBack];
  [[EarlGrey selectElementWithMatcher:OmniboxText(startURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that evaluating non-navigation user JavaScript doesn't affect history.
- (void)testBrowsingUserJavaScriptWithoutNavigation {
  // TODO(crbug.com/640220): Keyboard entry inside the omnibox fails only on
  // iPad running iOS 10.
  if (IsIPadIdiom() && base::ios::IsRunningOnIOS10OrLater())
    return;

  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL firstURL = web::test::HttpServer::MakeUrl("http://firstURL");
  const std::string firstResponse = "Test Page 1";
  const GURL secondURL = web::test::HttpServer::MakeUrl("http://secondURL");
  const std::string secondResponse = "Test Page 2";
  responses[firstURL] = firstResponse;
  responses[secondURL] = secondResponse;
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:firstURL];
  [ChromeEarlGrey loadURL:secondURL];

  // Execute some JavaScript in the omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"javascript:document.write('foo')")];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Go")]
      performAction:grey_tap()];

  id<GREYMatcher> webView = chrome_test_util::WebViewContainingText("foo");
  [[EarlGrey selectElementWithMatcher:webView] assertWithMatcher:grey_notNil()];

  // Verify that the JavaScript did not affect history by going back and then
  // forward again.
  [self goBack];
  [[EarlGrey selectElementWithMatcher:OmniboxText(firstURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [self goForward];
  [[EarlGrey selectElementWithMatcher:OmniboxText(secondURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tap the text field indicated by |ID| to open the keyboard, and then
// press the keyboard's "Go" button.
- (void)openKeyboardAndTapGoButtonWithTextFieldID:(const std::string&)ID {
  // Disable EarlGrey's synchronization since it is blocked by opening the
  // keyboard from a web view.
  [[GREYConfiguration sharedInstance]
          setValue:@NO
      forConfigKey:kGREYConfigKeySynchronizationEnabled];

  // Wait for web view to be interactable before tapping.
  GREYCondition* interactableCondition = [GREYCondition
      conditionWithName:@"Wait for web view to be interactable."
                  block:^BOOL {
                    NSError* error = nil;
                    id<GREYMatcher> webViewMatcher = WebViewInWebState(
                        chrome_test_util::GetCurrentWebState());
                    [[EarlGrey selectElementWithMatcher:webViewMatcher]
                        assertWithMatcher:grey_interactable()
                                    error:&error];
                    return !error;
                  }];
  GREYAssert(
      [interactableCondition waitWithTimeout:testing::kWaitForUIElementTimeout],
      @"Web view did not become interactable.");

  web::WebState* currentWebState = chrome_test_util::GetCurrentWebState();
  [[EarlGrey selectElementWithMatcher:web::WebViewInWebState(currentWebState)]
      performAction:web::webViewTapElement(currentWebState, ID)];

  // Wait until the keyboard shows up before tapping.
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for the keyboard to show up."
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:GoButtonMatcher()]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return (error == nil);
                  }];
  GREYAssert([condition waitWithTimeout:10],
             @"No keyboard with 'Go' button showed up.");

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Go")]
      performAction:grey_tap()];

  // Reenable synchronization now that the keyboard has been closed.
  [[GREYConfiguration sharedInstance]
          setValue:@YES
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
}

// Tests that submitting a POST-based form by tapping the 'Go' button on the
// keyboard navigates to the correct URL and the back button works as expected
// afterwards.
- (void)testBrowsingPostEntryWithKeyboard {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://postEntryWithKeyboard");
  const GURL destinationURL = web::test::HttpServer::MakeUrl("http://foo");
  // This is a page this an input text field and a button that posts to the
  // destination.
  responses[URL] = base::StringPrintf(
      "hello!"
      "<form action='%s' method='post'>"
      "<input value='textfield' id='textfield' type='text'></label>"
      "<input type='submit'></form>",
      destinationURL.spec().c_str());
  // This is the page that should be showing at the end of the test.
  responses[destinationURL] = "baz!";
  web::test::SetUpSimpleHttpServer(responses);

  // Open the URL, focus the textfield,and submit via keyboard.
  [ChromeEarlGrey loadURL:URL];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText("hello!")]
      assertWithMatcher:grey_notNil()];

  [self openKeyboardAndTapGoButtonWithTextFieldID:"textfield"];

  // Verify that the browser navigates to the expected URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go back and verify that the browser navigates to the original URL.
  [self goBack];
  [[EarlGrey selectElementWithMatcher:OmniboxText(URL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

@end
