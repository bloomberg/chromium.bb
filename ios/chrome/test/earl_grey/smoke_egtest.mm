// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/app/main_application_delegate_testing.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/app/main_controller_private.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ui/base/l10n/l10n_util.h"

@interface BrowserViewController (TabStripTest)
- (int)currentTabModelCount;
@end
@implementation BrowserViewController (TabStripTest)
- (int)currentTabModelCount {
  return [self tabModel].count;
}
@end

@interface SmokeTestCase : ChromeTestCase
@end

@implementation SmokeTestCase

// These aren't 'real' tests so please don't copy these.  They are just to get
// plumbing working for EG/XCTests.
- (void)testTapToolsMenu {
  NSString* menu = l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(menu)]
      performAction:grey_tap()];
}

- (void)testTabs {
  MainController* mainController =
      [MainApplicationDelegate sharedMainController];
  BrowserViewController* bvc =
      [[mainController browserViewInformation] currentBVC];
  XCTAssertEqual(1, [bvc currentTabModelCount]);
}
@end
