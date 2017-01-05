// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/native_app_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Tests that the name, icon and state are honoured after a call to
// |configureCell:|.
TEST(NativeAppItemTest, ConfigureCell) {
  NativeAppItem* item = [[NativeAppItem alloc] initWithType:0];
  NSString* text = @"Test Switch";
  UIImage* image = ios_internal::CollectionViewTestImage();

  item.name = text;
  item.icon = image;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[NativeAppCell class]]);

  NativeAppCell* appCell = static_cast<NativeAppCell*>(cell);
  EXPECT_FALSE(appCell.nameLabel.text);
  EXPECT_TRUE(appCell.iconImageView.image);
  EXPECT_NE(image, appCell.iconImageView.image);
  EXPECT_FALSE(appCell.accessoryView);

  [item configureCell:cell];
  EXPECT_NSEQ(text, appCell.nameLabel.text);
  EXPECT_NSEQ(image, appCell.iconImageView.image);
  EXPECT_EQ((UIView*)appCell.switchControl, appCell.accessoryView);
  EXPECT_FALSE(appCell.switchControl.on);
}

// Tests that the NativeAppItemSwitchOff configures the cell with a switch,
// turned off.
TEST(NativeAppItemTest, State_SwitchOff) {
  NativeAppItem* item = [[NativeAppItem alloc] initWithType:0];
  item.state = NativeAppItemSwitchOff;
  NativeAppCell* appCell = [[[item cellClass] alloc] init];
  [item configureCell:appCell];

  EXPECT_EQ((UIView*)appCell.switchControl, appCell.accessoryView);
  EXPECT_FALSE(appCell.switchControl.on);
}

// Tests that the NativeAppItemSwitchOn configures the cell with a switch,
// turned on.
TEST(NativeAppItemTest, State_SwitchOn) {
  NativeAppItem* item = [[NativeAppItem alloc] initWithType:0];
  item.state = NativeAppItemSwitchOn;
  NativeAppCell* appCell = [[[item cellClass] alloc] init];
  [item configureCell:appCell];

  EXPECT_EQ((UIView*)appCell.switchControl, appCell.accessoryView);
  EXPECT_TRUE(appCell.switchControl.on);
}

// Tests that the NativeAppItemInstall configures the cell with a button.
TEST(NativeAppItemTest, State_Install) {
  NativeAppItem* item = [[NativeAppItem alloc] initWithType:0];
  item.state = NativeAppItemInstall;
  NativeAppCell* appCell = [[[item cellClass] alloc] init];
  [item configureCell:appCell];

  EXPECT_EQ((UIView*)appCell.installButton, appCell.accessoryView);
}

}  // namespace
