// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/third_party/earl_grey2/src/TestLib/EarlGreyImpl/EarlGrey.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// WebShellTestCase will temporarily hold EG2 versions of web shell tests, until
// the EG2 tests are running on the bots and the EG1 versions can be deleted.
@interface WebShellTestCase : XCTestCase
@end

@implementation WebShellTestCase

- (void)setUp {
  [super setUp];
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    XCUIApplication* application = [[XCUIApplication alloc] init];
    [application launch];
  });
}

@end
