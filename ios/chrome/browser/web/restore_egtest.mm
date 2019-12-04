// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/net/url_test_util.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::OmniboxText;
using chrome_test_util::ContentSuggestionCollectionView;
using chrome_test_util::BackButton;
using chrome_test_util::ForwardButton;

namespace {

// Path to two test pages, page1 and page2 with associated contents and titles.
const char kPageOnePath[] = "/page1.html";
const char kPageOneContent[] = "page 1 content";
const char kPageOneTitle[] = "page1";
const char kPageTwoPath[] = "/page2.html";
const char kPageTwoContent[] = "page 2 content";
const char kPageTwoTitle[] = "page 2";

// Path to a test page used to count each page load.
const char kCountURL[] = "/countme.html";

// Extended timeout used for restore page loads to account for navigating to
// the SlimNav placeholder and then the target page.
const CGFloat kRestoreTimeout = 10;

// Response handler for page1 and page2 that supports 'airplane mode' by
// returning an empty RawHttpResponse when |responds_with_content| us false.
std::unique_ptr<net::test_server::HttpResponse> RestoreResponse(
    const bool& responds_with_content,
    const net::test_server::HttpRequest& request) {
  if (!responds_with_content) {
    return std::make_unique<net::test_server::RawHttpResponse>(
        /*headers=*/"", /*contents=*/"");
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  std::string title;
  std::string body;
  if (request.relative_url == kPageOnePath) {
    title = kPageOneTitle;
    body = kPageOneContent;
  } else if (request.relative_url == kPageTwoPath) {
    title = kPageTwoTitle;
    body = kPageTwoContent;
  } else {
    return nullptr;
  }
  http_response->set_content("<html><head><title>" + title +
                             "</title></head>"
                             "<body>" +
                             body + "</body></html>");
  return std::move(http_response);
}

// Response handler for |kCountURL| that counts the number of page loads.
std::unique_ptr<net::test_server::HttpResponse> CountResponse(
    int* counter,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kCountURL) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content("<html><head><title>Hello World</title></head>"
                             "<body>Hello World!</body></html>");
  (*counter)++;
  return std::move(http_response);
}
}

// Integration tests for restoring session history.
@interface RestoreTestCase : ChromeTestCase {
  // Use a second test server to ensure different origin navigations.
  std::unique_ptr<net::EmbeddedTestServer> _secondTestServer;
}

// The secondary EmbeddedTestServer instance.
@property(nonatomic, readonly)
    net::test_server::EmbeddedTestServer* secondTestServer;

@property(atomic) bool serverRespondsWithContent;

// Start the primary and secondary test server.  Separate servers are used to
// force cross domain tests (via different ports).
- (void)setUpRestoreServers;

// Trigger a session history restore.  In EG1 this is possible via the TabGrid
// CloseAll-Undo-Done method. In EG2, this is possible via
// Background-Terminate-Activate
- (void)triggerRestore;

// Navigate to a set of sites include cross-domains, chrome URLs, error pages
// and the NTP.
- (void)loadTestPages;

// Verify that each page visited in -loadTestPages is properly restored by
// navigating to each page and triggering a restore, confirming that pages are
// reloaded and back-forward history is preserved.  If |checkServerData| is YES,
// also check that the proper content is restored.
- (void)verifyRestoredTestPages:(BOOL)checkServerData;

@end

@implementation RestoreTestCase

- (net::EmbeddedTestServer*)secondTestServer {
  if (!_secondTestServer) {
    _secondTestServer = std::make_unique<net::EmbeddedTestServer>();
    NSString* bundlePath = [NSBundle bundleForClass:[self class]].resourcePath;
    _secondTestServer->ServeFilesFromDirectory(
        base::FilePath(base::SysNSStringToUTF8(bundlePath))
            .AppendASCII("ios/testing/data/http_server_files/"));
    net::test_server::RegisterDefaultHandlers(_secondTestServer.get());
  }
  return _secondTestServer.get();
}

// Navigates to a set of cross-domains, chrome URLs and error pages, and then
// tests that they are properly restored.
- (void)testRestoreHistory {
  [self setUpRestoreServers];
  [self loadTestPages];
  [self verifyRestoredTestPages:YES];
}

// Navigates to a set of cross-domains, chrome URLs and error pages, and then
// tests that they are properly restored in airplane mode.
- (void)testRestoreNoNetwork {
  [self setUpRestoreServers];
  [self loadTestPages];
  self.serverRespondsWithContent = false;
  [self verifyRestoredTestPages:NO];
}

