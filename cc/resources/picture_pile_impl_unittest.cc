// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "skia/ext/lazy_pixel_ref.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/gfx/rect.h"

namespace cc {
namespace {

class TestPixelRef : public SkPixelRef {
 public:
  // Pure virtual implementation.
  TestPixelRef(int width, int height)
    : pixels_(new char[4 * width * height]) {}
  virtual SkFlattenable::Factory getFactory() OVERRIDE { return NULL; }
  virtual void* onLockPixels(SkColorTable** color_table) OVERRIDE {
      return pixels_.get();
  }
  virtual void onUnlockPixels() OVERRIDE {}
  virtual SkPixelRef* deepCopy(
      SkBitmap::Config config,
      const SkIRect* subset) OVERRIDE {
    this->ref();
    return this;
  }
 private:
  scoped_ptr<char[]> pixels_;
};

class TestLazyPixelRef : public skia::LazyPixelRef {
 public:
  // Pure virtual implementation.
  TestLazyPixelRef(int width, int height)
    : pixels_(new char[4 * width * height]) {}
  virtual SkFlattenable::Factory getFactory() OVERRIDE { return NULL; }
  virtual void* onLockPixels(SkColorTable** color_table) OVERRIDE {
      return pixels_.get();
  }
  virtual void onUnlockPixels() OVERRIDE {}
  virtual bool PrepareToDecode(const PrepareParams& params) OVERRIDE {
    return true;
  }
  virtual SkPixelRef* deepCopy(
      SkBitmap::Config config,
      const SkIRect* subset) OVERRIDE {
    this->ref();
    return this;
  }
  virtual void Decode() OVERRIDE {}
 private:
  scoped_ptr<char[]> pixels_;
};

void CreateBitmap(gfx::Size size, const char* uri, SkBitmap* bitmap) {
  SkAutoTUnref<TestLazyPixelRef> lazy_pixel_ref;
  lazy_pixel_ref.reset(new TestLazyPixelRef(size.width(), size.height()));
  lazy_pixel_ref->setURI(uri);

  bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                    size.width(),
                    size.height());
  bitmap->setPixelRef(lazy_pixel_ref);
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

TEST(PicturePileImplTest, PixelRefIteratorEmpty) {
  gfx::Size tile_size(128, 128);
  gfx::Size layer_bounds(256, 256);

  // Create a filled pile with no recording.
  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  // Tile sized iterators.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128),
        1.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256),
        2.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 64, 64),
        0.5,
        pile);
    EXPECT_FALSE(iterator);
  }
  // Shifted tile sized iterators.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(140, 140, 128, 128),
        1.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(280, 280, 256, 256),
        2.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(70, 70, 64, 64),
        0.5,
        pile);
    EXPECT_FALSE(iterator);
  }
  // Layer sized iterators.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256),
        1.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 512, 512),
        2.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128),
        0.5,
        pile);
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
        gfx::Rect(0, 0, 128, 128),
        1.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256),
        2.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 64, 64),
        0.5,
        pile);
    EXPECT_FALSE(iterator);
  }
  // Shifted tile sized iterators.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(140, 140, 128, 128),
        1.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(280, 280, 256, 256),
        2.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(70, 70, 64, 64),
        0.5,
        pile);
    EXPECT_FALSE(iterator);
  }
  // Layer sized iterators.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256),
        1.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 512, 512),
        2.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128),
        0.5,
        pile);
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
        gfx::Rect(0, 0, 128, 128),
        1.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256),
        2.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 64, 64),
        0.5,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  // Shifted tile sized iterators. These should find only one pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(140, 140, 128, 128),
        1.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(280, 280, 256, 256),
        2.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(70, 70, 64, 64),
        0.5,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  // Ensure there's no lazy pixel refs in the empty cell
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(140, 0, 128, 128),
        1.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  // Layer sized iterators. These should find all 3 pixel refs.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 256, 256),
        1.0,
        pile);
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
        gfx::Rect(0, 0, 512, 512),
        2.0,
        pile);
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
        gfx::Rect(0, 0, 128, 128),
        0.5,
        pile);
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
        gfx::Rect(0, 0, 256, 256),
        1.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 512, 512),
        2.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128),
        0.5,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  // Shifted tile sized iterators. These should find only one pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(260, 260, 256, 256),
        1.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(520, 520, 512, 512),
        2.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(130, 130, 128, 128),
        0.5,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  // Ensure there's no lazy pixel refs in the empty cell
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 256, 256, 256),
        1.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  // Layer sized iterators. These should find three pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 512, 512),
        1.0,
        pile);
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
        gfx::Rect(0, 0, 1024, 1024),
        2.0,
        pile);
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
        gfx::Rect(0, 0, 256, 256),
        0.5,
        pile);
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
      gfx::Rect(0, 0, 512, 512),
      1.0,
      pile);
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
        gfx::Rect(0, 0, 256, 256),
        1.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 512, 512),
        2.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128),
        0.5,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  // Shifted tile sized iterators. These should find only one pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(260, 260, 256, 256),
        1.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(520, 520, 512, 512),
        2.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(130, 130, 128, 128),
        0.5,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  // Ensure there's no lazy pixel refs in the empty cell
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 256, 256, 256),
        1.0,
        pile);
    EXPECT_FALSE(iterator);
  }
  // Layer sized iterators. These should find three pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 512, 512),
        1.0,
        pile);
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
        gfx::Rect(0, 0, 1024, 1024),
        2.0,
        pile);
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
        gfx::Rect(0, 0, 256, 256),
        0.5,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][1].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
}

