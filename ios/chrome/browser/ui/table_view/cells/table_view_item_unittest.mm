// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using TableViewItemTest = PlatformTest;

TEST_F(TableViewItemTest, ConfigureCellPortsAccessibilityProperties) {
  TableViewItem* item = [[TableViewItem alloc] initWithType:0];
  item.accessibilityIdentifier = @"test_identifier";
  item.accessibilityTraits = UIAccessibilityTraitButton;
  UITableViewCell* cell = [[[item cellClass] alloc] init];
  EXPECT_TRUE([cell isMemberOfClass:[UITableViewCell class]]);
  EXPECT_EQ(UIAccessibilityTraitNone, [cell accessibilityTraits]);
  EXPECT_FALSE([cell accessibilityIdentifier]);
  [item configureCell:cell];
  EXPECT_EQ(UIAccessibilityTraitButton, [cell accessibilityTraits]);
  EXPECT_NSEQ(@"test_identifier", [cell accessibilityIdentifier]);
}

}  // namespace
