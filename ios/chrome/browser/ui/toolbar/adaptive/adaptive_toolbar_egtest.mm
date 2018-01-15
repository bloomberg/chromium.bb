// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "components/strings/grit/components_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns the trait collection for this application main window.
UITraitCollection* TraitCollection() {
  return [UIApplication sharedApplication].keyWindow.traitCollection;
}

// Whether the application has a compact width size class.
BOOL IsCompactWidth() {
  return TraitCollection().horizontalSizeClass ==
         UIUserInterfaceSizeClassCompact;
}
}

#pragma mark - TestCase

// Test case for the adaptive toolbar UI.
@interface AdaptiveToolbarTestCase : ChromeTestCase

@end

@implementation AdaptiveToolbarTestCase

// Verifies the existence and state of toolbar UI elements.
- (void)testToolbarUI {
  id<GREYMatcher> bookmarkButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(IDS_TOOLTIP_STAR);

  // Navigate to a page and verify the back button is enabled.
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_interactable()];

  // Check that the bookmark button is visible on compact width.
  if (IsCompactWidth()) {
    [[EarlGrey selectElementWithMatcher:bookmarkButton]
        assertWithMatcher:grey_sufficientlyVisible()];
  }

  // TODO(crbug.com/801145): Test more pieces of UI.
}

@end