// Tests that only the selected web state is loaded on a session restore.
- (void)testRestoreOneWebstateOnly {
  // Visit the background page.
  int visitCounter = 0;
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&CountResponse, &visitCounter));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL countPage = self.testServer->GetURL(kCountURL);
  [ChromeEarlGrey loadURL:countPage];
  GREYAssertEqual(1, visitCounter, @"The page should have been loaded once");

  // Visit the forground page.
  [ChromeEarlGrey openNewTab];
  const GURL echoPage = self.testServer->GetURL("/echo");
  [ChromeEarlGrey loadURL:echoPage];

  // Trigger a restore and confirm the background page is not reloaded.
  [self triggerRestore];
  [[EarlGrey selectElementWithMatcher:OmniboxText(echoPage.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
  GREYAssertEqual(1, visitCounter, @"The page should not reload");
}

// Tests that only the selected web state is loaded Restore-after-Crash.  This
// is only possible in EG2.
- (void)testRestoreOneWebstateOnlyAfterCrash {
#if defined(CHROME_EARL_GREY_2)
  // Visit the background page.
  int visitCounter = 0;
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&CountResponse, &visitCounter));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL countPage = self.testServer->GetURL(kCountURL);
  [ChromeEarlGrey loadURL:countPage];
  GREYAssertEqual(1, visitCounter, @"The page should have been loaded once");

  // Visit the foreground page.
  [ChromeEarlGrey openNewTab];
  const GURL echoPage = self.testServer->GetURL("/echo");
  [ChromeEarlGrey loadURL:echoPage];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  // Clear cache, save the session and trigger a crash/activate.
  [ChromeEarlGrey removeBrowsingCache];
  [ChromeEarlGrey saveSessionImmediately];
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithFeaturesEnabled:{}
      disabled:{}
      relaunchPolicy:ForceRelaunchByKilling];
  // Restore after crash and confirm the background page is not reloaded.
  [[EarlGrey selectElementWithMatcher:grey_text(@"Restore")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OmniboxText(echoPage.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
  GREYAssertEqual(1, visitCounter, @"The page should not reload");
#endif
}

#pragma mark Utility methods

- (void)setUpRestoreServers {
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &RestoreResponse, std::cref(_serverRespondsWithContent)));
  self.secondTestServer->RegisterRequestHandler(base::BindRepeating(
      &RestoreResponse, std::cref(_serverRespondsWithContent)));
  self.serverRespondsWithContent = true;
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  GREYAssertTrue(self.secondTestServer->Start(),
                 @"Second test server failed to start.");
}

- (void)triggerRestore {
#if defined(CHROME_EARL_GREY_1)
  [ChromeEarlGrey triggerRestoreViaTabGridRemoveAllUndo];
#elif defined(CHROME_EARL_GREY_2)
  [ChromeEarlGrey saveSessionImmediately];
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithFeaturesEnabled:{}
      disabled:{}
      relaunchPolicy:ForceRelaunchByCleanShutdown];
#endif  // defined(CHROME_EARL_GREY_2)
}

- (void)loadTestPages {
  // Load page1.
  const GURL pageOne = self.testServer->GetURL(kPageOnePath);
  [ChromeEarlGrey loadURL:pageOne];
  [ChromeEarlGrey waitForWebStateContainingText:kPageOneContent];

  // Load chrome url
  const GURL chromePage = GURL("chrome://chrome-urls");
  [ChromeEarlGrey loadURL:chromePage];

  // Load error page.
  const GURL errorPage = GURL("http://ndtv1234.com");
  [ChromeEarlGrey loadURL:errorPage];
  [ChromeEarlGrey waitForWebStateContainingText:"ERR_"];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Load page2.
  const GURL pageTwo = self.secondTestServer->GetURL(kPageTwoPath);
  [ChromeEarlGrey loadURL:pageTwo];
  [ChromeEarlGrey waitForWebStateContainingText:kPageTwoContent];
}

- (void)verifyRestoredTestPages:(BOOL)checkServerData {
  const GURL pageOne = self.testServer->GetURL(kPageOnePath);
  const GURL pageTwo = self.secondTestServer->GetURL(kPageTwoPath);

  // Restore page2
  [self triggerRestore];
  [[EarlGrey selectElementWithMatcher:OmniboxText(pageTwo.GetContent())]
      assertWithMatcher:grey_notNil()];
  if (checkServerData) {
    [ChromeEarlGrey waitForWebStateContainingText:kPageTwoContent
                                          timeout:kRestoreTimeout];
  }

  // Confirm page1 is still in the history.
  [[EarlGrey selectElementWithMatcher:BackButton()]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:grey_text(base::SysUTF8ToNSString(
                                          kPageOneTitle))]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];

  // Go back to error page.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OmniboxText("ndtv1234.com")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebStateContainingText:"ERR_" timeout:kRestoreTimeout];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  [self triggerRestore];
  [[EarlGrey selectElementWithMatcher:OmniboxText("ndtv1234.com")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebStateContainingText:"ERR_" timeout:kRestoreTimeout];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Go back to chrome url.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OmniboxText("chrome://chrome-urls")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebStateContainingText:"List of Chrome"
                                        timeout:kRestoreTimeout];
  [self triggerRestore];
  [[EarlGrey selectElementWithMatcher:OmniboxText("chrome://chrome-urls")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebStateContainingText:"List of Chrome"
                                        timeout:kRestoreTimeout];

  // Go back to page1 and confirm page2 is still in the forward history.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OmniboxText(pageOne.GetContent())]
      assertWithMatcher:grey_notNil()];
  if (checkServerData) {
    [ChromeEarlGrey waitForWebStateContainingText:kPageOneContent
                                          timeout:kRestoreTimeout];
    [[EarlGrey selectElementWithMatcher:ForwardButton()]
        performAction:grey_longPress()];
    [[EarlGrey selectElementWithMatcher:grey_text(base::SysUTF8ToNSString(
                                            kPageTwoTitle))]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:ForwardButton()]
        performAction:grey_tap()];
  }
  [self triggerRestore];
  [[EarlGrey selectElementWithMatcher:OmniboxText(pageOne.GetContent())]
      assertWithMatcher:grey_notNil()];
  if (checkServerData) {
    [ChromeEarlGrey waitForWebStateContainingText:kPageOneContent
                                          timeout:kRestoreTimeout];
    [[EarlGrey selectElementWithMatcher:ForwardButton()]
        performAction:grey_longPress()];
    [[EarlGrey selectElementWithMatcher:grey_text(base::SysUTF8ToNSString(
                                            kPageTwoTitle))]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:ForwardButton()]
        performAction:grey_tap()];
  }
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Confirm the NTP is still at the start.
  [[EarlGrey selectElementWithMatcher:ContentSuggestionCollectionView()]
      assertWithMatcher:grey_notNil()];
  [self triggerRestore];
  [[EarlGrey selectElementWithMatcher:ContentSuggestionCollectionView()]
      assertWithMatcher:grey_notNil()];
}

@end
