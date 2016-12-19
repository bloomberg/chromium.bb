// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_util.h"

#import <EarlGrey/EarlGrey.h>

#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"

namespace {
const NSTimeInterval kWaitForToolbarAnimationTimeout = 1.0;
}  // namespace

namespace chrome_test_util {

void AssertToolbarNotVisible() {
  ConditionBlock condition = ^{
    id<GREYMatcher> toolsMenuButton =
        grey_allOf(grey_accessibilityID(kToolbarToolsMenuButtonIdentifier),
                   grey_sufficientlyVisible(), nil);
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:toolsMenuButton]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error != nil;
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 kWaitForToolbarAnimationTimeout, condition),
             @"The toolbar is still visible.");
}

void AssertToolbarVisible() {
  ConditionBlock condition = ^{
    id<GREYMatcher> toolsMenuButton =
        grey_allOf(grey_accessibilityID(kToolbarToolsMenuButtonIdentifier),
                   grey_sufficientlyVisible(), nil);
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:toolsMenuButton]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 kWaitForToolbarAnimationTimeout, condition),
             @"The toolbar is not visible.");
}

}  // namespace chrome_test_util
