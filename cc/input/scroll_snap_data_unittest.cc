// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_snap_data.h"
#include "cc/input/snap_selection_strategy.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class ScrollSnapDataTest : public testing::Test {};

TEST_F(ScrollSnapDataTest, StartAlignmentCalculation) {
  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::RectF(10, 10, 200, 300), gfx::ScrollOffset(600, 800));
  SnapAreaData area(ScrollSnapAlign(SnapAlignment::kStart),
                    gfx::RectF(100, 150, 100, 100), false);
  container.AddSnapAreaData(area);

  gfx::ScrollOffset snap_position;
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndPosition(gfx::ScrollOffset(0, 0), true,
                                                  true);
  EXPECT_TRUE(container.FindSnapPosition(*strategy, &snap_position));
  EXPECT_EQ(90, snap_position.x());
  EXPECT_EQ(140, snap_position.y());
}

TEST_F(ScrollSnapDataTest, CenterAlignmentCalculation) {
  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::RectF(10, 10, 200, 300), gfx::ScrollOffset(600, 800));
  SnapAreaData area(ScrollSnapAlign(SnapAlignment::kCenter),
                    gfx::RectF(100, 150, 100, 100), false);
  container.AddSnapAreaData(area);

  gfx::ScrollOffset snap_position;
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndPosition(gfx::ScrollOffset(0, 0), true,
                                                  true);
  EXPECT_TRUE(container.FindSnapPosition(*strategy, &snap_position));
  EXPECT_EQ(40, snap_position.x());
  EXPECT_EQ(40, snap_position.y());
}

TEST_F(ScrollSnapDataTest, EndAlignmentCalculation) {
  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::RectF(10, 10, 200, 200), gfx::ScrollOffset(600, 800));
  SnapAreaData area(ScrollSnapAlign(SnapAlignment::kEnd),
                    gfx::RectF(150, 200, 100, 100), false);
  container.AddSnapAreaData(area);

  gfx::ScrollOffset snap_position;
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndPosition(gfx::ScrollOffset(0, 0), true,
                                                  true);
  EXPECT_TRUE(container.FindSnapPosition(*strategy, &snap_position));
  EXPECT_EQ(40, snap_position.x());
  EXPECT_EQ(90, snap_position.y());
}

TEST_F(ScrollSnapDataTest, UnreachableSnapPositionCalculation) {
  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 200, 200), gfx::ScrollOffset(100, 100));
  SnapAreaData area(ScrollSnapAlign(SnapAlignment::kEnd, SnapAlignment::kStart),
                    gfx::RectF(200, 0, 100, 100), false);
  container.AddSnapAreaData(area);

  gfx::ScrollOffset snap_position;
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndPosition(gfx::ScrollOffset(50, 50),
                                                  true, true);
  EXPECT_TRUE(container.FindSnapPosition(*strategy, &snap_position));
  // Aligning to start on x would lead the scroll offset larger than max, and
  // aligning to end on y would lead the scroll offset smaller than zero. So
  // we expect these are clamped.
  EXPECT_EQ(100, snap_position.x());
  EXPECT_EQ(0, snap_position.y());
}

TEST_F(ScrollSnapDataTest, FindsClosestSnapPositionIndependently) {
  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 200, 200), gfx::ScrollOffset(600, 800));
  SnapAreaData snap_x_only(
      ScrollSnapAlign(SnapAlignment::kNone, SnapAlignment::kStart),
      gfx::RectF(80, 0, 150, 150), false);
  SnapAreaData snap_y_only(
      ScrollSnapAlign(SnapAlignment::kStart, SnapAlignment::kNone),
      gfx::RectF(0, 70, 150, 150), false);
  SnapAreaData snap_on_both(ScrollSnapAlign(SnapAlignment::kStart),
                            gfx::RectF(50, 150, 150, 150), false);
  container.AddSnapAreaData(snap_x_only);
  container.AddSnapAreaData(snap_y_only);
  container.AddSnapAreaData(snap_on_both);

  gfx::ScrollOffset snap_position;
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndPosition(gfx::ScrollOffset(100, 100),
                                                  true, true);
  EXPECT_TRUE(container.FindSnapPosition(*strategy, &snap_position));
  EXPECT_EQ(80, snap_position.x());
  EXPECT_EQ(70, snap_position.y());
}

