// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/display_item_list.h"

#include <vector>

#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "cc/playback/clip_display_item.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/playback/filter_display_item.h"
#include "cc/playback/transform_display_item.h"
#include "cc/test/skia_common.h"
#include "skia/ext/refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/effects/SkBitmapSource.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

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
  const bool use_cached_picture = true;
  scoped_refptr<DisplayItemList> list =
      DisplayItemList::Create(layer_rect, use_cached_picture);

  gfx::PointF offset(8.f, 9.f);
  gfx::RectF recording_rect(offset, layer_rect.size());
  canvas = skia::SharePtr(
      recorder.beginRecording(gfx::RectFToSkRect(recording_rect)));
  canvas->translate(offset.x(), offset.y());
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  picture = skia::AdoptRef(recorder.endRecordingAsPicture());
  auto* item = list->CreateAndAppendItem<DrawingDisplayItem>();
  item->SetNew(picture);
  list->ProcessAppendedItems();
  list->CreateAndCacheSkPicture();
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
  const bool use_cached_picture = true;
  scoped_refptr<DisplayItemList> list =
      DisplayItemList::Create(layer_rect, use_cached_picture);

  gfx::PointF first_offset(8.f, 9.f);
  gfx::RectF first_recording_rect(first_offset, layer_rect.size());
  canvas = skia::SharePtr(
      recorder.beginRecording(gfx::RectFToSkRect(first_recording_rect)));
  canvas->translate(first_offset.x(), first_offset.y());
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  picture = skia::AdoptRef(recorder.endRecordingAsPicture());
  auto* item1 = list->CreateAndAppendItem<DrawingDisplayItem>();
  item1->SetNew(picture.Pass());

  gfx::Rect clip_rect(60, 60, 10, 10);
  auto* item2 = list->CreateAndAppendItem<ClipDisplayItem>();
  item2->SetNew(clip_rect, std::vector<SkRRect>());

  gfx::PointF second_offset(2.f, 3.f);
  gfx::RectF second_recording_rect(second_offset, layer_rect.size());
  canvas = skia::SharePtr(
      recorder.beginRecording(gfx::RectFToSkRect(second_recording_rect)));
  canvas->translate(second_offset.x(), second_offset.y());
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  picture = skia::AdoptRef(recorder.endRecordingAsPicture());
  auto* item3 = list->CreateAndAppendItem<DrawingDisplayItem>();
  item3->SetNew(picture.Pass());

  list->CreateAndAppendItem<EndClipDisplayItem>();
  list->ProcessAppendedItems();
  list->CreateAndCacheSkPicture();

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
  const bool use_cached_picture = true;
  scoped_refptr<DisplayItemList> list =
      DisplayItemList::Create(layer_rect, use_cached_picture);

  gfx::PointF first_offset(8.f, 9.f);
  gfx::RectF first_recording_rect(first_offset, layer_rect.size());
  canvas = skia::SharePtr(
      recorder.beginRecording(gfx::RectFToSkRect(first_recording_rect)));
  canvas->translate(first_offset.x(), first_offset.y());
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  picture = skia::AdoptRef(recorder.endRecordingAsPicture());
  auto* item1 = list->CreateAndAppendItem<DrawingDisplayItem>();
  item1->SetNew(picture);

  gfx::Transform transform;
  transform.Rotate(45.0);
  auto* item2 = list->CreateAndAppendItem<TransformDisplayItem>();
  item2->SetNew(transform);

  gfx::PointF second_offset(2.f, 3.f);
  gfx::RectF second_recording_rect(second_offset, layer_rect.size());
  canvas = skia::SharePtr(
      recorder.beginRecording(gfx::RectFToSkRect(second_recording_rect)));
  canvas->translate(second_offset.x(), second_offset.y());
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  picture = skia::AdoptRef(recorder.endRecordingAsPicture());
  auto* item3 = list->CreateAndAppendItem<DrawingDisplayItem>();
  item3->SetNew(picture);

  list->CreateAndAppendItem<EndTransformDisplayItem>();
  list->ProcessAppendedItems();
  list->CreateAndCacheSkPicture();

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

