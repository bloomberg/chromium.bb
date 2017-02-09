// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/most_visited_cell.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

static NSString* title = @"Most Visited Cell Title";
const GURL URL = GURL("http://example.com");

// Fixture to test MostVisitedCell.
class MostVisitedCellTest : public PlatformTest {
 protected:
  MostVisitedCell* cell_;
};

TEST_F(MostVisitedCellTest, TestConstructor) {
  CGRect rect = CGRectMake(0, 0, 100, 100);
  cell_ = [[MostVisitedCell alloc] initWithFrame:rect];
  [cell_ setURL:URL];
  EXPECT_TRUE(cell_);
  UIGraphicsBeginImageContext([cell_ bounds].size);
  [cell_ drawRect:[cell_ bounds]];
  UIGraphicsEndImageContext();
  EXPECT_EQ(URL, [cell_ URL]);
}

TEST_F(MostVisitedCellTest, ValidateTitle) {
  CGRect rect = CGRectMake(0, 0, 100, 100);
  cell_ = [[MostVisitedCell alloc] initWithFrame:rect];
  [cell_ setText:title];
  EXPECT_EQ(title, [cell_ accessibilityLabel]);
}
