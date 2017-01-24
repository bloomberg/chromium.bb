// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#include "ios/web/public/test/response_providers/data_response_provider.h"
#include "ui/base/l10n/l10n_util.h"

using chrome_test_util::BackButton;
using chrome_test_util::ForwardButton;
using chrome_test_util::TapWebViewElementWithId;
using chrome_test_util::WebViewContainingText;

namespace {

// URL for the test window.history.go() test file.  The page at this URL
// contains several buttons that trigger window.history commands.  Additionally
// the page contains several divs used to display the state of the page:
// - A div that is populated with |kOnLoadText| when the onload event fires.
// - A div that is populated with |kNoOpText| 1s after a button is tapped.
// - A div that is populated with |kPopStateReceivedText| when a popstate event
//   is received by the page.
// - A div that is populated with the state object (if it's a string) upon the
//   receipt of a popstate event.
// - A div that is populated with |kHashChangeReceivedText| when a hashchange
//   event is received.
// When a button on the page is tapped, all pre-existing div text is cleared,
// so matching against this webview text after a button is tapped ensures that
// the state is set in response to the most recently executed script.
const char kWindowHistoryGoTestURL[] =
    "http://ios/testing/data/http_server_files/history_go.html";

// URL of a sample file-based page.
const char kSampleFileBasedURL[] =
    "http://ios/testing/data/http_server_files/chromium_logo_page.html";

// Strings used by history_go.html.
const char kOnLoadText[] = "OnLoadText";
const char kNoOpText[] = "NoOpText";

// Button ids for history_go.html.
NSString* const kGoNoParameterID = @"go-no-parameter";
NSString* const kGoZeroID = @"go-zero";
NSString* const kGoTwoID = @"go-2";
NSString* const kGoBackTwoID = @"go-back-2";

// URLs and labels for tests that navigate back and forward.
const char kBackHTMLButtonLabel[] = "BackHTMLButton";
const char kForwardHTMLButtonLabel[] = "ForwardHTMLButton";
const char kForwardHTMLSentinel[] = "Forward page loaded";
const char kTestPageSentinel[] = "Test Page";
const char kBackURL[] = "http://back";
const char kForwardURL[] = "http://forward";
const char kTestURL[] = "http://test";

// URLs and labels for scenarioWindowLocation* tests.
const char kHashChangeWithHistoryLabel[] = "hashChangedWithHistory";
const char kHashChangeWithoutHistoryLabel[] = "hashChangedWithoutHistory";
const char kPage1URL[] = "http://page1";
const char kHashChangedWithHistoryURL[] =
    "http://page1/#hashChangedWithHistory";
const char kHashChangedWithoutHistoryURL[] =
    "http://page1/#hashChangedWithoutHistory";
const char kNoHashChangeText[] = "No hash change";
// An HTML page with two links that run JavaScript when they're clicked. The
// first link updates |window.location.hash|, the second link changes
// |window.location|.
const char kHashChangedHTML[] =
    "<html><body>"
    "<a href='javascript:window.location.hash=\"#hashChangedWithHistory\"' "
    "   id=\"hashChangedWithHistory\"'>hashChangedWithHistory</a><br />"
    "<a href='javascript:"
    "           window.location.replace(\"#hashChangedWithoutHistory\")' "
    "   id=\"hashChangedWithoutHistory\">hashChangedWithoutHistory</a>"
    "</body></html>";

void SetupBackAndForwardResponseProvider() {
  std::map<GURL, std::string> responses;
  GURL testURL = web::test::HttpServer::MakeUrl(kTestURL);
  GURL backURL = web::test::HttpServer::MakeUrl(kBackURL);
  GURL forwardURL = web::test::HttpServer::MakeUrl(kForwardURL);
  responses[testURL] = "<html>Test Page</html>";
  responses[backURL] =
      "<html>"
      "<input type=\"button\" value=\"BackHTMLButton\" id=\"BackHTMLButton\""
      "onclick=\"window.history.back()\" />"
      "</html>";
  responses[forwardURL] =
      "<html>"
      "<input type=\"button\" value=\"ForwardHTMLButton\""
      "id=\"ForwardHTMLButton\" onclick=\"window.history.forward()\" /></br>"
      "Forward page loaded</html>";
  web::test::SetUpSimpleHttpServer(responses);
}

// Matcher for the error page.
// TODO(crbug.com/638674): Evaluate if this can move to shared code. See
// ios/chrome/browser/ui/error_page_egtest.mm.
id<GREYMatcher> ErrorPage() {
  NSString* const kDNSError =
      l10n_util::GetNSString(IDS_ERRORPAGES_HEADING_NOT_AVAILABLE);
  NSString* const kInternetDisconnectedError =
      l10n_util::GetNSString(IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED);
  return grey_anyOf(chrome_test_util::StaticHtmlViewContainingText(kDNSError),
                    chrome_test_util::StaticHtmlViewContainingText(
                        kInternetDisconnectedError),
                    nil);
}

// URLs for server redirect tests.
const char kRedirectIndexURL[] = "http://redirect";
const char kRedirect301URL[] = "http://redirect/redirect?code=301";
const char kRedirectWindowURL[] = "http://redirect/redirectWindow.html";
const char kRedirectRefreshURL[] = "http://redirect/redirectRefresh.html";
const char kDestinationURL[] = "http://redirect/destination.html";
const char kLastURL[] = "http://redirect/last.html";

class RedirectResponseProvider : public web::DataResponseProvider {
 public:
  RedirectResponseProvider()
      : index_url_(web::test::HttpServer::MakeUrl(kRedirectIndexURL)),
        redirect_301_url_(web::test::HttpServer::MakeUrl(kRedirect301URL)),
        redirect_refresh_url_(
            web::test::HttpServer::MakeUrl(kRedirectRefreshURL)),
        redirect_window_url_(
            web::test::HttpServer::MakeUrl(kRedirectWindowURL)),
        destination_url_(web::test::HttpServer::MakeUrl(kDestinationURL)) {}

