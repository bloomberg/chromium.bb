// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/paint_aggregator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(PaintAggregator, InitialState) {
  PaintAggregator greg;
  EXPECT_FALSE(greg.HasPendingUpdate());
}

TEST(PaintAggregator, SingleInvalidation) {
  PaintAggregator greg;

  gfx::Rect rect(2, 4, 10, 16);
  greg.InvalidateRect(rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_TRUE(update.scroll_rect.IsEmpty());
  ASSERT_EQ(1U, update.paint_rects.size());

  EXPECT_EQ(rect, update.paint_rects[0]);
}

TEST(PaintAggregator, DoubleDisjointInvalidation) {
  PaintAggregator greg;

  gfx::Rect r1(2, 4, 2, 40);
  gfx::Rect r2(4, 2, 40, 2);

  greg.InvalidateRect(r1);
  greg.InvalidateRect(r2);

  gfx::Rect expected_bounds = gfx::UnionRects(r1, r2);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_TRUE(update.scroll_rect.IsEmpty());
  EXPECT_EQ(2U, update.paint_rects.size());

  EXPECT_EQ(expected_bounds, update.GetPaintBounds());
}

TEST(PaintAggregator, DisjointInvalidationsCombined) {
  PaintAggregator greg;

  // Make the rectangles such that they don't overlap but cover a very large
  // percentage of the area of covered by their union. This is so we're not
  // very sensitive to the combining heuristic in the paint aggregator.
  gfx::Rect r1(2, 4, 2, 1000);
  gfx::Rect r2(5, 2, 2, 1000);

  greg.InvalidateRect(r1);
  greg.InvalidateRect(r2);

  gfx::Rect expected_bounds = gfx::UnionRects(r1, r2);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_TRUE(update.scroll_rect.IsEmpty());
  ASSERT_EQ(1U, update.paint_rects.size());

  EXPECT_EQ(expected_bounds, update.paint_rects[0]);
}

TEST(PaintAggregator, SingleScroll) {
  PaintAggregator greg;

  gfx::Rect rect(1, 2, 3, 4);
  gfx::Vector2d delta(1, 0);
  greg.ScrollRect(delta, rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_TRUE(update.paint_rects.empty());
  EXPECT_FALSE(update.scroll_rect.IsEmpty());

  EXPECT_EQ(rect, update.scroll_rect);

  EXPECT_EQ(delta.x(), update.scroll_delta.x());
  EXPECT_EQ(delta.y(), update.scroll_delta.y());

  gfx::Rect resulting_damage = update.GetScrollDamage();
  gfx::Rect expected_damage(1, 2, 1, 4);
  EXPECT_EQ(expected_damage, resulting_damage);
}

TEST(PaintAggregator, DoubleOverlappingScroll) {
  PaintAggregator greg;

  gfx::Rect rect(1, 2, 3, 4);
  gfx::Vector2d delta1(1, 0);
  gfx::Vector2d delta2(1, 0);
  greg.ScrollRect(delta1, rect);
  greg.ScrollRect(delta2, rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_TRUE(update.paint_rects.empty());
  EXPECT_FALSE(update.scroll_rect.IsEmpty());

  EXPECT_EQ(rect, update.scroll_rect);

  gfx::Vector2d expected_delta = delta1 + delta2;
  EXPECT_EQ(expected_delta.ToString(), update.scroll_delta.ToString());

  gfx::Rect resulting_damage = update.GetScrollDamage();
  gfx::Rect expected_damage(1, 2, 2, 4);
  EXPECT_EQ(expected_damage, resulting_damage);
}

TEST(PaintAggregator, NegatingScroll) {
  PaintAggregator greg;

  // Scroll twice in opposite directions by equal amounts.  The result
  // should be no scrolling.

  gfx::Rect rect(1, 2, 3, 4);
  gfx::Vector2d delta1(1, 0);
  gfx::Vector2d delta2(-1, 0);
  greg.ScrollRect(delta1, rect);
  greg.ScrollRect(delta2, rect);

  EXPECT_FALSE(greg.HasPendingUpdate());
}

TEST(PaintAggregator, DiagonalScroll) {
  PaintAggregator greg;

  // We don't support optimized diagonal scrolling, so this should result in
  // repainting.

  gfx::Rect rect(1, 2, 3, 4);
  gfx::Vector2d delta(1, 1);
  greg.ScrollRect(delta, rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_TRUE(update.scroll_rect.IsEmpty());
  ASSERT_EQ(1U, update.paint_rects.size());

  EXPECT_EQ(rect, update.paint_rects[0]);
}

TEST(PaintAggregator, ContainedPaintAfterScroll) {
  PaintAggregator greg;

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(gfx::Vector2d(2, 0), scroll_rect);

  gfx::Rect paint_rect(4, 4, 2, 2);
  greg.InvalidateRect(paint_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  // expecting a paint rect inside the scroll rect
  EXPECT_FALSE(update.scroll_rect.IsEmpty());
  EXPECT_EQ(1U, update.paint_rects.size());

  EXPECT_EQ(scroll_rect, update.scroll_rect);
  EXPECT_EQ(paint_rect, update.paint_rects[0]);
}

TEST(PaintAggregator, ContainedPaintBeforeScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect(4, 4, 2, 2);
  greg.InvalidateRect(paint_rect);

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(gfx::Vector2d(2, 0), scroll_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  // Expecting a paint rect inside the scroll rect
  EXPECT_FALSE(update.scroll_rect.IsEmpty());
  EXPECT_EQ(1U, update.paint_rects.size());

  paint_rect.Offset(2, 0);

  EXPECT_EQ(scroll_rect, update.scroll_rect);
  EXPECT_EQ(paint_rect, update.paint_rects[0]);
}

TEST(PaintAggregator, ContainedPaintsBeforeAndAfterScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect1(4, 4, 2, 2);
  greg.InvalidateRect(paint_rect1);

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(gfx::Vector2d(2, 0), scroll_rect);

  gfx::Rect paint_rect2(6, 4, 2, 2);
  greg.InvalidateRect(paint_rect2);

  gfx::Rect expected_paint_rect = paint_rect2;

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  // Expecting a paint rect inside the scroll rect
  EXPECT_FALSE(update.scroll_rect.IsEmpty());
  EXPECT_EQ(1U, update.paint_rects.size());

  EXPECT_EQ(scroll_rect, update.scroll_rect);
  EXPECT_EQ(expected_paint_rect, update.paint_rects[0]);
}

TEST(PaintAggregator, LargeContainedPaintAfterScroll) {
  PaintAggregator greg;

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(gfx::Vector2d(0, 1), scroll_rect);

  gfx::Rect paint_rect(0, 0, 10, 9);  // Repaint 90%
  greg.InvalidateRect(paint_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_TRUE(update.scroll_rect.IsEmpty());
  EXPECT_EQ(1U, update.paint_rects.size());

  EXPECT_EQ(scroll_rect, update.paint_rects[0]);
}

TEST(PaintAggregator, LargeContainedPaintBeforeScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect(0, 0, 10, 9);  // Repaint 90%
  greg.InvalidateRect(paint_rect);

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(gfx::Vector2d(0, 1), scroll_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_TRUE(update.scroll_rect.IsEmpty());
  EXPECT_EQ(1U, update.paint_rects.size());

  EXPECT_EQ(scroll_rect, update.paint_rects[0]);
}

TEST(PaintAggregator, OverlappingPaintBeforeScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect(4, 4, 10, 2);
  greg.InvalidateRect(paint_rect);

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(gfx::Vector2d(2, 0), scroll_rect);

  gfx::Rect expected_paint_rect = gfx::UnionRects(scroll_rect, paint_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_TRUE(update.scroll_rect.IsEmpty());
  EXPECT_EQ(1U, update.paint_rects.size());

  EXPECT_EQ(expected_paint_rect, update.paint_rects[0]);
}

TEST(PaintAggregator, OverlappingPaintAfterScroll) {
  PaintAggregator greg;

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(gfx::Vector2d(2, 0), scroll_rect);

  gfx::Rect paint_rect(4, 4, 10, 2);
  greg.InvalidateRect(paint_rect);

  gfx::Rect expected_paint_rect = gfx::UnionRects(scroll_rect, paint_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_TRUE(update.scroll_rect.IsEmpty());
  EXPECT_EQ(1U, update.paint_rects.size());

  EXPECT_EQ(expected_paint_rect, update.paint_rects[0]);
}

TEST(PaintAggregator, DisjointPaintBeforeScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect(4, 4, 10, 2);
  greg.InvalidateRect(paint_rect);

  gfx::Rect scroll_rect(0, 0, 2, 10);
  greg.ScrollRect(gfx::Vector2d(2, 0), scroll_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_FALSE(update.scroll_rect.IsEmpty());
  EXPECT_EQ(1U, update.paint_rects.size());

  EXPECT_EQ(paint_rect, update.paint_rects[0]);
  EXPECT_EQ(scroll_rect, update.scroll_rect);
}

TEST(PaintAggregator, DisjointPaintAfterScroll) {
  PaintAggregator greg;

  gfx::Rect scroll_rect(0, 0, 2, 10);
  greg.ScrollRect(gfx::Vector2d(2, 0), scroll_rect);

  gfx::Rect paint_rect(4, 4, 10, 2);
  greg.InvalidateRect(paint_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_FALSE(update.scroll_rect.IsEmpty());
  EXPECT_EQ(1U, update.paint_rects.size());

  EXPECT_EQ(paint_rect, update.paint_rects[0]);
  EXPECT_EQ(scroll_rect, update.scroll_rect);
}

TEST(PaintAggregator, ContainedPaintTrimmedByScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect(4, 4, 6, 6);
  greg.InvalidateRect(paint_rect);

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(gfx::Vector2d(2, 0), scroll_rect);

  // The paint rect should have become narrower.
  gfx::Rect expected_paint_rect(6, 4, 4, 6);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_FALSE(update.scroll_rect.IsEmpty());
  EXPECT_EQ(1U, update.paint_rects.size());

  EXPECT_EQ(expected_paint_rect, update.paint_rects[0]);
  EXPECT_EQ(scroll_rect, update.scroll_rect);
}

TEST(PaintAggregator, ContainedPaintEliminatedByScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect(4, 4, 6, 6);
  greg.InvalidateRect(paint_rect);

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(gfx::Vector2d(6, 0), scroll_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_FALSE(update.scroll_rect.IsEmpty());
  EXPECT_TRUE(update.paint_rects.empty());

  EXPECT_EQ(scroll_rect, update.scroll_rect);
}

TEST(PaintAggregator, ContainedPaintAfterScrollTrimmedByScrollDamage) {
  PaintAggregator greg;

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(gfx::Vector2d(4, 0), scroll_rect);

  gfx::Rect paint_rect(2, 0, 4, 10);
  greg.InvalidateRect(paint_rect);

  gfx::Rect expected_scroll_damage(0, 0, 4, 10);
  gfx::Rect expected_paint_rect(4, 0, 2, 10);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_FALSE(update.scroll_rect.IsEmpty());
  EXPECT_EQ(1U, update.paint_rects.size());

  EXPECT_EQ(scroll_rect, update.scroll_rect);
  EXPECT_EQ(expected_scroll_damage, update.GetScrollDamage());
  EXPECT_EQ(expected_paint_rect, update.paint_rects[0]);
}

TEST(PaintAggregator, ContainedPaintAfterScrollEliminatedByScrollDamage) {
  PaintAggregator greg;

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(gfx::Vector2d(4, 0), scroll_rect);

  gfx::Rect paint_rect(2, 0, 2, 10);
  greg.InvalidateRect(paint_rect);

  gfx::Rect expected_scroll_damage(0, 0, 4, 10);

  EXPECT_TRUE(greg.HasPendingUpdate());
  PaintAggregator::PendingUpdate update;
  greg.PopPendingUpdate(&update);

  EXPECT_FALSE(update.scroll_rect.IsEmpty());
  EXPECT_TRUE(update.paint_rects.empty());

  EXPECT_EQ(scroll_rect, update.scroll_rect);
  EXPECT_EQ(expected_scroll_damage, update.GetScrollDamage());
}

}  // namespace content