TEST(DisplayItemListTest, FilterItem) {
  gfx::Rect layer_rect(100, 100);
  FilterOperations filters;
  unsigned char pixels[4 * 100 * 100] = {0};
  const bool use_cached_picture = true;
  scoped_refptr<DisplayItemList> list =
      DisplayItemList::Create(layer_rect, use_cached_picture);

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
  auto* item = list->CreateAndAppendItem<FilterDisplayItem>();
  item->SetNew(filters, filter_bounds);
  list->CreateAndAppendItem<EndFilterDisplayItem>();
  list->ProcessAppendedItems();
  list->CreateAndCacheSkPicture();

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

TEST(DisplayItemListTest, CompactingItems) {
  gfx::Rect layer_rect(100, 100);
  SkPictureRecorder recorder;
  skia::RefPtr<SkCanvas> canvas;
  skia::RefPtr<SkPicture> picture;
  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  SkPaint red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};

  gfx::PointF offset(8.f, 9.f);
  gfx::RectF recording_rect(offset, layer_rect.size());

  bool use_cached_picture = false;
  scoped_refptr<DisplayItemList> list_without_caching =
      DisplayItemList::Create(layer_rect, use_cached_picture);

  canvas = skia::SharePtr(
      recorder.beginRecording(gfx::RectFToSkRect(recording_rect)));
  canvas->translate(offset.x(), offset.y());
  canvas->drawRectCoords(0.f, 0.f, 60.f, 60.f, red_paint);
  canvas->drawRectCoords(50.f, 50.f, 75.f, 75.f, blue_paint);
  picture = skia::AdoptRef(recorder.endRecordingAsPicture());
  auto* item1 = list_without_caching->CreateAndAppendItem<DrawingDisplayItem>();
  item1->SetNew(picture);
  list_without_caching->ProcessAppendedItems();
  DrawDisplayList(pixels, layer_rect, list_without_caching);

  unsigned char expected_pixels[4 * 100 * 100] = {0};
  use_cached_picture = true;
  scoped_refptr<DisplayItemList> list_with_caching =
      DisplayItemList::Create(layer_rect, use_cached_picture);
  auto* item2 = list_with_caching->CreateAndAppendItem<DrawingDisplayItem>();
  item2->SetNew(picture);
  list_with_caching->ProcessAppendedItems();
  list_with_caching->CreateAndCacheSkPicture();
  DrawDisplayList(expected_pixels, layer_rect, list_with_caching);

  EXPECT_EQ(0, memcmp(pixels, expected_pixels, 4 * 100 * 100));
}

TEST(DisplayItemListTest, PictureMemoryUsage) {
  scoped_refptr<DisplayItemList> list;
  size_t memory_usage;

  // Make an SkPicture whose size is known.
  gfx::Rect layer_rect(100, 100);
  SkPictureRecorder recorder;
  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  SkCanvas* canvas = recorder.beginRecording(gfx::RectFToSkRect(layer_rect));
  for (int i = 0; i < 100; i++)
    canvas->drawPaint(blue_paint);
  skia::RefPtr<SkPicture> picture =
      skia::AdoptRef(recorder.endRecordingAsPicture());
  size_t picture_size = SkPictureUtils::ApproximateBytesUsed(picture.get());
  ASSERT_GE(picture_size, 100 * sizeof(SkPaint));
  ASSERT_LE(picture_size, 200 * sizeof(SkPaint));

  // Using a cached picture, we should get about the right size.
  list = DisplayItemList::Create(layer_rect, true);
  auto* item = list->CreateAndAppendItem<DrawingDisplayItem>();
  item->SetNew(picture);
  list->ProcessAppendedItems();
  list->CreateAndCacheSkPicture();
  memory_usage = list->PictureMemoryUsage();
  EXPECT_GE(memory_usage, picture_size);
  EXPECT_LE(memory_usage, 2 * picture_size);

  // Using no cached picture, we should still get the right size.
  list = DisplayItemList::Create(layer_rect, false);
  item = list->CreateAndAppendItem<DrawingDisplayItem>();
  item->SetNew(picture);
  list->ProcessAppendedItems();
  memory_usage = list->PictureMemoryUsage();
  EXPECT_GE(memory_usage, picture_size);
  EXPECT_LE(memory_usage, 2 * picture_size);

  // To avoid double counting, we expect zero size to be computed if both the
  // picture and items are retained (currently this only happens due to certain
  // categories being traced).
  list = new DisplayItemList(layer_rect, true, true);
  item = list->CreateAndAppendItem<DrawingDisplayItem>();
  item->SetNew(picture);
  list->ProcessAppendedItems();
  list->CreateAndCacheSkPicture();
  memory_usage = list->PictureMemoryUsage();
  EXPECT_EQ(static_cast<size_t>(0), memory_usage);
}

}  // namespace cc
