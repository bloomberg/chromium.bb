// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture.h"

#include "base/memory/ref_counted.h"
#include "cc/test/fake_content_layer_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkTileGridPicture.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

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
  one_rect_picture->Record(&content_layer_client, NULL, tile_grid_info);
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
  two_rect_picture->Record(&content_layer_client, NULL, tile_grid_info);
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

}  // namespace
}  // namespace cc