TEST_F(ScrollSnapDataTest, FindsClosestSnapPositionOnAxisValueBoth) {
  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 200, 200), gfx::ScrollOffset(600, 800));
  SnapAreaData snap_x_only(
      ScrollSnapAlign(SnapAlignment::kNone, SnapAlignment::kStart),
      gfx::RectF(80, 0, 150, 150), false);
  SnapAreaData snap_y_only(
      ScrollSnapAlign(SnapAlignment::kStart, SnapAlignment::kNone),
      gfx::RectF(0, 70, 150, 150), false);
  SnapAreaData snap_on_both(ScrollSnapAlign(SnapAlignment::kStart),
                            gfx::RectF(50, 150, 150, 150), false);

  container.AddSnapAreaData(snap_x_only);
  container.AddSnapAreaData(snap_y_only);
  container.AddSnapAreaData(snap_on_both);
  gfx::ScrollOffset snap_position;
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndPosition(gfx::ScrollOffset(40, 120),
                                                  true, true);
  EXPECT_TRUE(container.FindSnapPosition(*strategy, &snap_position));
  EXPECT_EQ(50, snap_position.x());
  EXPECT_EQ(150, snap_position.y());
}

TEST_F(ScrollSnapDataTest, DoesNotSnapOnNonScrolledAxis) {
  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 200, 200), gfx::ScrollOffset(600, 800));
  SnapAreaData snap_x_only(
      ScrollSnapAlign(SnapAlignment::kNone, SnapAlignment::kStart),
      gfx::RectF(80, 0, 150, 150), false);
  SnapAreaData snap_y_only(
      ScrollSnapAlign(SnapAlignment::kStart, SnapAlignment::kNone),
      gfx::RectF(0, 70, 150, 150), false);
  container.AddSnapAreaData(snap_x_only);
  container.AddSnapAreaData(snap_y_only);

  gfx::ScrollOffset snap_position;
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndPosition(gfx::ScrollOffset(100, 100),
                                                  true, false);
  EXPECT_TRUE(container.FindSnapPosition(*strategy, &snap_position));
  EXPECT_EQ(80, snap_position.x());
  EXPECT_EQ(100, snap_position.y());
}

TEST_F(ScrollSnapDataTest, DoesNotSnapOnNonVisibleAreas) {
  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 200, 200), gfx::ScrollOffset(600, 800));
  SnapAreaData snap_x_only(
      ScrollSnapAlign(SnapAlignment::kNone, SnapAlignment::kStart),
      gfx::RectF(300, 400, 100, 100), false);
  SnapAreaData snap_y_only(
      ScrollSnapAlign(SnapAlignment::kStart, SnapAlignment::kNone),
      gfx::RectF(400, 300, 100, 100), false);

  container.AddSnapAreaData(snap_x_only);
  container.AddSnapAreaData(snap_y_only);
  gfx::ScrollOffset snap_position;
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndPosition(gfx::ScrollOffset(0, 0), true,
                                                  true);
  EXPECT_FALSE(container.FindSnapPosition(*strategy, &snap_position));
}

TEST_F(ScrollSnapDataTest, SnapOnClosestAxisFirstIfVisibilityConflicts) {
  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 200, 200), gfx::ScrollOffset(600, 800));

  // Both the areas are currently visible.
  // However, if we snap to them on x and y independently, none is visible after
  // snapping. So we only snap on the axis that has a closer snap point first.
  // After that, we look for another snap point on y axis which does not
  // conflict with the snap point on x.
  SnapAreaData snap_x(
      ScrollSnapAlign(SnapAlignment::kNone, SnapAlignment::kStart),
      gfx::RectF(150, 0, 100, 100), false);
  SnapAreaData snap_y1(
      ScrollSnapAlign(SnapAlignment::kStart, SnapAlignment::kNone),
      gfx::RectF(0, 180, 100, 100), false);
  SnapAreaData snap_y2(
      ScrollSnapAlign(SnapAlignment::kStart, SnapAlignment::kNone),
      gfx::RectF(250, 80, 100, 100), false);
  container.AddSnapAreaData(snap_x);
  container.AddSnapAreaData(snap_y1);
  container.AddSnapAreaData(snap_y2);

  gfx::ScrollOffset snap_position;
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndPosition(gfx::ScrollOffset(0, 0), true,
                                                  true);
  EXPECT_TRUE(container.FindSnapPosition(*strategy, &snap_position));
  EXPECT_EQ(150, snap_position.x());
  EXPECT_EQ(80, snap_position.y());
}

