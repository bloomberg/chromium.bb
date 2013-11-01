// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/test/skia_common.h"
#include "skia/ext/lazy_pixel_ref.h"
#include "skia/ext/refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size_conversions.h"

namespace cc {
namespace {

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
  pile->RerecordPile();

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
  pile->RerecordPile();

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
  pile->RerecordPile();

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
  pile->RerecordPile();

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

TEST(PicturePileImplTest, AnalyzeIsSolidEmpty) {
  gfx::Size tile_size(100, 100);
  gfx::Size layer_bounds(400, 400);

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  PicturePileImpl::Analysis analysis;
  EXPECT_FALSE(analysis.is_solid_color);

  pile->AnalyzeInRect(gfx::Rect(0, 0, 400, 400), 1.f, &analysis);

  EXPECT_TRUE(analysis.is_solid_color);
  EXPECT_EQ(analysis.solid_color, SkColorSetARGB(0, 0, 0, 0));
}

TEST(PicturePileImplTest, PixelRefIteratorEmpty) {
  gfx::Size tile_size(128, 128);
  gfx::Size layer_bounds(256, 256);

  // Create a filled pile with no recording.
  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  // Tile sized iterators.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128), 1.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256), 2.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 64, 64), 0.5, pile.get());
    EXPECT_FALSE(iterator);
  }
  // Shifted tile sized iterators.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(140, 140, 128, 128), 1.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(280, 280, 256, 256), 2.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(70, 70, 64, 64), 0.5, pile.get());
    EXPECT_FALSE(iterator);
  }
  // Layer sized iterators.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256), 1.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 512, 512), 2.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128), 0.5, pile.get());
    EXPECT_FALSE(iterator);
  }
}

TEST(PicturePileImplTest, PixelRefIteratorNoLazyRefs) {
  gfx::Size tile_size(128, 128);
  gfx::Size layer_bounds(256, 256);

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SkPaint simple_paint;
  simple_paint.setColor(SkColorSetARGB(255, 12, 23, 34));

  SkBitmap non_lazy_bitmap;
  CreateBitmap(gfx::Size(128, 128), "notlazy", &non_lazy_bitmap);

  pile->add_draw_rect_with_paint(gfx::Rect(0, 0, 256, 256), simple_paint);
  pile->add_draw_rect_with_paint(gfx::Rect(128, 128, 512, 512), simple_paint);
  pile->add_draw_rect_with_paint(gfx::Rect(512, 0, 256, 256), simple_paint);
  pile->add_draw_rect_with_paint(gfx::Rect(0, 512, 256, 256), simple_paint);
  pile->add_draw_bitmap(non_lazy_bitmap, gfx::Point(128, 0));
  pile->add_draw_bitmap(non_lazy_bitmap, gfx::Point(0, 128));
  pile->add_draw_bitmap(non_lazy_bitmap, gfx::Point(150, 150));

  pile->RerecordPile();

  // Tile sized iterators.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128), 1.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256), 2.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 64, 64), 0.5, pile.get());
    EXPECT_FALSE(iterator);
  }
  // Shifted tile sized iterators.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(140, 140, 128, 128), 1.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(280, 280, 256, 256), 2.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(70, 70, 64, 64), 0.5, pile.get());
    EXPECT_FALSE(iterator);
  }
  // Layer sized iterators.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256), 1.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 512, 512), 2.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128), 0.5, pile.get());
    EXPECT_FALSE(iterator);
  }
}

TEST(PicturePileImplTest, PixelRefIteratorLazyRefs) {
  gfx::Size tile_size(128, 128);
  gfx::Size layer_bounds(256, 256);

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SkBitmap lazy_bitmap[2][2];
  CreateBitmap(gfx::Size(32, 32), "lazy", &lazy_bitmap[0][0]);
  CreateBitmap(gfx::Size(32, 32), "lazy", &lazy_bitmap[1][0]);
  CreateBitmap(gfx::Size(32, 32), "lazy", &lazy_bitmap[1][1]);

  // Lazy pixel refs are found in the following cells:
  // |---|---|
  // | x |   |
  // |---|---|
  // | x | x |
  // |---|---|
  pile->add_draw_bitmap(lazy_bitmap[0][0], gfx::Point(0, 0));
  pile->add_draw_bitmap(lazy_bitmap[1][0], gfx::Point(0, 130));
  pile->add_draw_bitmap(lazy_bitmap[1][1], gfx::Point(140, 140));

  pile->RerecordPile();

  // Tile sized iterators. These should find only one pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128), 1.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256), 2.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 64, 64), 0.5, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  // Shifted tile sized iterators. These should find only one pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(140, 140, 128, 128), 1.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(280, 280, 256, 256), 2.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(70, 70, 64, 64), 0.5, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  // Ensure there's no lazy pixel refs in the empty cell
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(140, 0, 128, 128), 1.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  // Layer sized iterators. These should find all 3 pixel refs.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256), 1.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][0].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 512, 512), 2.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][0].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128), 0.5, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][0].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
}

