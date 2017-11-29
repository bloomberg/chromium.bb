// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test suite to verify Internet connectivity.
@interface DeviceCheckTestCase : ChromeTestCase
@end

@implementation DeviceCheckTestCase

// Verifies Internet connectivity by navigating to browsingtest.appspot.com.
- (void)testNetworkConnection {
  // TODO(crbug.com/763582) Remove EARL_GREY_TEST_DISABLED if
  // wifi tests running.
  EARL_GREY_TEST_DISABLED(@"Test disabled due to Wifi issue.");
  [ChromeEarlGrey loadURL:GURL("http://browsingtest.appspot.com")];
  [ChromeEarlGrey waitForWebViewContainingText:"Window1"];
}

@end