TEST_F(ScrollSnapDataTest, DoesNotSnapToPositionsOutsideProximityRange) {
  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 200, 200), gfx::ScrollOffset(600, 800));
  container.set_proximity_range(gfx::ScrollOffset(50, 50));

  SnapAreaData area(ScrollSnapAlign(SnapAlignment::kStart),
                    gfx::RectF(80, 160, 100, 100), false);
  container.AddSnapAreaData(area);

  gfx::ScrollOffset snap_position;
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndPosition(gfx::ScrollOffset(100, 100),
                                                  true, true);
  EXPECT_TRUE(container.FindSnapPosition(*strategy, &snap_position));

  // The snap position on x, 80, is within the proximity range of [50, 150].
  // However, the snap position on y, 160, is outside the proximity range of
  // [50, 150], so we should only snap on x.
  EXPECT_EQ(80, snap_position.x());
  EXPECT_EQ(100, snap_position.y());
}

TEST_F(ScrollSnapDataTest, MandatoryReturnsToCurrentIfNoValidAreaForward) {
  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 200, 200), gfx::ScrollOffset(2000, 2000));
  SnapAreaData area(ScrollSnapAlign(SnapAlignment::kStart),
                    gfx::RectF(600, 0, 100, 100), false);
  container.AddSnapAreaData(area);
  gfx::ScrollOffset snap_position;

  std::unique_ptr<SnapSelectionStrategy> direction_strategy =
      SnapSelectionStrategy::CreateForDirection(gfx::ScrollOffset(600, 0),
                                                gfx::ScrollOffset(5, 0));
  EXPECT_TRUE(container.FindSnapPosition(*direction_strategy, &snap_position));
  // The snap direction is right. However, there is no valid snap position on
  // that direction. So we have to stay at the current snap position of 600 as
  // the snap type is mandatory.
  EXPECT_EQ(600, snap_position.x());
  EXPECT_EQ(0, snap_position.y());

  std::unique_ptr<SnapSelectionStrategy> end_direction_strategy =
      SnapSelectionStrategy::CreateForEndAndDirection(
          gfx::ScrollOffset(600, 0), gfx::ScrollOffset(15, 15));
  EXPECT_TRUE(
      container.FindSnapPosition(*end_direction_strategy, &snap_position));
  // The snap direction is down and right. However, there is no valid snap
  // position on that direction. So we have to stay at the current snap position
  // of (600, 0) as the snap type is mandatory.
  EXPECT_EQ(600, snap_position.x());
  EXPECT_EQ(0, snap_position.y());

  // If the scroll-snap-type is proximity, we wouldn't consider the current
  // snap area valid even if there is no snap area forward.
  container.set_scroll_snap_type(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kProximity));
  EXPECT_FALSE(container.FindSnapPosition(*direction_strategy, &snap_position));
  EXPECT_FALSE(
      container.FindSnapPosition(*end_direction_strategy, &snap_position));
}

TEST_F(ScrollSnapDataTest, MandatorySnapsBackwardIfNoValidAreaForward) {
  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 200, 200), gfx::ScrollOffset(2000, 2000));
  SnapAreaData area(ScrollSnapAlign(SnapAlignment::kStart),
                    gfx::RectF(600, 0, 100, 100), false);
  container.AddSnapAreaData(area);
  gfx::ScrollOffset snap_position;

  std::unique_ptr<SnapSelectionStrategy> direction_strategy =
      SnapSelectionStrategy::CreateForDirection(gfx::ScrollOffset(650, 0),
                                                gfx::ScrollOffset(5, 0));
  EXPECT_TRUE(container.FindSnapPosition(*direction_strategy, &snap_position));
  // The snap direction is right. However, there is no valid snap position on
  // that direction. So we have to scroll back to the snap position of 600 as
  // the snap type is mandatory.
  EXPECT_EQ(600, snap_position.x());
  EXPECT_EQ(0, snap_position.y());

  std::unique_ptr<SnapSelectionStrategy> end_direction_strategy =
      SnapSelectionStrategy::CreateForEndAndDirection(
          gfx::ScrollOffset(650, 10), gfx::ScrollOffset(15, 15));
  EXPECT_TRUE(
      container.FindSnapPosition(*end_direction_strategy, &snap_position));
  // The snap direction is down and right. However, there is no valid snap
  // position on that direction. So we have to scroll back to the snap position
  // of (600, 0) as the snap type is mandatory.
  EXPECT_EQ(600, snap_position.x());
  EXPECT_EQ(0, snap_position.y());

  // If the scroll-snap-type is proximity, we wouldn't consider the backward
  // snap area valid even if there is no snap area forward.
  container.set_scroll_snap_type(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kProximity));
  EXPECT_FALSE(container.FindSnapPosition(*direction_strategy, &snap_position));
  EXPECT_FALSE(
      container.FindSnapPosition(*end_direction_strategy, &snap_position));
}

}  // namespace cc
