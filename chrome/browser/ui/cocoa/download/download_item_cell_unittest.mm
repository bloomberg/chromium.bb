// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/download/download_item_cell.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

TEST(DownloadItemCellTest, ToggleStatusText) {
  scoped_nsobject<DownloadItemCell> cell;
  cell.reset([[DownloadItemCell alloc] initTextCell:@""]);
  EXPECT_FALSE([cell isStatusTextVisible]);
  EXPECT_FLOAT_EQ(0.0, [cell statusTextAlpha]);

  [cell showSecondaryTitle];
  [cell skipVisibilityAnimation];
  EXPECT_TRUE([cell isStatusTextVisible]);
  EXPECT_FLOAT_EQ(1.0, [cell statusTextAlpha]);

  [cell hideSecondaryTitle];
  [cell skipVisibilityAnimation];
  EXPECT_FALSE([cell isStatusTextVisible]);
  EXPECT_FLOAT_EQ(0.0, [cell statusTextAlpha]);
}