 private:
  bool CanHandleRequest(const Request& request) override {
    return request.url == index_url_ || request.url == redirect_window_url_ ||
           request.url == redirect_refresh_url_ ||
           request.url == redirect_301_url_ || request.url == destination_url_;
  }
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    *headers = GetDefaultResponseHeaders();
    if (request.url == index_url_) {
      *response_body =
          "<p><a href=\"redirect?code=301\""
          "      id=\"redirect301\">redirect301</a></p>"
          "<p><a href=\"redirectRefresh.html\""
          "      id=\"redirectRefresh\">redirectRefresh</a></p>"
          "<p><a href=\"redirectWindow.html\""
          "      id=\"redirectWindow\">redirectWindow</a></p>";
    } else if (request.url == redirect_301_url_) {
      *headers = GetRedirectResponseHeaders(destination_url_.spec(),
                                            net::HTTP_MOVED_PERMANENTLY);
    } else if (request.url == redirect_refresh_url_) {
      *response_body =
          "<head>"
          "  <meta HTTP-EQUIV=\"REFRESH\" content=\"0; url=destination.html\">"
          "</head>"
          "<body><p>Redirecting</p></body>";
    } else if (request.url == redirect_window_url_) {
      *response_body =
          "<head>"
          "  <meta HTTP-EQUIV=\"REFRESH\" content=\"0; url=destination.html\">"
          "</head>"
          "<body>Redirecting"
          "  <script>window.open(\"destination.html\", \"_self\");</script>"
          "</body>";
    } else if (request.url == destination_url_) {
      *response_body = "<html><body><p>You've arrived</p></body></html>";
    } else if (request.url == last_url_) {
      *response_body = "<html><body><p>Go back from here</p></body></html>";
    } else {
      NOTREACHED();
    }
  }

  // Member variables for test URLs.
  const GURL index_url_;
  const GURL redirect_301_url_;
  const GURL redirect_refresh_url_;
  const GURL redirect_window_url_;
  const GURL destination_url_;
  const GURL last_url_;
};

}  // namespace

// Integration tests for navigating history via JavaScript and the forward and
// back buttons.
@interface NavigationTestCase : ChromeTestCase

