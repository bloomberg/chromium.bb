// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/test/fake_content_layer_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkTileGridPicture.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

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

void DrawPicture(unsigned char* buffer,
                 gfx::Rect layer_rect,
                 scoped_refptr<Picture> picture) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                   layer_rect.width(),
                   layer_rect.height());
  bitmap.setPixels(buffer);
  SkDevice device(bitmap);
  SkCanvas canvas(&device);
  canvas.clipRect(gfx::RectToSkRect(layer_rect));
  picture->Raster(&canvas, layer_rect, 1.0f, false);
}

void CreateBitmap(gfx::Size size, const char* uri, SkBitmap* bitmap) {
  SkAutoTUnref<TestLazyPixelRef> lazy_pixel_ref;
  lazy_pixel_ref.reset(new TestLazyPixelRef(size.width(), size.height()));
  lazy_pixel_ref->setURI(uri);

  bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                    size.width(),
                    size.height());
  bitmap->setPixelRef(lazy_pixel_ref);
}

TEST(PictureTest, AsBase64String) {
  SkGraphics::Init();

  gfx::Rect layer_rect(100, 100);

  SkTileGridPicture::TileGridInfo tile_grid_info;
  tile_grid_info.fTileInterval = SkISize::Make(100, 100);
  tile_grid_info.fMargin.setEmpty();
  tile_grid_info.fOffset.setZero();

  FakeContentLayerClient content_layer_client;

  SkPaint red_paint;
  red_paint.setColor(SkColorSetARGB(255, 255, 0, 0));
  SkPaint green_paint;
  green_paint.setColor(SkColorSetARGB(255, 0, 255, 0));

  // Invalid picture (not base64).
  scoped_refptr<Picture> invalid_picture =
      Picture::CreateFromBase64String("abc!@#$%");
  EXPECT_TRUE(!invalid_picture);

  // Invalid picture (empty string).
  scoped_refptr<Picture> second_invalid_picture =
      Picture::CreateFromBase64String("");
  EXPECT_TRUE(!second_invalid_picture);

  // Invalid picture (random base64 string).
  scoped_refptr<Picture> third_invalid_picture =
      Picture::CreateFromBase64String("ABCDABCDABCDABCDABCDABCDABCDABCDABCD"
                                      "ABCDABCDABCDABCDABCDABCDABCDABCDABCD"
                                      "ABCDABCDABCDABCDABCDABCDABCDABCDABCD"
                                      "ABCDABCDABCDABCDABCDABCDABCDABCDABCD"
                                      "ABCDABCDABCDABCDABCDABCDABCDABCDABCD"
                                      "ABCDABCDABCDABCDABCDABCDABCDABCDABCD"
                                      "ABCDABCDABCDABCDABCDABCDABCDABCDABCD");
  EXPECT_TRUE(!third_invalid_picture);

  // Single full-size rect picture.
  content_layer_client.add_draw_rect(layer_rect, red_paint);
  scoped_refptr<Picture> one_rect_picture = Picture::Create(layer_rect);
  one_rect_picture->Record(&content_layer_client, tile_grid_info, NULL);
  std::string serialized_one_rect;
  one_rect_picture->AsBase64String(&serialized_one_rect);

  // Reconstruct the picture.
  scoped_refptr<Picture> one_rect_picture_check =
      Picture::CreateFromBase64String(serialized_one_rect);
  EXPECT_TRUE(!!one_rect_picture_check);

  // Check for equivalence.
  unsigned char one_rect_buffer[4 * 100 * 100] = {0};
  DrawPicture(one_rect_buffer, layer_rect, one_rect_picture);
  unsigned char one_rect_buffer_check[4 * 100 * 100] = {0};
  DrawPicture(one_rect_buffer_check, layer_rect, one_rect_picture_check);

  EXPECT_EQ(one_rect_picture->LayerRect(),
            one_rect_picture_check->LayerRect());
  EXPECT_EQ(one_rect_picture->OpaqueRect(),
            one_rect_picture_check->OpaqueRect());
  EXPECT_TRUE(
      memcmp(one_rect_buffer, one_rect_buffer_check, 4 * 100 * 100) == 0);

  // Two rect picture.
  content_layer_client.add_draw_rect(gfx::Rect(25, 25, 50, 50), green_paint);
  scoped_refptr<Picture> two_rect_picture = Picture::Create(layer_rect);
  two_rect_picture->Record(&content_layer_client, tile_grid_info, NULL);
  std::string serialized_two_rect;
  two_rect_picture->AsBase64String(&serialized_two_rect);

  // Reconstruct the picture.
  scoped_refptr<Picture> two_rect_picture_check =
      Picture::CreateFromBase64String(serialized_two_rect);
  EXPECT_TRUE(!!two_rect_picture_check);

  // Check for equivalence.
  unsigned char two_rect_buffer[4 * 100 * 100] = {0};
  DrawPicture(two_rect_buffer, layer_rect, two_rect_picture);
  unsigned char two_rect_buffer_check[4 * 100 * 100] = {0};
  DrawPicture(two_rect_buffer_check, layer_rect, two_rect_picture_check);

  EXPECT_EQ(two_rect_picture->LayerRect(),
            two_rect_picture_check->LayerRect());
  EXPECT_EQ(two_rect_picture->OpaqueRect(),
            two_rect_picture_check->OpaqueRect());
  EXPECT_TRUE(
      memcmp(two_rect_buffer, two_rect_buffer_check, 4 * 100 * 100) == 0);
}

