// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item+collection_view_controller.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using CollectionViewItemTest = PlatformTest;

TEST_F(CollectionViewItemTest, Accessors) {
  CollectionViewItem* five = [[CollectionViewItem alloc] initWithType:5];
  CollectionViewItem* twelve = [[CollectionViewItem alloc] initWithType:12];

  EXPECT_EQ(5, [five type]);
  EXPECT_EQ(12, [twelve type]);

  // Test setting the type property used in CollectionViewController.
  [five setType:55];
  EXPECT_EQ(55, [five type]);

  [twelve setType:1212];
  EXPECT_EQ(1212, [twelve type]);
}

TEST_F(CollectionViewItemTest, ConfigureCellPortsAccessibilityProperties) {
  CollectionViewItem* item = [[CollectionViewItem alloc] initWithType:0];
  item.accessibilityIdentifier = @"test_identifier";
  item.accessibilityTraits = UIAccessibilityTraitButton;
  MDCCollectionViewCell* cell = [[[item cellClass] alloc] init];
  EXPECT_TRUE([cell isMemberOfClass:[MDCCollectionViewCell class]]);
  EXPECT_EQ(UIAccessibilityTraitNone, [cell accessibilityTraits]);
  EXPECT_FALSE([cell accessibilityIdentifier]);
  [item configureCell:cell];
  EXPECT_EQ(UIAccessibilityTraitButton, [cell accessibilityTraits]);
  EXPECT_NSEQ(@"test_identifier", [cell accessibilityIdentifier]);
}

}  // namespace