// Adds hashchange listener to the page that changes the inner html of the page
// to |content| when a hashchange is detected.
- (void)addHashChangeListenerWithContent:(std::string)content;

// Loads index page for redirect operations, taps the link with |redirectLabel|
// and then perform series of back-forward navigations asserting the proper
// behavior.
- (void)verifyBackAndForwardAfterRedirect:(std::string)redirectLabel;

@end

@implementation NavigationTestCase

#pragma mark window.history.go operations

// Tests reloading the current page via window.history.go() with no parameters.
- (void)testHistoryGoNoParameter {
  web::test::SetUpFileBasedHttpServer();

  // Load the history test page and ensure that its onload text is visible.
  const GURL windowHistoryURL =
      web::test::HttpServer::MakeUrl(kWindowHistoryGoTestURL);
  [ChromeEarlGrey loadURL:windowHistoryURL];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kOnLoadText)]
      assertWithMatcher:grey_notNil()];

  // Tap on the window.history.go() button.  This will clear |kOnLoadText|, so
  // the subsequent check for |kOnLoadText| will only pass if a reload has
  // occurred.
  [ChromeEarlGrey tapWebViewElementWithID:kGoNoParameterID];

  // Verify that the onload text is reset.
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kOnLoadText)]
      assertWithMatcher:grey_notNil()];
}

// Tests reloading the current page via history.go(0).
- (void)testHistoryGoDeltaZero {
  web::test::SetUpFileBasedHttpServer();

  // Load the history test page and ensure that its onload text is visible.
  const GURL windowHistoryURL =
      web::test::HttpServer::MakeUrl(kWindowHistoryGoTestURL);
  [ChromeEarlGrey loadURL:windowHistoryURL];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kOnLoadText)]
      assertWithMatcher:grey_notNil()];

  // Tap on the window.history.go() button.  This will clear |kOnLoadText|, so
  // the subsequent check for |kOnLoadText| will only pass if a reload has
  // occurred.
  [ChromeEarlGrey tapWebViewElementWithID:kGoZeroID];

  // Verify that the onload text is reset.
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kOnLoadText)]
      assertWithMatcher:grey_notNil()];
}

// Tests that calling window.history.go() with an offset that is out of bounds
// is a no-op.
- (void)testHistoryGoOutOfBounds {
  web::test::SetUpFileBasedHttpServer();

  // Load the history test page and ensure that its onload text is visible.
  const GURL windowHistoryURL =
      web::test::HttpServer::MakeUrl(kWindowHistoryGoTestURL);
  [ChromeEarlGrey loadURL:windowHistoryURL];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kOnLoadText)]
      assertWithMatcher:grey_notNil()];

  // Tap on the window.history.go(2) button.  This will clear all div text, so
  // the subsequent check for |kNoOpText| will only pass if no navigations have
  // occurred.
  [ChromeEarlGrey tapWebViewElementWithID:kGoTwoID];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kNoOpText)]
      assertWithMatcher:grey_notNil()];

  // Tap on the window.history.go(-2) button.  This will clear all div text, so
  // the subsequent check for |kNoOpText| will only pass if no navigations have
  // occurred.
  [ChromeEarlGrey tapWebViewElementWithID:kGoBackTwoID];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kNoOpText)]
      assertWithMatcher:grey_notNil()];
}

