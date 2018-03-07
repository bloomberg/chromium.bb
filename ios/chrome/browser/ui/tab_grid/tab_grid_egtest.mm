// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Matcher for done button in tab grid.
id<GREYMatcher> TabGridDoneButton() {
  return grey_allOf(grey_accessibilityID(kTabGridDoneButtonAccessibilityID),
                    grey_sufficientlyVisible(), nil);
}
// Identifer for cell at given |index|.
NSString* IdentifierForCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u", kGridCellIdentifierPrefix, index];
}
}  // namespace

@interface TabGridTestCase : ChromeTestCase
@end

@implementation TabGridTestCase

// Tests entering and leaving the tab grid.
- (void)testEnteringAndLeavingTabGrid {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridDoneButton()]
      performAction:grey_tap()];
}

// Tests that tapping on the first cell shows that tab.
- (void)testTappingOnFirstCell {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          IdentifierForCellAtIndex(0))]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
