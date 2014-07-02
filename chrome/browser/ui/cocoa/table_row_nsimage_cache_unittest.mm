// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/table_row_nsimage_cache.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace {

class TestTable : public TableRowNSImageCache::Table {
 public:

  std::vector<SkBitmap>* rows() {
    return &rows_;
  }

  // TableRowNSImageCache::Table overrides.
  virtual int RowCount() const OVERRIDE {
    return rows_.size();
  }
  virtual gfx::ImageSkia GetIcon(int index) const OVERRIDE {
    return gfx::ImageSkia::CreateFrom1xBitmap(rows_[index]);
  }

 private:
  std::vector<SkBitmap> rows_;
};

SkBitmap MakeImage(int width, int height) {
  SkBitmap image;
  EXPECT_TRUE(image.allocN32Pixels(width, height));
  image.eraseARGB(255, 255, 0, 0);
  return image;
}

// Define this as a macro so that the original variable names can be used in
// test output.
#define COMPARE_SK_NS_IMG_SIZES(skia, cocoa) \
  EXPECT_EQ(skia.width(), [cocoa size].width); \
  EXPECT_EQ(skia.height(), [cocoa size].height);

TEST(TableRowNSImageCacheTest, ModelChanged) {
  TestTable table;
  std::vector<SkBitmap>* rows = table.rows();
  rows->push_back(MakeImage(10, 10));
  rows->push_back(MakeImage(20, 20));
  rows->push_back(MakeImage(30, 30));
  TableRowNSImageCache cache(&table);

  NSImage* image0 = cache.GetImageForRow(0);
  NSImage* image1 = cache.GetImageForRow(1);
  NSImage* image2 = cache.GetImageForRow(2);

  COMPARE_SK_NS_IMG_SIZES(rows->at(0), image0);
  COMPARE_SK_NS_IMG_SIZES(rows->at(1), image1);
  COMPARE_SK_NS_IMG_SIZES(rows->at(2), image2);

  rows->clear();

  rows->push_back(MakeImage(15, 15));
  rows->push_back(MakeImage(25, 25));
  rows->push_back(MakeImage(35, 35));
  rows->push_back(MakeImage(45, 45));

  // Invalidate the entire model.
  cache.OnModelChanged();

  EXPECT_NE(image0, cache.GetImageForRow(0));
  image0 = cache.GetImageForRow(0);

  EXPECT_NE(image1, cache.GetImageForRow(1));
  image1 = cache.GetImageForRow(1);

  EXPECT_NE(image2, cache.GetImageForRow(2));
  image2 = cache.GetImageForRow(2);

  NSImage* image3 = cache.GetImageForRow(3);

  COMPARE_SK_NS_IMG_SIZES(rows->at(0), image0);
  COMPARE_SK_NS_IMG_SIZES(rows->at(1), image1);
  COMPARE_SK_NS_IMG_SIZES(rows->at(2), image2);
  COMPARE_SK_NS_IMG_SIZES(rows->at(3), image3);
}


TEST(TableRowNSImageCacheTest, ItemsChanged) {
  TestTable table;
  std::vector<SkBitmap>* rows = table.rows();
  rows->push_back(MakeImage(10, 10));
  rows->push_back(MakeImage(20, 20));
  rows->push_back(MakeImage(30, 30));
  TableRowNSImageCache cache(&table);

  NSImage* image0 = cache.GetImageForRow(0);
  NSImage* image1 = cache.GetImageForRow(1);
  NSImage* image2 = cache.GetImageForRow(2);

  COMPARE_SK_NS_IMG_SIZES(rows->at(0), image0);
  COMPARE_SK_NS_IMG_SIZES(rows->at(1), image1);
  COMPARE_SK_NS_IMG_SIZES(rows->at(2), image2);

  // Update the middle image.
  (*rows)[1] = MakeImage(25, 25);
  cache.OnItemsChanged(/* start=*/1, /* count=*/1);

  // Make sure the other items remained the same.
  EXPECT_EQ(image0, cache.GetImageForRow(0));
  EXPECT_EQ(image2, cache.GetImageForRow(2));

  image1 = cache.GetImageForRow(1);
  COMPARE_SK_NS_IMG_SIZES(rows->at(1), image1);

  // Update more than one image.
  (*rows)[0] = MakeImage(15, 15);
  (*rows)[1] = MakeImage(27, 27);
  EXPECT_EQ(3U, rows->size());
  cache.OnItemsChanged(0, 2);

  image0 = cache.GetImageForRow(0);
  image1 = cache.GetImageForRow(1);

  COMPARE_SK_NS_IMG_SIZES(rows->at(0), image0);
  COMPARE_SK_NS_IMG_SIZES(rows->at(1), image1);
}


