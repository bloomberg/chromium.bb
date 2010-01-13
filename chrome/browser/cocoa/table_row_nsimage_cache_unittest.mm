// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/table_row_nsimage_cache.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Return;

class MockTable : public TableRowNSImageCache::Table {
 public:
  MOCK_CONST_METHOD0(RowCount, int());
  MOCK_CONST_METHOD1(GetIcon, SkBitmap(int));
};

TEST(TableRowNSImageCacheTest, Basics) {
  SkBitmap first_bitmap;
  first_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 40, 30);
  first_bitmap.eraseRGB(255, 0, 0);
  ASSERT_TRUE(first_bitmap.allocPixels());

  SkBitmap second_bitmap;
  second_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 20, 10);
  second_bitmap.eraseRGB(0, 255, 0);
  ASSERT_TRUE(second_bitmap.allocPixels());

  MockTable table;

  EXPECT_CALL(table, RowCount()).WillRepeatedly(Return(2));
  TableRowNSImageCache cache(&table);

  // Check both images are only generated once
  EXPECT_CALL(table, GetIcon(0)).WillOnce(Return(first_bitmap));
  NSImage* first_image = cache.GetImageForRow(0);
  EXPECT_EQ(40, [first_image size].width);
  EXPECT_EQ(30, [first_image size].height);
  EXPECT_EQ(first_image, cache.GetImageForRow(0));

  EXPECT_CALL(table, GetIcon(1)).WillOnce(Return(second_bitmap));
  NSImage* second_image = cache.GetImageForRow(1);
  EXPECT_EQ(20, [second_image size].width);
  EXPECT_EQ(10, [second_image size].height);
  EXPECT_EQ(second_image, cache.GetImageForRow(1));

  // Check that invalidating the second icon only invalidates the second icon
  cache.OnItemsChanged(/* start =*/1, /* length =*/1);
  EXPECT_EQ(first_image, cache.GetImageForRow(0));
  
  EXPECT_CALL(table, GetIcon(1)).WillOnce(Return(first_bitmap));
  NSImage* new_second_image = cache.GetImageForRow(1);
  EXPECT_EQ(40, [new_second_image size].width);
  EXPECT_EQ(30, [new_second_image size].height);
  EXPECT_EQ(new_second_image, cache.GetImageForRow(1));
  EXPECT_NE(new_second_image, second_image);
}

}  // namespace
