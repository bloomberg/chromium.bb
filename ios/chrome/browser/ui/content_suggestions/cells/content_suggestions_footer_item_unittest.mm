// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_footer_item.h"

#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Tests that configureCell: sets the title of the button and the action.
TEST(ContentSuggestionsFooterItemTest, CellIsConfigured) {
  // Setup.
  NSString* title = @"testTitle";
  __block BOOL hasBeenCalled = NO;
  ProceduralBlock block = ^void() {
    hasBeenCalled = YES;
  };
  ContentSuggestionsFooterItem* item =
      [[ContentSuggestionsFooterItem alloc] initWithType:0
                                                   title:title
                                                   block:block];
  ContentSuggestionsFooterCell* cell = [[[item cellClass] alloc] init];
  ASSERT_EQ([ContentSuggestionsFooterCell class], [cell class]);
  ASSERT_EQ(nil, [cell.button titleForState:UIControlStateNormal]);

  // Action.
  [item configureCell:cell];

  // Tests.
  ASSERT_EQ(title, [cell.button titleForState:UIControlStateNormal]);
  ASSERT_FALSE(hasBeenCalled);
  [cell.button sendActionsForControlEvents:UIControlEventTouchUpInside];
  ASSERT_TRUE(hasBeenCalled);
}

// Tests that when the cell is reused the button's block is removed.
TEST(ContentSuggestionsFooterItemTest, ReuseCell) {
  __block BOOL hasBeenCalled = NO;
  ProceduralBlock block = ^void() {
    hasBeenCalled = YES;
  };
  ContentSuggestionsFooterItem* item =
      [[ContentSuggestionsFooterItem alloc] initWithType:0
                                                   title:@""
                                                   block:block];
  ContentSuggestionsFooterCell* cell = [[[item cellClass] alloc] init];
  [item configureCell:cell];

  // Action.
  [cell prepareForReuse];

  // Test.
  ASSERT_FALSE(hasBeenCalled);
  [cell.button sendActionsForControlEvents:UIControlEventTouchUpInside];
  ASSERT_FALSE(hasBeenCalled);
}
}
