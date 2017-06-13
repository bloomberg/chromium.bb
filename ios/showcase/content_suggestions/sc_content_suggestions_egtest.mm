// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "components/strings/grit/components_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for the suggestions view controller.
@interface SCContentSuggestionsTestCase : ShowcaseTestCase
@end

@implementation SCContentSuggestionsTestCase

// Tests launching ContentSuggestionsViewController.
- (void)testLaunch {
  showcase_utils::Open(@"ContentSuggestionsViewController");
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_FOOTER_TITLE)]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_HEADER)]
      assertWithMatcher:grey_sufficientlyVisible()];
  showcase_utils::Close();
}

@end
