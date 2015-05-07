// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_image_layer.h"

#include "cc/resources/display_item.h"
#include "cc/test/skia_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
namespace {

TEST(PictureImageLayerTest, PaintContentsToDisplayList) {
  scoped_refptr<PictureImageLayer> layer = PictureImageLayer::Create();
  gfx::Rect layer_rect(200, 200);

  SkBitmap image_bitmap;
  unsigned char image_pixels[4 * 200 * 200] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  image_bitmap.installPixels(info, image_pixels, info.minRowBytes());
  SkCanvas image_canvas(image_bitmap);
  image_canvas.clear(SK_ColorRED);
  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  image_canvas.drawRectCoords(0.f, 0.f, 100.f, 100.f, blue_paint);
  image_canvas.drawRectCoords(100.f, 100.f, 200.f, 200.f, blue_paint);

  layer->SetBitmap(image_bitmap);
  layer->SetBounds(gfx::Size(layer_rect.width(), layer_rect.height()));

  bool use_cached_picture = false;
  scoped_refptr<DisplayItemList> display_list =
      DisplayItemList::Create(layer_rect, use_cached_picture);
  layer->PaintContentsToDisplayList(
      display_list.get(), layer_rect,
      ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->ProcessAppendedItems();
  unsigned char actual_pixels[4 * 200 * 200] = {0};
  DrawDisplayList(actual_pixels, layer_rect, display_list);

  EXPECT_EQ(0, memcmp(actual_pixels, image_pixels, 4 * 200 * 200));
}

TEST(PictureImageLayerTest, PaintContentsToCachedDisplayList) {
  scoped_refptr<PictureImageLayer> layer = PictureImageLayer::Create();
  gfx::Rect layer_rect(200, 200);

  SkBitmap image_bitmap;
  unsigned char image_pixels[4 * 200 * 200] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  image_bitmap.installPixels(info, image_pixels, info.minRowBytes());
  SkCanvas image_canvas(image_bitmap);
  image_canvas.clear(SK_ColorRED);
  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  image_canvas.drawRectCoords(0.f, 0.f, 100.f, 100.f, blue_paint);
  image_canvas.drawRectCoords(100.f, 100.f, 200.f, 200.f, blue_paint);

  layer->SetBitmap(image_bitmap);
  layer->SetBounds(gfx::Size(layer_rect.width(), layer_rect.height()));

  bool use_cached_picture = true;
  scoped_refptr<DisplayItemList> display_list =
      DisplayItemList::Create(layer_rect, use_cached_picture);
  layer->PaintContentsToDisplayList(
      display_list.get(), layer_rect,
      ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  display_list->ProcessAppendedItems();
  display_list->CreateAndCacheSkPicture();
  unsigned char actual_pixels[4 * 200 * 200] = {0};
  DrawDisplayList(actual_pixels, layer_rect, display_list);

  EXPECT_EQ(0, memcmp(actual_pixels, image_pixels, 4 * 200 * 200));
}

}  // namespace
}  // namespace cc
