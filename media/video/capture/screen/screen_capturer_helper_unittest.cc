// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capturer_helper.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class ScreenCapturerHelperTest : public testing::Test {
 protected:
  ScreenCapturerHelper capturer_helper_;
};

bool Equals(const SkRegion& region1, const SkRegion& region2) {
  SkRegion::Iterator iter1(region1);
  SkRegion::Iterator iter2(region2);
  while (!iter1.done()) {
    SkIRect rect1 = iter1.rect();
    iter1.next();
    if (iter2.done()) {
      return false;
    }
    SkIRect rect2 = iter2.rect();
    iter2.next();
    if (rect1 != rect2) {
      return false;
    }
  }
  if (!iter2.done()) {
    return false;
  }
  return true;
}

TEST_F(ScreenCapturerHelperTest, ClearInvalidRegion) {
  SkRegion region;
  capturer_helper_.InvalidateRegion(SkRegion(SkIRect::MakeXYWH(1, 2, 3, 4)));
  capturer_helper_.ClearInvalidRegion();
  capturer_helper_.SwapInvalidRegion(&region);
  ASSERT_TRUE(region.isEmpty());
}

TEST_F(ScreenCapturerHelperTest, InvalidateRegion) {
  SkRegion region;
  capturer_helper_.SwapInvalidRegion(&region);
  ASSERT_TRUE(Equals(SkRegion(SkIRect::MakeEmpty()), region));

  capturer_helper_.InvalidateRegion(SkRegion(SkIRect::MakeXYWH(1, 2, 3, 4)));
  region.setEmpty();
  capturer_helper_.SwapInvalidRegion(&region);
  ASSERT_TRUE(Equals(SkRegion(SkIRect::MakeXYWH(1, 2, 3, 4)), region));

  capturer_helper_.InvalidateRegion(SkRegion(SkIRect::MakeXYWH(1, 2, 3, 4)));
  capturer_helper_.InvalidateRegion(SkRegion(SkIRect::MakeXYWH(4, 2, 3, 4)));
  region.setEmpty();
  capturer_helper_.SwapInvalidRegion(&region);
  ASSERT_TRUE(Equals(SkRegion(SkIRect::MakeXYWH(1, 2, 6, 4)), region));
}

TEST_F(ScreenCapturerHelperTest, InvalidateScreen) {
  SkRegion region;
  capturer_helper_.InvalidateScreen(SkISize::Make(12, 34));
  capturer_helper_.SwapInvalidRegion(&region);
  ASSERT_TRUE(Equals(SkRegion(SkIRect::MakeWH(12, 34)), region));
}

TEST_F(ScreenCapturerHelperTest, InvalidateFullScreen) {
  SkRegion region;
  capturer_helper_.set_size_most_recent(SkISize::Make(12, 34));
  capturer_helper_.InvalidateFullScreen();
  capturer_helper_.SwapInvalidRegion(&region);
  ASSERT_TRUE(Equals(SkRegion(SkIRect::MakeWH(12, 34)), region));
}

TEST_F(ScreenCapturerHelperTest, SizeMostRecent) {
  ASSERT_EQ(SkISize::Make(0, 0), capturer_helper_.size_most_recent());
  capturer_helper_.set_size_most_recent(SkISize::Make(12, 34));
  ASSERT_EQ(SkISize::Make(12, 34), capturer_helper_.size_most_recent());
}

