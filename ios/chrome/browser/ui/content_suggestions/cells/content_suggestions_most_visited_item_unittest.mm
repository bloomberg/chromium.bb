// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"

#include "ios/chrome/browser/ui/ui_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

TEST(ContentSuggestionsMostVisitedItemTest, Configure) {
  // Setup.
  ContentSuggestionsMostVisitedItem* item =
      [[ContentSuggestionsMostVisitedItem alloc] initWithType:0];

  // Action.
  ContentSuggestionsMostVisitedCell* cell = [[[item cellClass] alloc] init];

  // Test.
  ASSERT_EQ([ContentSuggestionsMostVisitedCell class], [cell class]);
}

TEST(ContentSuggestionsMostVisitedItemTest, SizeIPhone6) {
  // Setup.
  if (IsIPadIdiom())
    return;

  ContentSuggestionsMostVisitedCell* cell =
      [[ContentSuggestionsMostVisitedCell alloc] init];
  cell.frame = CGRectMake(0, 0, 360, 0);
  [cell layoutIfNeeded];

  // Test.
  EXPECT_EQ(4U, [cell numberOfTilesPerLine]);
}

TEST(ContentSuggestionsMostVisitedItemTest, SizeIPhone5) {
  // Setup.
  if (IsIPadIdiom())
    return;

  ContentSuggestionsMostVisitedCell* cell =
      [[ContentSuggestionsMostVisitedCell alloc] init];
  cell.frame = CGRectMake(0, 0, 320, 0);
  [cell layoutIfNeeded];

  // Test.
  EXPECT_EQ(3U, [cell numberOfTilesPerLine]);
}

// Test for iPad portrait and iPhone landscape.
TEST(ContentSuggestionsMostVisitedItemTest, SizeLarge) {
  // Setup.
  ContentSuggestionsMostVisitedCell* cell =
      [[ContentSuggestionsMostVisitedCell alloc] init];
  cell.frame = CGRectMake(0, 0, 720, 0);
  [cell layoutIfNeeded];

  // Test.
  EXPECT_EQ(4U, [cell numberOfTilesPerLine]);
}

TEST(ContentSuggestionsMostVisitedItemTest, SizeIPadSplit) {
  // Setup.
  if (!IsIPadIdiom())
    return;

  ContentSuggestionsMostVisitedCell* cell =
      [[ContentSuggestionsMostVisitedCell alloc] init];
  cell.frame = CGRectMake(0, 0, 360, 0);
  [cell layoutIfNeeded];

  // Test.
  EXPECT_EQ(3U, [cell numberOfTilesPerLine]);
}
}