// Tests going back and forward via history.go().
- (void)testHistoryGoDelta {
  std::map<GURL, std::string> responses;
  const GURL firstURL = web::test::HttpServer::MakeUrl("http://page1");
  const GURL secondURL = web::test::HttpServer::MakeUrl("http://page2");
  const GURL thirdURL = web::test::HttpServer::MakeUrl("http://page3");
  const GURL fourthURL = web::test::HttpServer::MakeUrl("http://page4");
  responses[firstURL] =
      "page1 <input type='button' value='goForward' id='goForward' "
      "onclick='window.history.go(2)' />";
  responses[secondURL] = "page2";
  responses[thirdURL] = "page3";
  responses[fourthURL] =
      "page4 <input type='button' value='goBack' id='goBack' "
      "onclick='window.history.go(-3)' />";
  web::test::SetUpSimpleHttpServer(responses);

  // Load 4 pages.
  [ChromeEarlGrey loadURL:firstURL];
  [ChromeEarlGrey loadURL:secondURL];
  [ChromeEarlGrey loadURL:thirdURL];
  [ChromeEarlGrey loadURL:fourthURL];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText("page4")]
      assertWithMatcher:grey_notNil()];

  // Tap button to go back 3 pages.
  TapWebViewElementWithId("goBack");
  [[EarlGrey selectElementWithMatcher:WebViewContainingText("page1")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          firstURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Tap button to go forward 2 pages.
  TapWebViewElementWithId("goForward");
  [[EarlGrey selectElementWithMatcher:WebViewContainingText("page3")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          thirdURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that calls to window.history.go() that span multiple documents causes
// a load to occur.
- (void)testHistoryCrossDocumentLoad {
  web::test::SetUpFileBasedHttpServer();

  // Load the history test page and ensure that its onload text is visible.
  const GURL windowHistoryURL =
      web::test::HttpServer::MakeUrl(kWindowHistoryGoTestURL);
  [ChromeEarlGrey loadURL:windowHistoryURL];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kOnLoadText)]
      assertWithMatcher:grey_notNil()];

  const GURL sampleURL = web::test::HttpServer::MakeUrl(kSampleFileBasedURL);
  [ChromeEarlGrey loadURL:sampleURL];

  [ChromeEarlGrey loadURL:windowHistoryURL];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kOnLoadText)]
      assertWithMatcher:grey_notNil()];

  // Tap the window.history.go(-2) button.  This will clear the current page's
  // |kOnLoadText|, so the subsequent check will only pass if another load
  // occurs.
  [ChromeEarlGrey tapWebViewElementWithID:kGoBackTwoID];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kOnLoadText)]
      assertWithMatcher:grey_notNil()];
}

#pragma mark window.history.[back/forward] operations

// Tests going back via history.back() then forward via forward button.
- (void)testHistoryBackNavigation {
  SetupBackAndForwardResponseProvider();

  // Navigate to a URL.
  const GURL firstURL = web::test::HttpServer::MakeUrl(kTestURL);
  [ChromeEarlGrey loadURL:firstURL];

  // Navigate to an HTML page with a back button.
  const GURL secondURL = web::test::HttpServer::MakeUrl(kBackURL);
  [ChromeEarlGrey loadURL:secondURL];

  // Tap the back button in the HTML and verify the first URL is loaded.
  TapWebViewElementWithId(kBackHTMLButtonLabel);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          firstURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Tap the forward button in the toolbar and verify the second URL is loaded.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          secondURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests going back via back button then forward via history.forward().
- (void)testHistoryForwardNavigation {
  SetupBackAndForwardResponseProvider();

  // Navigate to an HTML page with a forward button.
  const GURL firstURL = web::test::HttpServer::MakeUrl(kForwardURL);
  [ChromeEarlGrey loadURL:firstURL];

  // Navigate to some other page.
  const GURL secondURL = web::test::HttpServer::MakeUrl(kTestURL);
  [ChromeEarlGrey loadURL:secondURL];

  // Tap the back button in the toolbar and verify the page with forward button
  // is loaded.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText(kForwardHTMLSentinel)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          firstURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Tap the forward button in the HTML and verify the second URL is loaded.
  TapWebViewElementWithId(kForwardHTMLButtonLabel);
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kTestPageSentinel)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          secondURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Verify that the forward button is not enabled.
  // TODO(crbug.com/638674): Evaluate if size class determination can move to
  // shared code.
  if (UIApplication.sharedApplication.keyWindow.traitCollection
          .horizontalSizeClass == UIUserInterfaceSizeClassCompact) {
    // In horizontally compact environments, the forward button is not visible.
    [[EarlGrey selectElementWithMatcher:ForwardButton()]
        assertWithMatcher:grey_nil()];
  } else {
    // In horizontally regular environments, the forward button is visible and
    // disabled.
    id<GREYMatcher> disabledForwardButton = grey_allOf(
        ForwardButton(),
        grey_accessibilityTrait(UIAccessibilityTraitNotEnabled), nil);
    [[EarlGrey selectElementWithMatcher:disabledForwardButton]
        assertWithMatcher:grey_notNil()];
  }
}

