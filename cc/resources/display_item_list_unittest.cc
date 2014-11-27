// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/display_item_list.h"

#include <vector>

#include "cc/resources/clip_display_item.h"
#include "cc/resources/drawing_display_item.h"
#include "cc/resources/transform_display_item.h"
#include "cc/test/skia_common.h"
#include "skia/ext/refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

TEST(DisplayItemListTest, SingleDrawingItem) {
  gfx::Rect layer_rect(100, 100);
  SkPictureRecorder recorder;
  skia::RefPtr<SkCanvas> canvas;
  skia::RefPtr<SkPicture> picture;
  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  SkPaint red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};
  scoped_refptr<DisplayItemList> list = DisplayItemList::Create();

  canvas = skia::SharePtr(
      recorder.beginRecording(layer_rect.width(), layer_rect.height()));
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  picture = skia::AdoptRef(recorder.endRecording());
  gfx::PointF offset(8.f, 9.f);
  list->AppendItem(DrawingDisplayItem::Create(picture, offset));
  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkCanvas expected_canvas(expected_bitmap);
  expected_canvas.clipRect(gfx::RectToSkRect(layer_rect));
  expected_canvas.drawRectCoords(0.f + offset.x(), 0.f + offset.y(),
                                 60.f + offset.x(), 60.f + offset.y(),
                                 red_paint);
  expected_canvas.drawRectCoords(50.f + offset.x(), 50.f + offset.y(),
                                 75.f + offset.x(), 75.f + offset.y(),
                                 blue_paint);

  EXPECT_EQ(0, memcmp(pixels, expected_pixels, 4 * 100 * 100));
}

TEST(DisplayItemListTest, ClipItem) {
  gfx::Rect layer_rect(100, 100);
  SkPictureRecorder recorder;
  skia::RefPtr<SkCanvas> canvas;
  skia::RefPtr<SkPicture> picture;
  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  SkPaint red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};
  scoped_refptr<DisplayItemList> list = DisplayItemList::Create();

  canvas = skia::SharePtr(
      recorder.beginRecording(layer_rect.width(), layer_rect.height()));
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  picture = skia::AdoptRef(recorder.endRecording());
  gfx::PointF first_offset(8.f, 9.f);
  list->AppendItem(DrawingDisplayItem::Create(picture, first_offset));

  gfx::Rect clip_rect(60, 60, 10, 10);
  list->AppendItem(ClipDisplayItem::Create(clip_rect, std::vector<SkRRect>()));

  canvas = skia::SharePtr(
      recorder.beginRecording(layer_rect.width(), layer_rect.height()));
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  picture = skia::AdoptRef(recorder.endRecording());
  gfx::PointF second_offset(2.f, 3.f);
  list->AppendItem(DrawingDisplayItem::Create(picture, second_offset));

  list->AppendItem(EndClipDisplayItem::Create());

  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkCanvas expected_canvas(expected_bitmap);
  expected_canvas.clipRect(gfx::RectToSkRect(layer_rect));
  expected_canvas.drawRectCoords(0.f + first_offset.x(), 0.f + first_offset.y(),
                                 60.f + first_offset.x(),
                                 60.f + first_offset.y(), red_paint);
  expected_canvas.clipRect(gfx::RectToSkRect(clip_rect));
  expected_canvas.drawRectCoords(
      50.f + second_offset.x(), 50.f + second_offset.y(),
      75.f + second_offset.x(), 75.f + second_offset.y(), blue_paint);

  EXPECT_EQ(0, memcmp(pixels, expected_pixels, 4 * 100 * 100));
}

TEST(DisplayItemListTest, TransformItem) {
  gfx::Rect layer_rect(100, 100);
  SkPictureRecorder recorder;
  skia::RefPtr<SkCanvas> canvas;
  skia::RefPtr<SkPicture> picture;
  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  SkPaint red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};
  scoped_refptr<DisplayItemList> list = DisplayItemList::Create();

  canvas = skia::SharePtr(
      recorder.beginRecording(layer_rect.width(), layer_rect.height()));
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  picture = skia::AdoptRef(recorder.endRecording());
  gfx::PointF first_offset(8.f, 9.f);
  list->AppendItem(DrawingDisplayItem::Create(picture, first_offset));

  gfx::Transform transform;
  transform.Rotate(45.0);
  list->AppendItem(TransformDisplayItem::Create(transform));

  canvas = skia::SharePtr(
      recorder.beginRecording(layer_rect.width(), layer_rect.height()));
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  picture = skia::AdoptRef(recorder.endRecording());
  gfx::PointF second_offset(2.f, 3.f);
  list->AppendItem(DrawingDisplayItem::Create(picture, second_offset));

  list->AppendItem(EndTransformDisplayItem::Create());

  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkCanvas expected_canvas(expected_bitmap);
  expected_canvas.clipRect(gfx::RectToSkRect(layer_rect));
  expected_canvas.drawRectCoords(0.f + first_offset.x(), 0.f + first_offset.y(),
                                 60.f + first_offset.x(),
                                 60.f + first_offset.y(), red_paint);
  expected_canvas.setMatrix(transform.matrix());
  expected_canvas.drawRectCoords(
      50.f + second_offset.x(), 50.f + second_offset.y(),
      75.f + second_offset.x(), 75.f + second_offset.y(), blue_paint);

  EXPECT_EQ(0, memcmp(pixels, expected_pixels, 4 * 100 * 100));
}

}  // namespace
}  // namespace cc