TEST(PicturePileImplTest, PixelRefIteratorLazyRefsOneTile) {
  gfx::Size tile_size(256, 256);
  gfx::Size layer_bounds(512, 512);

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SkBitmap lazy_bitmap[2][2];
  CreateBitmap(gfx::Size(32, 32), "lazy", &lazy_bitmap[0][0]);
  CreateBitmap(gfx::Size(32, 32), "lazy", &lazy_bitmap[0][1]);
  CreateBitmap(gfx::Size(32, 32), "lazy", &lazy_bitmap[1][1]);

  // Lazy pixel refs are found in the following cells:
  // |---|---|
  // | x | x |
  // |---|---|
  // |   | x |
  // |---|---|
  pile->add_draw_bitmap(lazy_bitmap[0][0], gfx::Point(0, 0));
  pile->add_draw_bitmap(lazy_bitmap[0][1], gfx::Point(260, 0));
  pile->add_draw_bitmap(lazy_bitmap[1][1], gfx::Point(260, 260));

  pile->RerecordPile();

  // Tile sized iterators. These should find only one pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256), 1.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 512, 512), 2.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128), 0.5, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  // Shifted tile sized iterators. These should find only one pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(260, 260, 256, 256), 1.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(520, 520, 512, 512), 2.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(130, 130, 128, 128), 0.5, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  // Ensure there's no lazy pixel refs in the empty cell
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 256, 256, 256), 1.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  // Layer sized iterators. These should find three pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 512, 512), 1.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][1].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 1024, 1024), 2.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][1].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256), 0.5, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][1].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }

  // Copy test.
  PicturePileImpl::PixelRefIterator iterator(
      gfx::Rect(0, 0, 512, 512), 1.0, pile.get());
  EXPECT_TRUE(iterator);
  EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
  EXPECT_TRUE(++iterator);
  EXPECT_TRUE(*iterator == lazy_bitmap[0][1].pixelRef());

  // copy now points to the same spot as iterator,
  // but both can be incremented independently.
  PicturePileImpl::PixelRefIterator copy = iterator;
  EXPECT_TRUE(++iterator);
  EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
  EXPECT_FALSE(++iterator);

  EXPECT_TRUE(copy);
  EXPECT_TRUE(*copy == lazy_bitmap[0][1].pixelRef());
  EXPECT_TRUE(++copy);
  EXPECT_TRUE(*copy == lazy_bitmap[1][1].pixelRef());
  EXPECT_FALSE(++copy);
}

TEST(PicturePileImplTest, PixelRefIteratorLazyRefsBaseNonLazy) {
  gfx::Size tile_size(256, 256);
  gfx::Size layer_bounds(512, 512);

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SkBitmap non_lazy_bitmap;
  CreateBitmap(gfx::Size(512, 512), "notlazy", &non_lazy_bitmap);

  SkBitmap lazy_bitmap[2][2];
  CreateBitmap(gfx::Size(128, 128), "lazy", &lazy_bitmap[0][0]);
  CreateBitmap(gfx::Size(128, 128), "lazy", &lazy_bitmap[0][1]);
  CreateBitmap(gfx::Size(128, 128), "lazy", &lazy_bitmap[1][1]);

  // One large non-lazy bitmap covers the whole grid.
  // Lazy pixel refs are found in the following cells:
  // |---|---|
  // | x | x |
  // |---|---|
  // |   | x |
  // |---|---|
  pile->add_draw_bitmap(non_lazy_bitmap, gfx::Point(0, 0));
  pile->add_draw_bitmap(lazy_bitmap[0][0], gfx::Point(0, 0));
  pile->add_draw_bitmap(lazy_bitmap[0][1], gfx::Point(260, 0));
  pile->add_draw_bitmap(lazy_bitmap[1][1], gfx::Point(260, 260));

  pile->RerecordPile();

  // Tile sized iterators. These should find only one pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256), 1.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 512, 512), 2.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128), 0.5, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  // Shifted tile sized iterators. These should find only one pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(260, 260, 256, 256), 1.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(520, 520, 512, 512), 2.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(130, 130, 128, 128), 0.5, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  // Ensure there's no lazy pixel refs in the empty cell
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 256, 256, 256), 1.0, pile.get());
    EXPECT_FALSE(iterator);
  }
  // Layer sized iterators. These should find three pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 512, 512), 1.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][1].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 1024, 1024), 2.0, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][1].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256), 0.5, pile.get());
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][1].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
}