// Tests navigating forward via window.history.forward() to an error page.
- (void)testHistoryForwardToErrorPage {
  SetupBackAndForwardResponseProvider();

  // Go to page 1 with a button which calls window.history.forward().
  const GURL forwardURL = web::test::HttpServer::MakeUrl(kForwardURL);
  [ChromeEarlGrey loadURL:forwardURL];

  // Go to page 2: 'www.badurljkljkljklfloofy.com'. This page should display a
  // page not available error.
  const GURL badURL("http://www.badurljkljkljklfloofy.com");
  [ChromeEarlGrey loadURL:badURL];
  [[EarlGrey selectElementWithMatcher:ErrorPage()]
      assertWithMatcher:grey_notNil()];

  // Go back to page 1 by clicking back button.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          forwardURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go forward to page 2 by calling window.history.forward() and assert that
  // the error page is shown.
  TapWebViewElementWithId(kForwardHTMLButtonLabel);
  [[EarlGrey selectElementWithMatcher:ErrorPage()]
      assertWithMatcher:grey_notNil()];
}

#pragma mark window.location.hash operations

// Loads a URL and modifies window.location.hash, then goes back and forward
// and verifies the URLs and that hashchange event is fired.
- (void)testWindowLocationChangeHash {
  std::map<GURL, std::string> responses;
  const GURL page1URL = web::test::HttpServer::MakeUrl(kPage1URL);
  const GURL hashChangedWithHistoryURL =
      web::test::HttpServer::MakeUrl(kHashChangedWithHistoryURL);
  responses[page1URL] = kHashChangedHTML;
  responses[hashChangedWithHistoryURL] = kHashChangedHTML;
  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:page1URL];

  // Click link to update location.hash and go to new URL (same page).
  chrome_test_util::TapWebViewElementWithId(kHashChangeWithHistoryLabel);

  // Navigate back to original URL. This should fire a hashchange event.
  std::string backHashChangeContent = "backHashChange";
  [self addHashChangeListenerWithContent:backHashChangeContent];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          page1URL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText(backHashChangeContent)]
      assertWithMatcher:grey_notNil()];

  // Navigate forward to the new URL. This should fire a hashchange event.
  std::string forwardHashChangeContent = "forwardHashChange";
  [self addHashChangeListenerWithContent:forwardHashChangeContent];
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(
                                   hashChangedWithHistoryURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText(forwardHashChangeContent)]
      assertWithMatcher:grey_notNil()];

  // Load a hash URL directly. This shouldn't fire a hashchange event.
  std::string hashChangeContent = "FAIL_loadUrlHashChange";
  [self addHashChangeListenerWithContent:hashChangeContent];
  [ChromeEarlGrey loadURL:hashChangedWithHistoryURL];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(hashChangeContent)]
      assertWithMatcher:grey_nil()];
}

