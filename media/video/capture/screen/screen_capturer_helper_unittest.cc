// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capturer_helper.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

using webrtc::DesktopRect;
using webrtc::DesktopRegion;
using webrtc::DesktopSize;

namespace media {

class ScreenCapturerHelperTest : public testing::Test {
 protected:
  ScreenCapturerHelper capturer_helper_;
};

bool Equals(const DesktopRegion& region1, const DesktopRegion& region2) {
  DesktopRegion::Iterator iter1(region1);
  DesktopRegion::Iterator iter2(region2);
  while (!iter1.IsAtEnd() && !iter1.IsAtEnd()) {
    if (!iter1.rect().equals(iter2.rect())) {
      return false;
    }
    iter1.Advance();
    iter2.Advance();
  }
  return iter1.IsAtEnd() && iter2.IsAtEnd();
}

bool Equals(const SkRegion& region1, const SkRegion& region2) {
  SkRegion::Iterator iter1(region1);
  SkRegion::Iterator iter2(region2);
  while (!iter1.done() && !iter2.done()) {
    if (iter1.rect() != iter2.rect()) {
      return false;
    }
    iter1.next();
    iter2.next();
  }
  return iter1.done() && iter2.done();
}

DesktopRegion RectToRegion(const DesktopRect& rect) {
  webrtc::DesktopRegion result;
  result.SetRect(rect);
  return result;
}

TEST_F(ScreenCapturerHelperTest, ClearInvalidRegion) {
  DesktopRegion region;
  region.SetRect(DesktopRect::MakeXYWH(1, 2, 3, 4));
  capturer_helper_.InvalidateRegion(region);
  capturer_helper_.ClearInvalidRegion();
  capturer_helper_.TakeInvalidRegion(&region);
  ASSERT_TRUE(region.is_empty());
}

TEST_F(ScreenCapturerHelperTest, InvalidateRegion) {
  DesktopRegion region;
  capturer_helper_.TakeInvalidRegion(&region);
  ASSERT_TRUE(region.is_empty());

  region.SetRect(DesktopRect::MakeXYWH(1, 2, 3, 4));
  capturer_helper_.InvalidateRegion(region);
  capturer_helper_.TakeInvalidRegion(&region);
  ASSERT_TRUE(Equals(RectToRegion(DesktopRect::MakeXYWH(1, 2, 3, 4)), region));

  capturer_helper_.InvalidateRegion(
      RectToRegion(DesktopRect::MakeXYWH(1, 2, 3, 4)));
  capturer_helper_.InvalidateRegion(
      RectToRegion(DesktopRect::MakeXYWH(4, 2, 3, 4)));
  capturer_helper_.TakeInvalidRegion(&region);
  ASSERT_TRUE(Equals(RectToRegion(DesktopRect::MakeXYWH(1, 2, 6, 4)), region));
}

TEST_F(ScreenCapturerHelperTest, InvalidateScreen) {
  DesktopRegion region;
  capturer_helper_.InvalidateScreen(DesktopSize(12, 34));
  capturer_helper_.TakeInvalidRegion(&region);
  ASSERT_TRUE(Equals(RectToRegion(DesktopRect::MakeWH(12, 34)), region));
}

TEST_F(ScreenCapturerHelperTest, SizeMostRecent) {
  ASSERT_TRUE(capturer_helper_.size_most_recent().is_empty());
  capturer_helper_.set_size_most_recent(DesktopSize(12, 34));
  ASSERT_TRUE(
      DesktopSize(12, 34).equals(capturer_helper_.size_most_recent()));
}

TEST_F(ScreenCapturerHelperTest, SetLogGridSize) {
  capturer_helper_.set_size_most_recent(DesktopSize(10, 10));

  DesktopRegion region;
  capturer_helper_.TakeInvalidRegion(&region);
  ASSERT_TRUE(Equals(RectToRegion(DesktopRect()), region));

  capturer_helper_.InvalidateRegion(
      RectToRegion(DesktopRect::MakeXYWH(7, 7, 1, 1)));
  capturer_helper_.TakeInvalidRegion(&region);
  ASSERT_TRUE(Equals(RectToRegion(DesktopRect::MakeXYWH(7, 7, 1, 1)), region));

  capturer_helper_.SetLogGridSize(-1);
  capturer_helper_.InvalidateRegion(
      RectToRegion(DesktopRect::MakeXYWH(7, 7, 1, 1)));
  capturer_helper_.TakeInvalidRegion(&region);
  ASSERT_TRUE(Equals(RectToRegion(DesktopRect::MakeXYWH(7, 7, 1, 1)), region));

  capturer_helper_.SetLogGridSize(0);
  capturer_helper_.InvalidateRegion(
      RectToRegion(DesktopRect::MakeXYWH(7, 7, 1, 1)));
  capturer_helper_.TakeInvalidRegion(&region);
  ASSERT_TRUE(Equals(RectToRegion(DesktopRect::MakeXYWH(7, 7, 1, 1)), region));

  capturer_helper_.SetLogGridSize(1);
  capturer_helper_.InvalidateRegion(
      RectToRegion(DesktopRect::MakeXYWH(7, 7, 1, 1)));
  capturer_helper_.TakeInvalidRegion(&region);
  ASSERT_TRUE(Equals(RectToRegion(DesktopRect::MakeXYWH(6, 6, 2, 2)), region));

  capturer_helper_.SetLogGridSize(2);
  capturer_helper_.InvalidateRegion(
      RectToRegion(DesktopRect::MakeXYWH(7, 7, 1, 1)));
  capturer_helper_.TakeInvalidRegion(&region);
  ASSERT_TRUE(Equals(RectToRegion(DesktopRect::MakeXYWH(4, 4, 4, 4)), region));

  capturer_helper_.SetLogGridSize(0);
  capturer_helper_.InvalidateRegion(
      RectToRegion(DesktopRect::MakeXYWH(7, 7, 1, 1)));
  capturer_helper_.TakeInvalidRegion(&region);
  ASSERT_TRUE(Equals(RectToRegion(DesktopRect::MakeXYWH(7, 7, 1, 1)), region));
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
