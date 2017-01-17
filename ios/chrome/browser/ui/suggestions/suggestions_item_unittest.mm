// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/suggestions/suggestions_item.h"

#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Tests that configureCell: set all the fields of the cell.
TEST(SuggestionsItemTest, CellIsConfigured) {
  NSString* title = @"testTitle";
  NSString* subtitle = @"testSubtitle";
  SuggestionsItem* item =
      [[SuggestionsItem alloc] initWithType:0 title:title subtitle:subtitle];
  SuggestionsCell* cell = [[[item cellClass] alloc] init];
  EXPECT_TRUE([cell isMemberOfClass:[SuggestionsCell class]]);

  [item configureCell:cell];
  EXPECT_EQ(title, [cell.titleButton titleForState:UIControlStateNormal]);
  EXPECT_EQ(subtitle, cell.detailTextLabel.text);
}

}  // namespace