TEST(PicturePileImpl, RasterContentsOpaque) {
  gfx::Size tile_size(1000, 1000);
  gfx::Size layer_bounds(3, 5);
  float contents_scale = 1.5f;
  float raster_divisions = 2.f;

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  // Because the caller sets content opaque, it also promises that it
  // has at least filled in layer_bounds opaquely.
  SkPaint white_paint;
  white_paint.setColor(SK_ColorWHITE);
  pile->add_draw_rect_with_paint(gfx::Rect(layer_bounds), white_paint);

  pile->SetMinContentsScale(contents_scale);
  pile->set_background_color(SK_ColorBLACK);
  pile->set_contents_opaque(true);
  pile->set_clear_canvas_with_debug_color(false);
  pile->RerecordPile();

  gfx::Size content_bounds(
      gfx::ToCeiledSize(gfx::ScaleSize(layer_bounds, contents_scale)));

  // Simulate drawing into different tiles at different offsets.
  int step_x = std::ceil(content_bounds.width() / raster_divisions);
  int step_y = std::ceil(content_bounds.height() / raster_divisions);
  for (int offset_x = 0; offset_x < content_bounds.width();
       offset_x += step_x) {
    for (int offset_y = 0; offset_y < content_bounds.height();
         offset_y += step_y) {
      gfx::Rect content_rect(offset_x, offset_y, step_x, step_y);
      content_rect.Intersect(gfx::Rect(content_bounds));

      // Simulate a canvas rect larger than the content rect.  Every pixel
      // up to one pixel outside the content rect is guaranteed to be opaque.
      // Outside of that is undefined.
      gfx::Rect canvas_rect(content_rect);
      canvas_rect.Inset(0, 0, -1, -1);

      SkBitmap bitmap;
      bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                       canvas_rect.width(),
                       canvas_rect.height());
      bitmap.allocPixels();
      SkCanvas canvas(bitmap);
      canvas.clear(SK_ColorTRANSPARENT);

      FakeRenderingStatsInstrumentation rendering_stats_instrumentation;

      pile->RasterToBitmap(&canvas,
                           canvas_rect,
                           contents_scale,
                           &rendering_stats_instrumentation);

      SkColor* pixels = reinterpret_cast<SkColor*>(bitmap.getPixels());
      int num_pixels = bitmap.width() * bitmap.height();
      bool all_white = true;
      for (int i = 0; i < num_pixels; ++i) {
        EXPECT_EQ(SkColorGetA(pixels[i]), 255u);
        all_white &= (SkColorGetR(pixels[i]) == 255);
        all_white &= (SkColorGetG(pixels[i]) == 255);
        all_white &= (SkColorGetB(pixels[i]) == 255);
      }

      // If the canvas doesn't extend past the edge of the content,
      // it should be entirely white. Otherwise, the edge of the content
      // will be non-white.
      EXPECT_EQ(all_white, gfx::Rect(content_bounds).Contains(canvas_rect));
    }
  }
}

TEST(PicturePileImpl, RasterContentsTransparent) {
  gfx::Size tile_size(1000, 1000);
  gfx::Size layer_bounds(5, 3);
  float contents_scale = 0.5f;

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);
  pile->set_background_color(SK_ColorTRANSPARENT);
  pile->set_contents_opaque(false);
  pile->SetMinContentsScale(contents_scale);
  pile->set_clear_canvas_with_debug_color(false);
  pile->RerecordPile();

  gfx::Size content_bounds(
      gfx::ToCeiledSize(gfx::ScaleSize(layer_bounds, contents_scale)));

  gfx::Rect canvas_rect(content_bounds);
  canvas_rect.Inset(0, 0, -1, -1);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                   canvas_rect.width(),
                   canvas_rect.height());
  bitmap.allocPixels();
  SkCanvas canvas(bitmap);

  FakeRenderingStatsInstrumentation rendering_stats_instrumentation;
  pile->RasterToBitmap(
      &canvas, canvas_rect, contents_scale, &rendering_stats_instrumentation);

  SkColor* pixels = reinterpret_cast<SkColor*>(bitmap.getPixels());
  int num_pixels = bitmap.width() * bitmap.height();
  for (int i = 0; i < num_pixels; ++i) {
    EXPECT_EQ(SkColorGetA(pixels[i]), 0u);
  }
}

}  // namespace
}  // namespace cc
