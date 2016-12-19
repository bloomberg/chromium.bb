// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_item.h"

#import <UIKit/UIKit.h>

#include "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class ToolsMenuViewItemTest : public PlatformTest {
 protected:
  void SetUp() override {
    toolsMenuViewItem_.reset([[ToolsMenuViewItem alloc] init]);
  }
  base::scoped_nsobject<ToolsMenuViewItem> toolsMenuViewItem_;
};

TEST_F(ToolsMenuViewItemTest, CustomizeCellAccessibilityTrait) {
  base::scoped_nsobject<ToolsMenuViewCell> cell1(
      [[ToolsMenuViewCell alloc] init]);
  base::scoped_nsobject<ToolsMenuViewCell> cell2(
      [[ToolsMenuViewCell alloc] init]);

  [toolsMenuViewItem_ setActive:YES];
  [cell1 setAccessibilityTraits:UIAccessibilityTraitNotEnabled];
  [cell1 configureForMenuItem:toolsMenuViewItem_];
  EXPECT_FALSE(cell1.get().accessibilityTraits &
               UIAccessibilityTraitNotEnabled);

  [toolsMenuViewItem_ setActive:NO];
  [cell2 configureForMenuItem:toolsMenuViewItem_];
  EXPECT_TRUE(cell2.get().accessibilityTraits & UIAccessibilityTraitNotEnabled);

  // Happens when cell2 is reused.
  [toolsMenuViewItem_ setActive:YES];
  [cell2 configureForMenuItem:toolsMenuViewItem_];
  EXPECT_FALSE(cell2.get().accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}

}  // namespace
