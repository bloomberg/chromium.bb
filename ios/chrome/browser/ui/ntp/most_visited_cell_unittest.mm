// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/ntp/most_visited_cell.h"
#include "testing/platform_test.h"

static NSString* title = @"Most Visited Cell Title";
const GURL URL = GURL("http://example.com");

// Fixture to test MostVisitedCell.
class MostVisitedCellTest : public PlatformTest {
 protected:
  base::scoped_nsobject<MostVisitedCell> cell_;
};

TEST_F(MostVisitedCellTest, TestConstructor) {
  CGRect rect = CGRectMake(0, 0, 100, 100);
  cell_.reset([[MostVisitedCell alloc] initWithFrame:rect]);
  [cell_ setURL:URL];
  EXPECT_TRUE(cell_.get());
  UIGraphicsBeginImageContext([cell_ bounds].size);
  [cell_ drawRect:[cell_ bounds]];
  UIGraphicsEndImageContext();
  EXPECT_EQ(URL, [cell_ URL]);
}

TEST_F(MostVisitedCellTest, ValidateTitle) {
  CGRect rect = CGRectMake(0, 0, 100, 100);
  cell_.reset([[MostVisitedCell alloc] initWithFrame:rect]);
  [cell_ setText:title];
  EXPECT_EQ(title, [cell_ accessibilityLabel]);
}
