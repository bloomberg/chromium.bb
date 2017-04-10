// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited.h"

#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

TEST(ContentSuggestionsMostVisitedTest, Constructor) {
  NSString* title = @"Test title";
  FaviconAttributes* attributes =
      [[FaviconAttributes alloc] initWithImage:[[UIImage alloc] init]];
  ContentSuggestionsMostVisited* mostVisited =
      [ContentSuggestionsMostVisited mostVisitedWithTitle:title
                                               attributes:attributes];

  ASSERT_TRUE([title isEqualToString:mostVisited.title]);
  ASSERT_EQ(attributes, mostVisited.attributes);
}
}
