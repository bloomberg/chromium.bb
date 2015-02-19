// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/display_item_list.h"

#include <vector>

#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "cc/resources/clip_display_item.h"
#include "cc/resources/drawing_display_item.h"
#include "cc/resources/filter_display_item.h"
#include "cc/resources/transform_display_item.h"
#include "cc/test/skia_common.h"
#include "skia/ext/refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/effects/SkBitmapSource.h"
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

  gfx::PointF offset(8.f, 9.f);
  gfx::RectF recording_rect(offset, layer_rect.size());
  canvas = skia::SharePtr(
      recorder.beginRecording(gfx::RectFToSkRect(recording_rect)));
  canvas->translate(offset.x(), offset.y());
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  picture = skia::AdoptRef(recorder.endRecording());
  list->AppendItem(DrawingDisplayItem::Create(picture));
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

  gfx::PointF first_offset(8.f, 9.f);
  gfx::RectF first_recording_rect(first_offset, layer_rect.size());
  canvas = skia::SharePtr(
      recorder.beginRecording(gfx::RectFToSkRect(first_recording_rect)));
  canvas->translate(first_offset.x(), first_offset.y());
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  picture = skia::AdoptRef(recorder.endRecording());
  list->AppendItem(DrawingDisplayItem::Create(picture));

  gfx::Rect clip_rect(60, 60, 10, 10);
  list->AppendItem(ClipDisplayItem::Create(clip_rect, std::vector<SkRRect>()));

  gfx::PointF second_offset(2.f, 3.f);
  gfx::RectF second_recording_rect(second_offset, layer_rect.size());
  canvas = skia::SharePtr(
      recorder.beginRecording(gfx::RectFToSkRect(second_recording_rect)));
  canvas->translate(second_offset.x(), second_offset.y());
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  picture = skia::AdoptRef(recorder.endRecording());
  list->AppendItem(DrawingDisplayItem::Create(picture));

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

  gfx::PointF first_offset(8.f, 9.f);
  gfx::RectF first_recording_rect(first_offset, layer_rect.size());
  canvas = skia::SharePtr(
      recorder.beginRecording(gfx::RectFToSkRect(first_recording_rect)));
  canvas->translate(first_offset.x(), first_offset.y());
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  picture = skia::AdoptRef(recorder.endRecording());
  list->AppendItem(DrawingDisplayItem::Create(picture));

  gfx::Transform transform;
  transform.Rotate(45.0);
  list->AppendItem(TransformDisplayItem::Create(transform));

  gfx::PointF second_offset(2.f, 3.f);
  gfx::RectF second_recording_rect(second_offset, layer_rect.size());
  canvas = skia::SharePtr(
      recorder.beginRecording(gfx::RectFToSkRect(second_recording_rect)));
  canvas->translate(second_offset.x(), second_offset.y());
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  picture = skia::AdoptRef(recorder.endRecording());
  list->AppendItem(DrawingDisplayItem::Create(picture));

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

TEST(DisplayItemList, FilterItem) {
  gfx::Rect layer_rect(100, 100);
  FilterOperations filters;
  unsigned char pixels[4 * 100 * 100] = {0};
  scoped_refptr<DisplayItemList> list = DisplayItemList::Create();

  SkBitmap source_bitmap;
  source_bitmap.allocN32Pixels(50, 50);
  SkCanvas source_canvas(source_bitmap);
  source_canvas.clear(SkColorSetRGB(128, 128, 128));

  // For most SkImageFilters, the |dst| bounds computed by computeFastBounds are
  // dependent on the provided |src| bounds. This means, for example, that
  // translating |src| results in a corresponding translation of |dst|. But this
  // is not the case for all SkImageFilters; for some of them (e.g.
  // SkBitmapSource), the computation of |dst| in computeFastBounds doesn't
  // involve |src| at all. Incorrectly assuming such a relationship (e.g. by
  // translating |dst| after it is computed by computeFastBounds, rather than
  // translating |src| before it provided to computedFastBounds) can cause
  // incorrect clipping of filter output. To test for this, we include an
  // SkBitmapSource filter in |filters|. Here, |src| is |filter_bounds|, defined
  // below.
  skia::RefPtr<SkImageFilter> image_filter =
      skia::AdoptRef(SkBitmapSource::Create(source_bitmap));
  filters.Append(FilterOperation::CreateReferenceFilter(image_filter));
  filters.Append(FilterOperation::CreateBrightnessFilter(0.5f));
  gfx::RectF filter_bounds(10.f, 10.f, 50.f, 50.f);
  list->AppendItem(FilterDisplayItem::Create(filters, filter_bounds));
  list->AppendItem(EndFilterDisplayItem::Create());

  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkPaint paint;
  paint.setColor(SkColorSetRGB(64, 64, 64));
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkCanvas expected_canvas(expected_bitmap);
  expected_canvas.drawRect(RectFToSkRect(filter_bounds), paint);

  EXPECT_EQ(0, memcmp(pixels, expected_pixels, 4 * 100 * 100));
}

}  // namespace
}  // namespace cc