TEST(PicturePileImplTest, PixelRefIteratorMultiplePictures) {
  gfx::Size tile_size(256, 256);
  gfx::Size layer_bounds(256, 256);

  SkTileGridPicture::TileGridInfo tile_grid_info;
  tile_grid_info.fTileInterval = SkISize::Make(256, 256);
  tile_grid_info.fMargin.setEmpty();
  tile_grid_info.fOffset.setZero();

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(tile_size, layer_bounds);

  SkBitmap lazy_bitmap[2][2];
  CreateBitmap(gfx::Size(32, 32), "lazy", &lazy_bitmap[0][0]);
  CreateBitmap(gfx::Size(32, 32), "lazy", &lazy_bitmap[0][1]);
  CreateBitmap(gfx::Size(32, 32), "lazy", &lazy_bitmap[1][1]);
  SkBitmap non_lazy_bitmap;
  CreateBitmap(gfx::Size(256, 256), "notlazy", &non_lazy_bitmap);

  // Each bitmap goes into its own picture, the final layout
  // has lazy pixel refs in the following regions:
  // ||=======||
  // ||x|   |x||
  // ||--   --||
  // ||     |x||
  // ||=======||
  pile->add_draw_bitmap(non_lazy_bitmap, gfx::Point(0, 0));
  pile->RerecordPile();

  FakeContentLayerClient content_layer_clients[2][2];
  scoped_refptr<Picture> pictures[2][2];
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) {
      if (x == 0 && y == 1)
        continue;
      content_layer_clients[y][x].add_draw_bitmap(
          lazy_bitmap[y][x],
          gfx::Point(x * 128 + 10, y * 128 + 10));
      pictures[y][x] = Picture::Create(
          gfx::Rect(x * 128 + 10, y * 128 + 10, 64, 64));
      pictures[y][x]->Record(
          &content_layer_clients[y][x],
          tile_grid_info,
          NULL);
      pictures[y][x]->GatherPixelRefs(tile_grid_info, NULL);
      pile->AddPictureToRecording(0, 0, pictures[y][x]);
    }
  }

  // These should find only one pixel ref.
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 0, 128, 128),
        1.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][0].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(128, 0, 128, 128),
        1.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[0][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(128, 128, 128, 128),
        1.0,
        pile);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][1].pixelRef());
    EXPECT_FALSE(++iterator);
  }
  // This one should not find any refs
  {
    PicturePileImpl::PixelRefIterator iterator(
        gfx::Rect(0, 128, 128, 128),
        1.0,
        pile);
    EXPECT_FALSE(iterator);
  }
}

}  // namespace
}  // namespace cc
