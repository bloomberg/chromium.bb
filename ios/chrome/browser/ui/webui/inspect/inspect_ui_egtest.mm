// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/web/public/test/element_selector.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Id of the "Start Logging" button.
const char kStartLoggingButtonId[] = "start-logging";
// Id of the "Stop Logging" button.
const char kStopLoggingButtonId[] = "stop-logging";
}  // namespace

using web::test::ElementSelector;

// Test case for chrome://inspect WebUI page.
@interface InspectUITestCase : ChromeTestCase
@end

@implementation InspectUITestCase

// Tests that chrome://inspect allows the user to enable and disable logging.
- (void)testStartStopLogging {
  [ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)];

  ElementSelector startLoggingButton =
      ElementSelector::ElementSelectorId(kStartLoggingButtonId);
  [ChromeEarlGrey waitForWebViewContainingElement:startLoggingButton];

  [ChromeEarlGrey
      tapWebViewElementWithID:base::SysUTF8ToNSString(kStartLoggingButtonId)];

  ElementSelector stopLoggingButton =
      ElementSelector::ElementSelectorId(kStopLoggingButtonId);
  [ChromeEarlGrey waitForWebViewContainingElement:stopLoggingButton];

  [ChromeEarlGrey
      tapWebViewElementWithID:base::SysUTF8ToNSString(kStopLoggingButtonId)];

  [ChromeEarlGrey waitForWebViewContainingElement:startLoggingButton];
}

@end
