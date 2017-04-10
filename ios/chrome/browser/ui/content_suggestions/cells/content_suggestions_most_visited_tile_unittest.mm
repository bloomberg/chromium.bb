// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile.h"

#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

TEST(ContentSuggestionsMostVisitedTileTest, Constructor) {
  NSString* title = @"Test title";
  ContentSuggestionsMostVisitedTile* tile =
      [ContentSuggestionsMostVisitedTile tileWithTitle:title attributes:nil];

  ASSERT_TRUE([title isEqualToString:tile.accessibilityLabel]);
}
}