TEST(PictureTest, PixelRefIterator) {
  gfx::Rect layer_rect(2048, 2048);

  SkTileGridPicture::TileGridInfo tile_grid_info;
  tile_grid_info.fTileInterval = SkISize::Make(512, 512);
  tile_grid_info.fMargin.setEmpty();
  tile_grid_info.fOffset.setZero();

  FakeContentLayerClient content_layer_client;

  // Lazy pixel refs are found in the following grids:
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  SkBitmap lazy_bitmap[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        CreateBitmap(gfx::Size(500, 500), "lazy", &lazy_bitmap[y][x]);
        content_layer_client.add_draw_bitmap(
            lazy_bitmap[y][x],
            gfx::Point(x * 512 + 6, y * 512 + 6));
      }
    }
  }

  scoped_refptr<Picture> picture = Picture::Create(layer_rect);
  picture->Record(&content_layer_client, tile_grid_info, NULL);
  picture->GatherPixelRefs(tile_grid_info, NULL);

  // Default iterator does not have any pixel refs
  {
    Picture::PixelRefIterator iterator;
    EXPECT_FALSE(iterator);
  }
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      Picture::PixelRefIterator iterator(
          gfx::Rect(x * 512, y * 512, 500, 500),
          picture);
      if ((x + y) & 1) {
        EXPECT_TRUE(iterator) << x << " " << y;
        EXPECT_TRUE(*iterator == lazy_bitmap[y][x].pixelRef()) << x << " " << y;
        EXPECT_FALSE(++iterator) << x << " " << y;
      } else {
        EXPECT_FALSE(iterator) << x << " " << y;
      }
    }
  }
  // Capture 4 pixel refs.
  {
    Picture::PixelRefIterator iterator(
        gfx::Rect(512, 512, 2048, 2048),
        picture);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][2].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[2][1].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[2][3].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[3][2].pixelRef());
    EXPECT_FALSE(++iterator);
  }

  // Copy test.
  Picture::PixelRefIterator iterator(
        gfx::Rect(512, 512, 2048, 2048),
        picture);
  EXPECT_TRUE(iterator);
  EXPECT_TRUE(*iterator == lazy_bitmap[1][2].pixelRef());
  EXPECT_TRUE(++iterator);
  EXPECT_TRUE(*iterator == lazy_bitmap[2][1].pixelRef());

  // copy now points to the same spot as iterator,
  // but both can be incremented independently.
  Picture::PixelRefIterator copy = iterator;
  EXPECT_TRUE(++iterator);
  EXPECT_TRUE(*iterator == lazy_bitmap[2][3].pixelRef());
  EXPECT_TRUE(++iterator);
  EXPECT_TRUE(*iterator == lazy_bitmap[3][2].pixelRef());
  EXPECT_FALSE(++iterator);

  EXPECT_TRUE(copy);
  EXPECT_TRUE(*copy == lazy_bitmap[2][1].pixelRef());
  EXPECT_TRUE(++copy);
  EXPECT_TRUE(*copy == lazy_bitmap[2][3].pixelRef());
  EXPECT_TRUE(++copy);
  EXPECT_TRUE(*copy == lazy_bitmap[3][2].pixelRef());
  EXPECT_FALSE(++copy);
}

