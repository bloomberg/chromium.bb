// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/paint_aggregator.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PaintAggregator, InitialState) {
  PaintAggregator greg;
  EXPECT_FALSE(greg.HasPendingUpdate());
}

TEST(PaintAggregator, SingleInvalidation) {
  PaintAggregator greg;

  gfx::Rect rect(2, 4, 10, 16);
  greg.InvalidateRect(rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  EXPECT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_EQ(1U, greg.GetPendingUpdate().paint_rects.size());

  EXPECT_EQ(rect, greg.GetPendingUpdate().paint_rects[0]);
}

TEST(PaintAggregator, DoubleDisjointInvalidation) {
  PaintAggregator greg;

  gfx::Rect r1(2, 4, 2, 4);
  gfx::Rect r2(4, 2, 4, 2);

  greg.InvalidateRect(r1);
  greg.InvalidateRect(r2);

  gfx::Rect expected_bounds = r1.Union(r2);

  EXPECT_TRUE(greg.HasPendingUpdate());
  EXPECT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_EQ(2U, greg.GetPendingUpdate().paint_rects.size());

  EXPECT_EQ(expected_bounds, greg.GetPendingUpdate().GetPaintBounds());
}

TEST(PaintAggregator, SingleScroll) {
  PaintAggregator greg;

  gfx::Rect rect(1, 2, 3, 4);
  gfx::Point delta(1, 0);
  greg.ScrollRect(delta.x(), delta.y(), rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  EXPECT_TRUE(greg.GetPendingUpdate().paint_rects.empty());
  EXPECT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());

  EXPECT_EQ(rect, greg.GetPendingUpdate().scroll_rect);

  EXPECT_EQ(delta.x(), greg.GetPendingUpdate().scroll_delta.x());
  EXPECT_EQ(delta.y(), greg.GetPendingUpdate().scroll_delta.y());

  gfx::Rect resulting_damage = greg.GetPendingUpdate().GetScrollDamage();
  gfx::Rect expected_damage(1, 2, 1, 4);
  EXPECT_EQ(expected_damage, resulting_damage);
}

TEST(PaintAggregator, DoubleOverlappingScroll) {
  PaintAggregator greg;

  gfx::Rect rect(1, 2, 3, 4);
  gfx::Point delta1(1, 0);
  gfx::Point delta2(1, 0);
  greg.ScrollRect(delta1.x(), delta1.y(), rect);
  greg.ScrollRect(delta2.x(), delta2.y(), rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  EXPECT_TRUE(greg.GetPendingUpdate().paint_rects.empty());
  EXPECT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());

  EXPECT_EQ(rect, greg.GetPendingUpdate().scroll_rect);

  gfx::Point expected_delta(delta1.x() + delta2.x(),
                            delta1.y() + delta2.y());
  EXPECT_EQ(expected_delta.x(), greg.GetPendingUpdate().scroll_delta.x());
  EXPECT_EQ(expected_delta.y(), greg.GetPendingUpdate().scroll_delta.y());

  gfx::Rect resulting_damage = greg.GetPendingUpdate().GetScrollDamage();
  gfx::Rect expected_damage(1, 2, 2, 4);
  EXPECT_EQ(expected_damage, resulting_damage);
}

TEST(PaintAggregator, DiagonalScroll) {
  PaintAggregator greg;

  // We don't support optimized diagonal scrolling, so this should result in
  // repainting.

  gfx::Rect rect(1, 2, 3, 4);
  gfx::Point delta(1, 1);
  greg.ScrollRect(delta.x(), delta.y(), rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  EXPECT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  ASSERT_EQ(1U, greg.GetPendingUpdate().paint_rects.size());

  EXPECT_EQ(rect, greg.GetPendingUpdate().paint_rects[0]);
}

TEST(PaintAggregator, ContainedPaintAfterScroll) {
  PaintAggregator greg;

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(2, 0, scroll_rect);

  gfx::Rect paint_rect(4, 4, 2, 2);
  greg.InvalidateRect(paint_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());

  // expecting a paint rect inside the scroll rect
  EXPECT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_EQ(1U, greg.GetPendingUpdate().paint_rects.size());

  EXPECT_EQ(scroll_rect, greg.GetPendingUpdate().scroll_rect);
  EXPECT_EQ(paint_rect, greg.GetPendingUpdate().paint_rects[0]);
}

TEST(PaintAggregator, ContainedPaintBeforeScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect(4, 4, 2, 2);
  greg.InvalidateRect(paint_rect);

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(2, 0, scroll_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());

  // Expecting a paint rect inside the scroll rect
  EXPECT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_EQ(1U, greg.GetPendingUpdate().paint_rects.size());

  paint_rect.Offset(2, 0);

  EXPECT_EQ(scroll_rect, greg.GetPendingUpdate().scroll_rect);
  EXPECT_EQ(paint_rect, greg.GetPendingUpdate().paint_rects[0]);
}

TEST(PaintAggregator, ContainedPaintsBeforeAndAfterScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect1(4, 4, 2, 2);
  greg.InvalidateRect(paint_rect1);

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(2, 0, scroll_rect);

  gfx::Rect paint_rect2(6, 4, 2, 2);
  greg.InvalidateRect(paint_rect2);

  gfx::Rect expected_paint_rect = paint_rect2;

  EXPECT_TRUE(greg.HasPendingUpdate());

  // Expecting a paint rect inside the scroll rect
  EXPECT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_EQ(1U, greg.GetPendingUpdate().paint_rects.size());

  EXPECT_EQ(scroll_rect, greg.GetPendingUpdate().scroll_rect);
  EXPECT_EQ(expected_paint_rect, greg.GetPendingUpdate().paint_rects[0]);
}

TEST(PaintAggregator, LargeContainedPaintAfterScroll) {
  PaintAggregator greg;

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(0, 1, scroll_rect);

  gfx::Rect paint_rect(0, 0, 10, 9);  // Repaint 90%
  greg.InvalidateRect(paint_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());

  EXPECT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_EQ(1U, greg.GetPendingUpdate().paint_rects.size());

  EXPECT_EQ(scroll_rect, greg.GetPendingUpdate().paint_rects[0]);
}

TEST(PaintAggregator, LargeContainedPaintBeforeScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect(0, 0, 10, 9);  // Repaint 90%
  greg.InvalidateRect(paint_rect);

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(0, 1, scroll_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());

  EXPECT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_EQ(1U, greg.GetPendingUpdate().paint_rects.size());

  EXPECT_EQ(scroll_rect, greg.GetPendingUpdate().paint_rects[0]);
}

TEST(PaintAggregator, OverlappingPaintBeforeScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect(4, 4, 10, 2);
  greg.InvalidateRect(paint_rect);

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(2, 0, scroll_rect);

  gfx::Rect expected_paint_rect = scroll_rect.Union(paint_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());

  EXPECT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_EQ(1U, greg.GetPendingUpdate().paint_rects.size());

  EXPECT_EQ(expected_paint_rect, greg.GetPendingUpdate().paint_rects[0]);
}

TEST(PaintAggregator, OverlappingPaintAfterScroll) {
  PaintAggregator greg;

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(2, 0, scroll_rect);

  gfx::Rect paint_rect(4, 4, 10, 2);
  greg.InvalidateRect(paint_rect);

  gfx::Rect expected_paint_rect = scroll_rect.Union(paint_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());

  EXPECT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_EQ(1U, greg.GetPendingUpdate().paint_rects.size());

  EXPECT_EQ(expected_paint_rect, greg.GetPendingUpdate().paint_rects[0]);
}

TEST(PaintAggregator, DisjointPaintBeforeScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect(4, 4, 10, 2);
  greg.InvalidateRect(paint_rect);

  gfx::Rect scroll_rect(0, 0, 2, 10);
  greg.ScrollRect(2, 0, scroll_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());

  EXPECT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_EQ(1U, greg.GetPendingUpdate().paint_rects.size());

  EXPECT_EQ(paint_rect, greg.GetPendingUpdate().paint_rects[0]);
  EXPECT_EQ(scroll_rect, greg.GetPendingUpdate().scroll_rect);
}

TEST(PaintAggregator, DisjointPaintAfterScroll) {
  PaintAggregator greg;

  gfx::Rect scroll_rect(0, 0, 2, 10);
  greg.ScrollRect(2, 0, scroll_rect);

  gfx::Rect paint_rect(4, 4, 10, 2);
  greg.InvalidateRect(paint_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());

  EXPECT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_EQ(1U, greg.GetPendingUpdate().paint_rects.size());

  EXPECT_EQ(paint_rect, greg.GetPendingUpdate().paint_rects[0]);
  EXPECT_EQ(scroll_rect, greg.GetPendingUpdate().scroll_rect);
}

TEST(PaintAggregator, ContainedPaintTrimmedByScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect(4, 4, 6, 6);
  greg.InvalidateRect(paint_rect);

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(2, 0, scroll_rect);

  // The paint rect should have become narrower.
  gfx::Rect expected_paint_rect(6, 4, 4, 6);

  EXPECT_TRUE(greg.HasPendingUpdate());

  EXPECT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_EQ(1U, greg.GetPendingUpdate().paint_rects.size());

  EXPECT_EQ(expected_paint_rect, greg.GetPendingUpdate().paint_rects[0]);
  EXPECT_EQ(scroll_rect, greg.GetPendingUpdate().scroll_rect);
}

TEST(PaintAggregator, ContainedPaintEliminatedByScroll) {
  PaintAggregator greg;

  gfx::Rect paint_rect(4, 4, 6, 6);
  greg.InvalidateRect(paint_rect);

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(6, 0, scroll_rect);

  EXPECT_TRUE(greg.HasPendingUpdate());

  EXPECT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_TRUE(greg.GetPendingUpdate().paint_rects.empty());

  EXPECT_EQ(scroll_rect, greg.GetPendingUpdate().scroll_rect);
}

TEST(PaintAggregator, ContainedPaintAfterScrollTrimmedByScrollDamage) {
  PaintAggregator greg;

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(4, 0, scroll_rect);

  gfx::Rect paint_rect(2, 0, 4, 10);
  greg.InvalidateRect(paint_rect);

  gfx::Rect expected_scroll_damage(0, 0, 4, 10);
  gfx::Rect expected_paint_rect(4, 0, 2, 10);

  EXPECT_TRUE(greg.HasPendingUpdate());

  EXPECT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_EQ(1U, greg.GetPendingUpdate().paint_rects.size());

  EXPECT_EQ(scroll_rect, greg.GetPendingUpdate().scroll_rect);
  EXPECT_EQ(expected_scroll_damage, greg.GetPendingUpdate().GetScrollDamage());
  EXPECT_EQ(expected_paint_rect, greg.GetPendingUpdate().paint_rects[0]);
}

TEST(PaintAggregator, ContainedPaintAfterScrollEliminatedByScrollDamage) {
  PaintAggregator greg;

  gfx::Rect scroll_rect(0, 0, 10, 10);
  greg.ScrollRect(4, 0, scroll_rect);

  gfx::Rect paint_rect(2, 0, 2, 10);
  greg.InvalidateRect(paint_rect);

  gfx::Rect expected_scroll_damage(0, 0, 4, 10);

  EXPECT_TRUE(greg.HasPendingUpdate());

  EXPECT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_TRUE(greg.GetPendingUpdate().paint_rects.empty());

  EXPECT_EQ(scroll_rect, greg.GetPendingUpdate().scroll_rect);
  EXPECT_EQ(expected_scroll_damage, greg.GetPendingUpdate().GetScrollDamage());
}
