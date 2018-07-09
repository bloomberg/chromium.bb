// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/test/scoped_feature_list.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/embedded_test_server_handlers.h"
#import "ios/testing/wait_util.h"
#include "ios/web/public/features.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;

namespace {

// Matcher for "Download" button on Download Manager UI.
id<GREYMatcher> DownloadButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD);
}

// Matcher for "Open In..." button on Download Manager UI.
id<GREYMatcher> OpenInButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_OPEN_IN);
}

// Provides downloads landing page with download link.
std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);
  result->set_content("<a id='download' href='/download?50000'>Download</a>");
  return result;
}

}  // namespace

// Tests critical user journeys for Download Manager.
@interface DownloadManagerTestCase : ChromeTestCase {
  base::test::ScopedFeatureList _featureList;
}
@end

@implementation DownloadManagerTestCase

- (void)setUp {
  [super setUp];

  _featureList.InitAndEnableFeature(web::features::kNewFileDownload);

  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/",
                          base::BindRepeating(&GetResponse)));

  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/download",
                          base::BindRepeating(&testing::HandleDownload)));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests sucessfull download up to the point where "Open in..." button is
// presented. EarlGreay does not allow testing "Open in..." dialog, because it
// is run in a separate process.
- (void)testSucessfullDownload {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebViewContainingText:"Download"];
  [ChromeEarlGrey tapWebViewElementWithID:@"download"];

  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  // Wait until Open in... button is shown.
  ConditionBlock openInShown = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:OpenInButton()]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return (error == nil);
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForDownloadTimeout, openInShown),
             @"Open in... button did not show up");
}

// Tests sucessfull download up to the point where "Open in..." button is
// presented. EarlGreay does not allow testing "Open in..." dialog, because it
// is run in a separate process. Performs download in Incognito.
- (void)testSucessfullDownloadInIncognito {
  chrome_test_util::OpenNewIncognitoTab();
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebViewContainingText:"Download"];
  [ChromeEarlGrey tapWebViewElementWithID:@"download"];

  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      performAction:grey_tap()];

  // Wait until Open in... button is shown.
  ConditionBlock openInShown = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:OpenInButton()]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return (error == nil);
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForDownloadTimeout, openInShown),
             @"Open in... button did not show up");
}

// Tests cancelling download UI.
- (void)testCancellingDownload {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebViewContainingText:"Download"];
  [ChromeEarlGrey tapWebViewElementWithID:@"download"];

  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::CloseButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DownloadButton()]
      assertWithMatcher:grey_nil()];
}

@end