TEST_F(ScreenCapturerHelperTest, SetLogGridSize) {
  capturer_helper_.set_size_most_recent(SkISize::Make(10, 10));

  SkRegion region;
  capturer_helper_.SwapInvalidRegion(&region);
  ASSERT_TRUE(Equals(SkRegion(SkIRect::MakeEmpty()), region));

  capturer_helper_.InvalidateRegion(SkRegion(SkIRect::MakeXYWH(7, 7, 1, 1)));
  region.setEmpty();
  capturer_helper_.SwapInvalidRegion(&region);
  ASSERT_TRUE(Equals(SkRegion(SkIRect::MakeXYWH(7, 7, 1, 1)), region));

  capturer_helper_.SetLogGridSize(-1);
  capturer_helper_.InvalidateRegion(SkRegion(SkIRect::MakeXYWH(7, 7, 1, 1)));
  region.setEmpty();
  capturer_helper_.SwapInvalidRegion(&region);
  ASSERT_TRUE(Equals(SkRegion(SkIRect::MakeXYWH(7, 7, 1, 1)), region));

  capturer_helper_.SetLogGridSize(0);
  capturer_helper_.InvalidateRegion(SkRegion(SkIRect::MakeXYWH(7, 7, 1, 1)));
  region.setEmpty();
  capturer_helper_.SwapInvalidRegion(&region);
  ASSERT_TRUE(Equals(SkRegion(SkIRect::MakeXYWH(7, 7, 1, 1)), region));

  capturer_helper_.SetLogGridSize(1);
  capturer_helper_.InvalidateRegion(SkRegion(SkIRect::MakeXYWH(7, 7, 1, 1)));
  region.setEmpty();
  capturer_helper_.SwapInvalidRegion(&region);
  ASSERT_TRUE(Equals(SkRegion(SkIRect::MakeXYWH(6, 6, 2, 2)), region));

  capturer_helper_.SetLogGridSize(2);
  capturer_helper_.InvalidateRegion(SkRegion(SkIRect::MakeXYWH(7, 7, 1, 1)));
  region.setEmpty();
  capturer_helper_.SwapInvalidRegion(&region);
  ASSERT_TRUE(Equals(SkRegion(SkIRect::MakeXYWH(4, 4, 4, 4)), region));

  capturer_helper_.SetLogGridSize(0);
  capturer_helper_.InvalidateRegion(SkRegion(SkIRect::MakeXYWH(7, 7, 1, 1)));
  region.setEmpty();
  capturer_helper_.SwapInvalidRegion(&region);
  ASSERT_TRUE(Equals(SkRegion(SkIRect::MakeXYWH(7, 7, 1, 1)), region));
}

void TestExpandRegionToGrid(const SkRegion& region, int log_grid_size,
                            const SkRegion& expandedRegionExpected) {
  scoped_ptr<SkRegion> expandedRegion1(
      ScreenCapturerHelper::ExpandToGrid(region, log_grid_size));
  ASSERT_TRUE(Equals(expandedRegionExpected, *expandedRegion1));
  scoped_ptr<SkRegion> expandedRegion2(
      ScreenCapturerHelper::ExpandToGrid(*expandedRegion1, log_grid_size));
  ASSERT_TRUE(Equals(*expandedRegion1, *expandedRegion2));
}

void TestExpandRectToGrid(int l, int t, int r, int b, int log_grid_size,
                          int lExpanded, int tExpanded,
                          int rExpanded, int bExpanded) {
  TestExpandRegionToGrid(SkRegion(SkIRect::MakeLTRB(l, t, r, b)), log_grid_size,
                         SkRegion(SkIRect::MakeLTRB(lExpanded, tExpanded,
                                                    rExpanded, bExpanded)));
}

