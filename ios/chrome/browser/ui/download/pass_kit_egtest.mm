// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/memory/ptr_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/wait_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::WaitUntilConditionOrTimeout;
using testing::kWaitForDownloadTimeout;

namespace {

// Returns matcher for PassKit error infobar.
id<GREYMatcher> PassKitErrorInfobar() {
  using l10n_util::GetNSStringWithFixup;
  NSString* label = GetNSStringWithFixup(IDS_IOS_GENERIC_PASSKIT_ERROR);
  return grey_accessibilityLabel(label);
}

// PassKit landing page and download request handler.
std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request) {
  auto result = base::MakeUnique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);

  if (request.GetURL().path() == "/") {
    result->set_content("<a id='bad' href='/bad'>Bad</a>");
  } else if (request.GetURL().path() == "/bad") {
    result->AddCustomHeader("Content-Type", "application/vnd.apple.pkpass");
    result->set_content("corrupted");
  }

  return result;
}

}  // namespace

// Tests PassKit file download.
@interface PassKitEGTest : ChromeTestCase
@end

@implementation PassKitEGTest

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(base::Bind(&GetResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that Chrome presents PassKit error infobar if PassKit file cannot be
// parsed.
- (void)testPassKitParsingError {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebViewContainingText:"Bad"];
  [ChromeEarlGrey tapWebViewElementWithID:@"bad"];

  bool infobarShown = WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:PassKitErrorInfobar()]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return (error == nil);
  });
  GREYAssert(infobarShown, @"PassKit error infobar was not shown");
}

@end