TEST(PictureTest, PixelRefIteratorNonZeroLayer) {
  gfx::Rect layer_rect(1024, 0, 2048, 2048);

  SkTileGridPicture::TileGridInfo tile_grid_info;
  tile_grid_info.fTileInterval = SkISize::Make(512, 512);
  tile_grid_info.fMargin.setEmpty();
  tile_grid_info.fOffset.setZero();

  FakeContentLayerClient content_layer_client;

  // Lazy pixel refs are found in the following grids:
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  SkBitmap lazy_bitmap[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        CreateBitmap(gfx::Size(500, 500), "lazy", &lazy_bitmap[y][x]);
        content_layer_client.add_draw_bitmap(
            lazy_bitmap[y][x],
            gfx::Point(1024 + x * 512 + 6, y * 512 + 6));
      }
    }
  }

  scoped_refptr<Picture> picture = Picture::Create(layer_rect);
  picture->Record(&content_layer_client, tile_grid_info, NULL);
  picture->GatherPixelRefs(tile_grid_info, NULL);

  // Default iterator does not have any pixel refs
  {
    Picture::PixelRefIterator iterator;
    EXPECT_FALSE(iterator);
  }
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      Picture::PixelRefIterator iterator(
          gfx::Rect(1024 + x * 512, y * 512, 500, 500),
          picture);
      if ((x + y) & 1) {
        EXPECT_TRUE(iterator) << x << " " << y;
        EXPECT_TRUE(*iterator == lazy_bitmap[y][x].pixelRef());
        EXPECT_FALSE(++iterator) << x << " " << y;
      } else {
        EXPECT_FALSE(iterator) << x << " " << y;
      }
    }
  }
  // Capture 4 pixel refs.
  {
    Picture::PixelRefIterator iterator(
        gfx::Rect(1024 + 512, 512, 2048, 2048),
        picture);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][2].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[2][1].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[2][3].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[3][2].pixelRef());
    EXPECT_FALSE(++iterator);
  }

  // Copy test.
  {
    Picture::PixelRefIterator iterator(
          gfx::Rect(1024 + 512, 512, 2048, 2048),
          picture);
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[1][2].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[2][1].pixelRef());

    // copy now points to the same spot as iterator,
    // but both can be incremented independently.
    Picture::PixelRefIterator copy = iterator;
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[2][3].pixelRef());
    EXPECT_TRUE(++iterator);
    EXPECT_TRUE(*iterator == lazy_bitmap[3][2].pixelRef());
    EXPECT_FALSE(++iterator);

    EXPECT_TRUE(copy);
    EXPECT_TRUE(*copy == lazy_bitmap[2][1].pixelRef());
    EXPECT_TRUE(++copy);
    EXPECT_TRUE(*copy == lazy_bitmap[2][3].pixelRef());
    EXPECT_TRUE(++copy);
    EXPECT_TRUE(*copy == lazy_bitmap[3][2].pixelRef());
    EXPECT_FALSE(++copy);
  }

  // Non intersecting rects
  {
    Picture::PixelRefIterator iterator(
        gfx::Rect(0, 0, 1000, 1000),
        picture);
    EXPECT_FALSE(iterator);
  }
  {
    Picture::PixelRefIterator iterator(
        gfx::Rect(3500, 0, 1000, 1000),
        picture);
    EXPECT_FALSE(iterator);
  }
  {
    Picture::PixelRefIterator iterator(
        gfx::Rect(0, 1100, 1000, 1000),
        picture);
    EXPECT_FALSE(iterator);
  }
  {
    Picture::PixelRefIterator iterator(
        gfx::Rect(3500, 1100, 1000, 1000),
        picture);
    EXPECT_FALSE(iterator);
  }
}

TEST(PictureTest, PixelRefIteratorOnePixelQuery) {
  gfx::Rect layer_rect(2048, 2048);

  SkTileGridPicture::TileGridInfo tile_grid_info;
  tile_grid_info.fTileInterval = SkISize::Make(512, 512);
  tile_grid_info.fMargin.setEmpty();
  tile_grid_info.fOffset.setZero();

  FakeContentLayerClient content_layer_client;

  // Lazy pixel refs are found in the following grids:
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  // |   | x |   | x |
  // |---|---|---|---|
  // | x |   | x |   |
  // |---|---|---|---|
  SkBitmap lazy_bitmap[4][4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      if ((x + y) & 1) {
        CreateBitmap(gfx::Size(500, 500), "lazy", &lazy_bitmap[y][x]);
        content_layer_client.add_draw_bitmap(
            lazy_bitmap[y][x],
            gfx::Point(x * 512 + 6, y * 512 + 6));
      }
    }
  }

  scoped_refptr<Picture> picture = Picture::Create(layer_rect);
  picture->Record(&content_layer_client, tile_grid_info, NULL);
  picture->GatherPixelRefs(tile_grid_info, NULL);

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      Picture::PixelRefIterator iterator(
          gfx::Rect(x * 512, y * 512 + 256, 1, 1),
          picture);
      if ((x + y) & 1) {
        EXPECT_TRUE(iterator) << x << " " << y;
        EXPECT_TRUE(*iterator == lazy_bitmap[y][x].pixelRef());
        EXPECT_FALSE(++iterator) << x << " " << y;
      } else {
        EXPECT_FALSE(iterator) << x << " " << y;
      }
    }
  }
}
}  // namespace
}  // namespace cc