TEST_F(ScreenCapturerHelperTest, ExpandToGrid) {
  const int LOG_GRID_SIZE = 4;
  const int GRID_SIZE = 1 << LOG_GRID_SIZE;
  for (int i = -2; i <= 2; i++) {
    int x = i * GRID_SIZE;
    for (int j = -2; j <= 2; j++) {
      int y = j * GRID_SIZE;
      TestExpandRectToGrid(x + 0, y + 0, x + 1, y + 1, LOG_GRID_SIZE,
                           x + 0, y + 0, x + GRID_SIZE, y + GRID_SIZE);
      TestExpandRectToGrid(x + 0, y + GRID_SIZE - 1, x + 1, y + GRID_SIZE,
                           LOG_GRID_SIZE,
                           x + 0, y + 0, x + GRID_SIZE, y + GRID_SIZE);
      TestExpandRectToGrid(x + GRID_SIZE - 1, y + GRID_SIZE - 1,
                           x + GRID_SIZE, y + GRID_SIZE, LOG_GRID_SIZE,
                           x + 0, y + 0, x + GRID_SIZE, y + GRID_SIZE);
      TestExpandRectToGrid(x + GRID_SIZE - 1, y + 0,
                           x + GRID_SIZE, y + 1, LOG_GRID_SIZE,
                           x + 0, y + 0, x + GRID_SIZE, y + GRID_SIZE);
      TestExpandRectToGrid(x - 1, y + 0, x + 1, y + 1, LOG_GRID_SIZE,
                           x - GRID_SIZE, y + 0, x + GRID_SIZE, y + GRID_SIZE);
      TestExpandRectToGrid(x - 1, y - 1, x + 1, y + 0, LOG_GRID_SIZE,
                           x - GRID_SIZE, y - GRID_SIZE, x + GRID_SIZE, y);
      TestExpandRectToGrid(x + 0, y - 1, x + 1, y + 1, LOG_GRID_SIZE,
                           x, y - GRID_SIZE, x + GRID_SIZE, y + GRID_SIZE);
      TestExpandRectToGrid(x - 1, y - 1, x + 0, y + 1, LOG_GRID_SIZE,
                           x - GRID_SIZE, y - GRID_SIZE, x, y + GRID_SIZE);

      SkRegion region(SkIRect::MakeLTRB(x - 1, y - 1, x + 1, y + 1));
      region.op(SkIRect::MakeLTRB(x - 1, y - 1, x + 0, y + 0),
                SkRegion::kDifference_Op);
      SkRegion expandedRegionExpected(SkIRect::MakeLTRB(
          x - GRID_SIZE, y - GRID_SIZE, x + GRID_SIZE, y + GRID_SIZE));
      expandedRegionExpected.op(
          SkIRect::MakeLTRB(x - GRID_SIZE, y - GRID_SIZE, x + 0, y + 0),
                            SkRegion::kDifference_Op);
      TestExpandRegionToGrid(region, LOG_GRID_SIZE, expandedRegionExpected);

      region.setRect(SkIRect::MakeLTRB(x - 1, y - 1, x + 1, y + 1));
      region.op(SkIRect::MakeLTRB(x - 1, y + 0, x + 0, y + 1),
                SkRegion::kDifference_Op);
      expandedRegionExpected.setRect(SkIRect::MakeLTRB(
          x - GRID_SIZE, y - GRID_SIZE, x + GRID_SIZE, y + GRID_SIZE));
      expandedRegionExpected.op(
          SkIRect::MakeLTRB(x - GRID_SIZE, y + 0, x + 0, y + GRID_SIZE),
                            SkRegion::kDifference_Op);
      TestExpandRegionToGrid(region, LOG_GRID_SIZE, expandedRegionExpected);

      region.setRect(SkIRect::MakeLTRB(x - 1, y - 1, x + 1, y + 1));
      region.op(SkIRect::MakeLTRB(x + 0, y + 0, x + 1, y + 1),
                SkRegion::kDifference_Op);
      expandedRegionExpected.setRect(SkIRect::MakeLTRB(
          x - GRID_SIZE, y - GRID_SIZE, x + GRID_SIZE, y + GRID_SIZE));
      expandedRegionExpected.op(
          SkIRect::MakeLTRB(x + 0, y + 0, x + GRID_SIZE, y + GRID_SIZE),
                            SkRegion::kDifference_Op);
      TestExpandRegionToGrid(region, LOG_GRID_SIZE, expandedRegionExpected);

      region.setRect(SkIRect::MakeLTRB(x - 1, y - 1, x + 1, y + 1));
      region.op(SkIRect::MakeLTRB(x + 0, y - 1, x + 1, y + 0),
                SkRegion::kDifference_Op);
      expandedRegionExpected.setRect(SkIRect::MakeLTRB(
          x - GRID_SIZE, y - GRID_SIZE, x + GRID_SIZE, y + GRID_SIZE));
      expandedRegionExpected.op(
          SkIRect::MakeLTRB(x + 0, y - GRID_SIZE, x + GRID_SIZE, y + 0),
                            SkRegion::kDifference_Op);
      TestExpandRegionToGrid(region, LOG_GRID_SIZE, expandedRegionExpected);
    }
  }
}

}  // namespace media