TEST(TableRowNSImageCacheTest, ItemsAdded) {
  TestTable table;
  std::vector<SkBitmap>* rows = table.rows();
  rows->push_back(MakeImage(10, 10));
  rows->push_back(MakeImage(20, 20));
  TableRowNSImageCache cache(&table);

  NSImage* image0 = cache.GetImageForRow(0);
  NSImage* image1 = cache.GetImageForRow(1);

  COMPARE_SK_NS_IMG_SIZES(rows->at(0), image0);
  COMPARE_SK_NS_IMG_SIZES(rows->at(1), image1);

  // Add an item to the end.
  rows->push_back(MakeImage(30, 30));
  cache.OnItemsAdded(2, 1);

  // Make sure image 1 stayed the same.
  EXPECT_EQ(image1, cache.GetImageForRow(1));
  COMPARE_SK_NS_IMG_SIZES(rows->at(1), image1);

  // Check that image 2 got added correctly.
  NSImage* image2 = cache.GetImageForRow(2);
  COMPARE_SK_NS_IMG_SIZES(rows->at(2), image2);

  // Add two items to the begging.
  rows->insert(rows->begin(), MakeImage(7, 7));
  rows->insert(rows->begin(), MakeImage(3, 3));
  cache.OnItemsAdded(0, 2);

  NSImage* image00 = cache.GetImageForRow(0);
  NSImage* image01 = cache.GetImageForRow(1);

  COMPARE_SK_NS_IMG_SIZES(rows->at(0), image00);
  COMPARE_SK_NS_IMG_SIZES(rows->at(1), image01);
}


TEST(TableRowNSImageCacheTest, ItemsRemoved) {
  TestTable table;
  std::vector<SkBitmap>* rows = table.rows();
  rows->push_back(MakeImage(10, 10));
  rows->push_back(MakeImage(20, 20));
  rows->push_back(MakeImage(30, 30));
  rows->push_back(MakeImage(40, 40));
  rows->push_back(MakeImage(50, 50));
  TableRowNSImageCache cache(&table);

  NSImage* image0 = cache.GetImageForRow(0);
  NSImage* image1 = cache.GetImageForRow(1);
  NSImage* image2 = cache.GetImageForRow(2);
  NSImage* image3 = cache.GetImageForRow(3);
  NSImage* image4 = cache.GetImageForRow(4);

  COMPARE_SK_NS_IMG_SIZES(rows->at(0), image0);
  COMPARE_SK_NS_IMG_SIZES(rows->at(1), image1);
  COMPARE_SK_NS_IMG_SIZES(rows->at(2), image2);
  COMPARE_SK_NS_IMG_SIZES(rows->at(3), image3);
  COMPARE_SK_NS_IMG_SIZES(rows->at(4), image4);

  rows->erase(rows->begin() + 1, rows->begin() + 4);  // [20x20, 50x50)
  cache.OnItemsRemoved(1, 3);

  image0 = cache.GetImageForRow(0);
  image1 = cache.GetImageForRow(1);

  COMPARE_SK_NS_IMG_SIZES(rows->at(0), image0);
  COMPARE_SK_NS_IMG_SIZES(rows->at(1), image1);
}

}  // namespace
