// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/testing/perf/startupLoggers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface StartupTestCase : XCTestCase
@end

@implementation StartupTestCase

// Simple test to launch the app and log startup data.
- (void)testColdStartUp {
  XCTAssertTrue(startup_loggers::LogData(@"testColdStartUp"));
}

@end
