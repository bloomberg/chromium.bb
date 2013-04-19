// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_pile_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

namespace cc {
namespace {

void RerecordPile(scoped_refptr<FakePicturePileImpl> pile) {
  for (int y = 0; y < pile->num_tiles_y(); ++y) {
    for (int x = 0; x < pile->num_tiles_x(); ++x) {
      pile->RemoveRecordingAt(x, y);
      pile->AddRecordingAt(x, y);
    }
  }
}

TEST(PicturePileImplTest, AnalyzeIsSolidUnscaled) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SkColor solid_color = SkColorSetARGB(255, 12, 23, 34);
  SkPaint solid_paint;
  solid_paint.setColor(solid_color);

  SkColor non_solid_color = SkColorSetARGB(128, 45, 56, 67);
  SkPaint non_solid_paint;
  non_solid_paint.setColor(non_solid_color);

  pile->add_draw_rect_with_paint(gfx::Rect(0, 0, 400, 400), solid_paint);
  RerecordPile(pile);

  // Ensure everything is solid
  for (int y = 0; y <= 300; y += 100) {
    for (int x = 0; x <= 300; x += 100) {
      PicturePileImpl::Analysis analysis;
      gfx::Rect rect(x, y, 100, 100);
      pile->AnalyzeInRect(rect, 1.0, &analysis);
      EXPECT_TRUE(analysis.is_solid_color) << rect.ToString();
      EXPECT_EQ(analysis.solid_color, solid_color) << rect.ToString();
    }
  }

  // One pixel non solid
  pile->add_draw_rect_with_paint(gfx::Rect(50, 50, 1, 1), non_solid_paint);
  RerecordPile(pile);

  PicturePileImpl::Analysis analysis;
  pile->AnalyzeInRect(gfx::Rect(0, 0, 100, 100), 1.0, &analysis);
  EXPECT_FALSE(analysis.is_solid_color);

  pile->AnalyzeInRect(gfx::Rect(100, 0, 100, 100), 1.0, &analysis);
  EXPECT_TRUE(analysis.is_solid_color);
  EXPECT_EQ(analysis.solid_color, solid_color);

  // Boundaries should be clipped
  analysis.is_solid_color = false;
  pile->AnalyzeInRect(gfx::Rect(350, 0, 100, 100), 1.0, &analysis);
  EXPECT_TRUE(analysis.is_solid_color);
  EXPECT_EQ(analysis.solid_color, solid_color);

  analysis.is_solid_color = false;
  pile->AnalyzeInRect(gfx::Rect(0, 350, 100, 100), 1.0, &analysis);
  EXPECT_TRUE(analysis.is_solid_color);
  EXPECT_EQ(analysis.solid_color, solid_color);

  analysis.is_solid_color = false;
  pile->AnalyzeInRect(gfx::Rect(350, 350, 100, 100), 1.0, &analysis);
  EXPECT_TRUE(analysis.is_solid_color);
  EXPECT_EQ(analysis.solid_color, solid_color);
}

TEST(PicturePileImplTest, AnalyzeIsSolidScaled) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SkColor solid_color = SkColorSetARGB(255, 12, 23, 34);
  SkPaint solid_paint;
  solid_paint.setColor(solid_color);

  SkColor non_solid_color = SkColorSetARGB(128, 45, 56, 67);
  SkPaint non_solid_paint;
  non_solid_paint.setColor(non_solid_color);

  pile->add_draw_rect_with_paint(gfx::Rect(0, 0, 400, 400), solid_paint);
  RerecordPile(pile);

  // Ensure everything is solid
  for (int y = 0; y <= 30; y += 10) {
    for (int x = 0; x <= 30; x += 10) {
      PicturePileImpl::Analysis analysis;
      gfx::Rect rect(x, y, 10, 10);
      pile->AnalyzeInRect(rect, 0.1f, &analysis);
      EXPECT_TRUE(analysis.is_solid_color) << rect.ToString();
      EXPECT_EQ(analysis.solid_color, solid_color) << rect.ToString();
    }
  }

  // One pixel non solid
  pile->add_draw_rect_with_paint(gfx::Rect(50, 50, 1, 1), non_solid_paint);
  RerecordPile(pile);

  PicturePileImpl::Analysis analysis;
  pile->AnalyzeInRect(gfx::Rect(0, 0, 10, 10), 0.1f, &analysis);
  EXPECT_FALSE(analysis.is_solid_color);

  pile->AnalyzeInRect(gfx::Rect(10, 0, 10, 10), 0.1f, &analysis);
  EXPECT_TRUE(analysis.is_solid_color);
  EXPECT_EQ(analysis.solid_color, solid_color);

  // Boundaries should be clipped
  analysis.is_solid_color = false;
  pile->AnalyzeInRect(gfx::Rect(35, 0, 10, 10), 0.1f, &analysis);
  EXPECT_TRUE(analysis.is_solid_color);
  EXPECT_EQ(analysis.solid_color, solid_color);

  analysis.is_solid_color = false;
  pile->AnalyzeInRect(gfx::Rect(0, 35, 10, 10), 0.1f, &analysis);
  EXPECT_TRUE(analysis.is_solid_color);
  EXPECT_EQ(analysis.solid_color, solid_color);

  analysis.is_solid_color = false;
  pile->AnalyzeInRect(gfx::Rect(35, 35, 10, 10), 0.1f, &analysis);
  EXPECT_TRUE(analysis.is_solid_color);
  EXPECT_EQ(analysis.solid_color, solid_color);
}


}  // namespace
}  // namespace cc