// Loads a URL and replaces its location, then updates its location.hash
// and verifies that going back returns to the replaced entry.
- (void)testWindowLocationReplaceAndChangeHash {
  std::map<GURL, std::string> responses;
  const GURL page1URL = web::test::HttpServer::MakeUrl(kPage1URL);
  const GURL hashChangedWithoutHistoryURL =
      web::test::HttpServer::MakeUrl(kHashChangedWithoutHistoryURL);
  const GURL hashChangedWithHistoryURL =
      web::test::HttpServer::MakeUrl(kHashChangedWithHistoryURL);
  responses[page1URL] = kHashChangedHTML;
  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:page1URL];

  // Tap link to replace the location value.
  TapWebViewElementWithId(kHashChangeWithoutHistoryLabel);
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(
                                   hashChangedWithoutHistoryURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Tap link to update the location.hash with a new value.
  TapWebViewElementWithId(kHashChangeWithHistoryLabel);
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(
                                   hashChangedWithHistoryURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Navigate back and verify that the URL that replaced window.location
  // has been reached.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(
                                   hashChangedWithoutHistoryURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Loads a URL and modifies window.location.hash twice, verifying that there is
// only one entry in the history by navigating back.
- (void)testWindowLocationChangeToSameHash {
  std::map<GURL, std::string> responses;
  const GURL page1URL = web::test::HttpServer::MakeUrl(kPage1URL);
  const GURL hashChangedWithHistoryURL =
      web::test::HttpServer::MakeUrl(kHashChangedWithHistoryURL);
  responses[page1URL] = kHashChangedHTML;
  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:page1URL];

  // Tap link to update location.hash with a new value.
  TapWebViewElementWithId(kHashChangeWithHistoryLabel);
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(
                                   hashChangedWithHistoryURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Tap link to update location.hash with the same value.
  TapWebViewElementWithId(kHashChangeWithHistoryLabel);

  // Tap back once to return to original URL.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          page1URL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Navigate forward and verify the URL.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(
                                   hashChangedWithHistoryURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

#pragma mark Redirect operations

// Navigates to a page that immediately redirects to another page via JavaScript
// then verifies the browsing history.
- (void)testJavaScriptRedirect {
  std::map<GURL, std::string> responses;
  // A starting page.
  const GURL initialURL = web::test::HttpServer::MakeUrl("http://initialURL");
  // A page that redirects immediately via the window.open JavaScript method.
  const GURL originURL = web::test::HttpServer::MakeUrl(
      "http://scenarioJavaScriptRedirect_origin");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  responses[initialURL] = "<html><body>Initial page</body></html>";
  responses[originURL] =
      "<script>window.open('" + destinationURL.spec() + "', '_self');</script>";
  responses[destinationURL] = "scenarioJavaScriptRedirect destination";

  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey loadURL:originURL];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Navigating back takes the user to the new tab page.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          initialURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Navigating forward take the user to destination page.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Test to load a page that contains a redirect window, then does multiple back
// and forth navigations.
- (void)testRedirectWindow {
  std::unique_ptr<web::DataResponseProvider> provider(
      new RedirectResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));
  [self verifyBackAndForwardAfterRedirect:"redirectWindow"];
}

// Test to load a page that contains a redirect refresh, then does multiple back
// and forth navigations.
- (void)testRedirectRefresh {
  std::unique_ptr<web::DataResponseProvider> provider(
      new RedirectResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));
  [self verifyBackAndForwardAfterRedirect:"redirectRefresh"];
}

// Test to load a page that performs a 301 redirect, then does multiple back and
// forth navigations.
- (void)test301Redirect {
  std::unique_ptr<web::DataResponseProvider> provider(
      new RedirectResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));
  [self verifyBackAndForwardAfterRedirect:"redirect301"];
}

#pragma mark Utility methods

- (void)addHashChangeListenerWithContent:(std::string)content {
  NSString* const script =
      [NSString stringWithFormat:
                    @"document.body.innerHTML = '%s';"
                     "window.addEventListener('hashchange', function(event) {"
                     "   document.body.innerHTML = '%s';"
                     "});",
                    kNoHashChangeText, content.c_str()];

  NSError* error = nil;
  chrome_test_util::ExecuteJavaScript(script, &error);
}

- (void)verifyBackAndForwardAfterRedirect:(std::string)redirectLabel {
  const GURL indexURL(web::test::HttpServer::MakeUrl(kRedirectIndexURL));
  const GURL destinationURL(web::test::HttpServer::MakeUrl(kDestinationURL));
  const GURL lastURL(web::test::HttpServer::MakeUrl(kLastURL));

  // Load index, tap on redirect link, and assert that the page is redirected
  // to the proper destination.
  [ChromeEarlGrey loadURL:indexURL];
  TapWebViewElementWithId(redirectLabel);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Navigate to a new URL, navigate back and assert that the resulting page is
  // the proper destination.
  [ChromeEarlGrey loadURL:lastURL];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Navigate back and assert that the resulting page is the initial index.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          indexURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Navigate forward and assert the the resulting page is the proper
  // destination.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

@end
