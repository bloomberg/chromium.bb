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
  EXPECT_FALSE(greg.GetPendingUpdate().paint_rect.IsEmpty());

  EXPECT_EQ(rect.x(), greg.GetPendingUpdate().paint_rect.x());
  EXPECT_EQ(rect.y(), greg.GetPendingUpdate().paint_rect.y());
  EXPECT_EQ(rect.width(), greg.GetPendingUpdate().paint_rect.width());
  EXPECT_EQ(rect.height(), greg.GetPendingUpdate().paint_rect.height());
}

TEST(PaintAggregator, DoubleDisjointInvalidation) {
  PaintAggregator greg;

  gfx::Rect r1(2, 4, 2, 4);
  gfx::Rect r2(4, 2, 4, 2);

  greg.InvalidateRect(r1);
  greg.InvalidateRect(r2);

  gfx::Rect expected = r1.Union(r2);

  EXPECT_TRUE(greg.HasPendingUpdate());
  EXPECT_TRUE(greg.GetPendingUpdate().scroll_rect.IsEmpty());
  EXPECT_FALSE(greg.GetPendingUpdate().paint_rect.IsEmpty());

  EXPECT_EQ(expected.x(), greg.GetPendingUpdate().paint_rect.x());
  EXPECT_EQ(expected.y(), greg.GetPendingUpdate().paint_rect.y());
  EXPECT_EQ(expected.width(), greg.GetPendingUpdate().paint_rect.width());
  EXPECT_EQ(expected.height(), greg.GetPendingUpdate().paint_rect.height());
}

TEST(PaintAggregator, SingleScroll) {
  PaintAggregator greg;

  gfx::Rect rect(1, 2, 3, 4);
  gfx::Point delta(1, 0);
  greg.ScrollRect(delta.x(), delta.y(), rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  EXPECT_TRUE(greg.GetPendingUpdate().paint_rect.IsEmpty());
  EXPECT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());

  EXPECT_EQ(rect.x(), greg.GetPendingUpdate().scroll_rect.x());
  EXPECT_EQ(rect.y(), greg.GetPendingUpdate().scroll_rect.y());
  EXPECT_EQ(rect.width(), greg.GetPendingUpdate().scroll_rect.width());
  EXPECT_EQ(rect.height(), greg.GetPendingUpdate().scroll_rect.height());

  EXPECT_EQ(delta.x(), greg.GetPendingUpdate().scroll_delta.x());
  EXPECT_EQ(delta.y(), greg.GetPendingUpdate().scroll_delta.y());

  gfx::Rect resulting_damage = greg.GetPendingUpdate().GetScrollDamage();
  gfx::Rect expected_damage(1, 2, 1, 4);
  EXPECT_EQ(expected_damage.x(), resulting_damage.x());
  EXPECT_EQ(expected_damage.y(), resulting_damage.y());
  EXPECT_EQ(expected_damage.width(), resulting_damage.width());
  EXPECT_EQ(expected_damage.height(), resulting_damage.height());
}

TEST(PaintAggregator, DoubleOverlappingScroll) {
  PaintAggregator greg;

  gfx::Rect rect(1, 2, 3, 4);
  gfx::Point delta1(1, 0);
  gfx::Point delta2(1, 0);
  greg.ScrollRect(delta1.x(), delta1.y(), rect);
  greg.ScrollRect(delta2.x(), delta2.y(), rect);

  EXPECT_TRUE(greg.HasPendingUpdate());
  EXPECT_TRUE(greg.GetPendingUpdate().paint_rect.IsEmpty());
  EXPECT_FALSE(greg.GetPendingUpdate().scroll_rect.IsEmpty());

  EXPECT_EQ(rect.x(), greg.GetPendingUpdate().scroll_rect.x());
  EXPECT_EQ(rect.y(), greg.GetPendingUpdate().scroll_rect.y());
  EXPECT_EQ(rect.width(), greg.GetPendingUpdate().scroll_rect.width());
  EXPECT_EQ(rect.height(), greg.GetPendingUpdate().scroll_rect.height());

  gfx::Point expected_delta(delta1.x() + delta2.x(),
                            delta1.y() + delta2.y());
  EXPECT_EQ(expected_delta.x(), greg.GetPendingUpdate().scroll_delta.x());
  EXPECT_EQ(expected_delta.y(), greg.GetPendingUpdate().scroll_delta.y());

  gfx::Rect resulting_damage = greg.GetPendingUpdate().GetScrollDamage();
  gfx::Rect expected_damage(1, 2, 2, 4);
  EXPECT_EQ(expected_damage.x(), resulting_damage.x());
  EXPECT_EQ(expected_damage.y(), resulting_damage.y());
  EXPECT_EQ(expected_damage.width(), resulting_damage.width());
  EXPECT_EQ(expected_damage.height(), resulting_damage.height());
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
  EXPECT_FALSE(greg.GetPendingUpdate().paint_rect.IsEmpty());

  EXPECT_EQ(rect.x(), greg.GetPendingUpdate().paint_rect.x());
  EXPECT_EQ(rect.y(), greg.GetPendingUpdate().paint_rect.y());
  EXPECT_EQ(rect.width(), greg.GetPendingUpdate().paint_rect.width());
  EXPECT_EQ(rect.height(), greg.GetPendingUpdate().paint_rect.height());
}

// TODO(darin): Add tests for mixed scrolling and invalidation
