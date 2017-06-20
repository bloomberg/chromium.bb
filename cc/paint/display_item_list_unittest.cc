// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/display_item_list.h"

#include <stddef.h>

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "cc/base/filter_operation.h"
#include "cc/base/filter_operations.h"
#include "cc/base/render_surface_filters.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_skcanvas.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "third_party/skia/include/effects/SkImageSource.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

bool CompareN32Pixels(void* actual_pixels,
                      void* expected_pixels,
                      int width,
                      int height) {
  if (memcmp(actual_pixels, expected_pixels, 4 * width * height) == 0)
    return true;

  SkImageInfo actual_info = SkImageInfo::MakeN32Premul(width, height);
  SkBitmap actual_bitmap;
  actual_bitmap.installPixels(actual_info, actual_pixels,
                              actual_info.minRowBytes());

  SkImageInfo expected_info = SkImageInfo::MakeN32Premul(width, height);
  SkBitmap expected_bitmap;
  expected_bitmap.installPixels(expected_info, expected_pixels,
                                expected_info.minRowBytes());

  std::string gen_bmp_data_url = GetPNGDataUrl(actual_bitmap);
  std::string ref_bmp_data_url = GetPNGDataUrl(expected_bitmap);

  LOG(ERROR) << "Pixels do not match!";
  LOG(ERROR) << "Actual: " << gen_bmp_data_url;
  LOG(ERROR) << "Expected: " << ref_bmp_data_url;
  return false;
}

}  // namespace

TEST(DisplayItemListTest, SingleUnpairedRange) {
  gfx::Rect layer_rect(100, 100);
  PaintFlags blue_flags;
  blue_flags.setColor(SK_ColorBLUE);
  PaintFlags red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};
  auto list = make_scoped_refptr(new DisplayItemList);

  gfx::Point offset(8, 9);

  PaintOpBuffer* buffer = list->StartPaint();
  buffer->push<SaveOp>();
  buffer->push<TranslateOp>(static_cast<float>(offset.x()),
                            static_cast<float>(offset.y()));
  buffer->push<DrawRectOp>(SkRect::MakeLTRB(0.f, 0.f, 60.f, 60.f), red_paint);
  buffer->push<DrawRectOp>(SkRect::MakeLTRB(50.f, 50.f, 75.f, 75.f),
                           blue_flags);
  buffer->push<RestoreOp>();
  list->EndPaintOfUnpaired(gfx::Rect(offset, layer_rect.size()));
  list->Finalize();
  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkiaPaintCanvas expected_canvas(expected_bitmap);
  expected_canvas.clipRect(gfx::RectToSkRect(layer_rect));
  expected_canvas.drawRect(
      SkRect::MakeLTRB(0.f + offset.x(), 0.f + offset.y(), 60.f + offset.x(),
                       60.f + offset.y()),
      red_paint);
  expected_canvas.drawRect(
      SkRect::MakeLTRB(50.f + offset.x(), 50.f + offset.y(), 75.f + offset.x(),
                       75.f + offset.y()),
      blue_flags);

  EXPECT_TRUE(CompareN32Pixels(pixels, expected_pixels, 100, 100));
}

TEST(DisplayItemListTest, EmptyUnpairedRangeDoesNotAddVisualRect) {
  gfx::Rect layer_rect(100, 100);
  auto list = make_scoped_refptr(new DisplayItemList);

  {
    list->StartPaint();
    list->EndPaintOfUnpaired(layer_rect);
  }
  // No ops.
  EXPECT_EQ(0u, list->op_count());

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<RestoreOp>();
    list->EndPaintOfUnpaired(layer_rect);
  }
  // Two ops.
  EXPECT_EQ(2u, list->op_count());
}

TEST(DisplayItemListTest, ClipPairedRange) {
  gfx::Rect layer_rect(100, 100);
  PaintFlags blue_flags;
  blue_flags.setColor(SK_ColorBLUE);
  PaintFlags red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};
  auto list = make_scoped_refptr(new DisplayItemList);

  gfx::Point first_offset(8, 9);
  gfx::Rect first_recording_rect(first_offset, layer_rect.size());

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<TranslateOp>(static_cast<float>(first_offset.x()),
                              static_cast<float>(first_offset.y()));
    buffer->push<DrawRectOp>(SkRect::MakeWH(60, 60), red_paint);
    buffer->push<RestoreOp>();
    list->EndPaintOfUnpaired(first_recording_rect);
  }

  gfx::Rect clip_rect(60, 60, 10, 10);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ClipRectOp>(gfx::RectToSkRect(clip_rect), SkClipOp::kIntersect,
                             true);
    list->EndPaintOfPairedBegin();
  }

  gfx::Point second_offset(2, 3);
  gfx::Rect second_recording_rect(second_offset, layer_rect.size());
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<TranslateOp>(static_cast<float>(second_offset.x()),
                              static_cast<float>(second_offset.y()));
    buffer->push<DrawRectOp>(SkRect::MakeLTRB(50.f, 50.f, 75.f, 75.f),
                             blue_flags);
    buffer->push<RestoreOp>();
    list->EndPaintOfUnpaired(second_recording_rect);
  }

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  list->Finalize();

  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkiaPaintCanvas expected_canvas(expected_bitmap);
  expected_canvas.clipRect(gfx::RectToSkRect(layer_rect));
  expected_canvas.drawRect(
      SkRect::MakeLTRB(0.f + first_offset.x(), 0.f + first_offset.y(),
                       60.f + first_offset.x(), 60.f + first_offset.y()),
      red_paint);
  expected_canvas.clipRect(gfx::RectToSkRect(clip_rect));
  expected_canvas.drawRect(
      SkRect::MakeLTRB(50.f + second_offset.x(), 50.f + second_offset.y(),
                       75.f + second_offset.x(), 75.f + second_offset.y()),
      blue_flags);

  EXPECT_TRUE(CompareN32Pixels(pixels, expected_pixels, 100, 100));
}

TEST(DisplayItemListTest, TransformPairedRange) {
  gfx::Rect layer_rect(100, 100);
  PaintFlags blue_flags;
  blue_flags.setColor(SK_ColorBLUE);
  PaintFlags red_paint;
  red_paint.setColor(SK_ColorRED);
  unsigned char pixels[4 * 100 * 100] = {0};
  auto list = make_scoped_refptr(new DisplayItemList);

  gfx::Point first_offset(8, 9);
  gfx::Rect first_recording_rect(first_offset, layer_rect.size());
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<TranslateOp>(static_cast<float>(first_offset.x()),
                              static_cast<float>(first_offset.y()));
    buffer->push<DrawRectOp>(SkRect::MakeWH(60, 60), red_paint);
    buffer->push<RestoreOp>();
    list->EndPaintOfUnpaired(first_recording_rect);
  }

  gfx::Transform transform;
  transform.Rotate(45.0);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ConcatOp>(static_cast<SkMatrix>(transform.matrix()));
    list->EndPaintOfPairedBegin();
  }

  gfx::Point second_offset(2, 3);
  gfx::Rect second_recording_rect(second_offset, layer_rect.size());
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<TranslateOp>(static_cast<float>(second_offset.x()),
                              static_cast<float>(second_offset.y()));
    buffer->push<DrawRectOp>(SkRect::MakeLTRB(50.f, 50.f, 75.f, 75.f),
                             blue_flags);
    buffer->push<RestoreOp>();
    list->EndPaintOfUnpaired(second_recording_rect);
  }

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  list->Finalize();

  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkiaPaintCanvas expected_canvas(expected_bitmap);
  expected_canvas.clipRect(gfx::RectToSkRect(layer_rect));
  expected_canvas.drawRect(
      SkRect::MakeLTRB(0.f + first_offset.x(), 0.f + first_offset.y(),
                       60.f + first_offset.x(), 60.f + first_offset.y()),
      red_paint);
  expected_canvas.setMatrix(transform.matrix());
  expected_canvas.drawRect(
      SkRect::MakeLTRB(50.f + second_offset.x(), 50.f + second_offset.y(),
                       75.f + second_offset.x(), 75.f + second_offset.y()),
      blue_flags);

  EXPECT_TRUE(CompareN32Pixels(pixels, expected_pixels, 100, 100));
}

TEST(DisplayItemListTest, FilterPairedRange) {
  gfx::Rect layer_rect(100, 100);
  FilterOperations filters;
  unsigned char pixels[4 * 100 * 100] = {0};
  auto list = make_scoped_refptr(new DisplayItemList);

  sk_sp<SkSurface> source_surface = SkSurface::MakeRasterN32Premul(50, 50);
  SkCanvas* source_canvas = source_surface->getCanvas();
  source_canvas->clear(SkColorSetRGB(128, 128, 128));
  sk_sp<SkImage> source_image = source_surface->makeImageSnapshot();

  // For most SkImageFilters, the |dst| bounds computed by computeFastBounds are
  // dependent on the provided |src| bounds. This means, for example, that
  // translating |src| results in a corresponding translation of |dst|. But this
  // is not the case for all SkImageFilters; for some of them (e.g.
  // SkImageSource), the computation of |dst| in computeFastBounds doesn't
  // involve |src| at all. Incorrectly assuming such a relationship (e.g. by
  // translating |dst| after it is computed by computeFastBounds, rather than
  // translating |src| before it provided to computedFastBounds) can cause
  // incorrect clipping of filter output. To test for this, we include an
  // SkImageSource filter in |filters|. Here, |src| is |filter_bounds|, defined
  // below.
  sk_sp<SkImageFilter> image_filter = SkImageSource::Make(source_image);
  filters.Append(FilterOperation::CreateReferenceFilter(image_filter));
  filters.Append(FilterOperation::CreateBrightnessFilter(0.5f));
  gfx::RectF filter_bounds(10.f, 10.f, 50.f, 50.f);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<TranslateOp>(filter_bounds.x(), filter_bounds.y());

    PaintFlags flags;
    flags.setImageFilter(
        RenderSurfaceFilters::BuildImageFilter(filters, filter_bounds.size()));

    SkRect layer_bounds = gfx::RectFToSkRect(filter_bounds);
    layer_bounds.offset(-filter_bounds.x(), -filter_bounds.y());
    buffer->push<SaveLayerOp>(&layer_bounds, &flags);
    buffer->push<TranslateOp>(-filter_bounds.x(), -filter_bounds.y());

    list->EndPaintOfPairedBegin();
  }

  // Include a rect drawing so that filter is actually applied to something.
  {
    PaintOpBuffer* buffer = list->StartPaint();

    PaintFlags red_flags;
    red_flags.setColor(SK_ColorRED);

    buffer->push<DrawRectOp>(
        SkRect::MakeLTRB(filter_bounds.x(), filter_bounds.y(),
                         filter_bounds.right(), filter_bounds.bottom()),
        red_flags);

    list->EndPaintOfUnpaired(ToEnclosingRect(filter_bounds));
  }

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();  // For SaveLayerOp.
    buffer->push<RestoreOp>();  // For SaveOp.
    list->EndPaintOfPairedEnd();
  }

  list->Finalize();

  DrawDisplayList(pixels, layer_rect, list);

  SkBitmap expected_bitmap;
  unsigned char expected_pixels[4 * 100 * 100] = {0};
  PaintFlags paint;
  paint.setColor(SkColorSetRGB(64, 64, 64));
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  expected_bitmap.installPixels(info, expected_pixels, info.minRowBytes());
  SkiaPaintCanvas expected_canvas(expected_bitmap);
  expected_canvas.drawRect(RectFToSkRect(filter_bounds), paint);

  EXPECT_TRUE(CompareN32Pixels(pixels, expected_pixels, 100, 100));
}

TEST(DisplayItemListTest, BytesUsed) {
  const int kNumPaintOps = 1000;
  size_t memory_usage;

  auto list = make_scoped_refptr(new DisplayItemList);

  gfx::Rect layer_rect(100, 100);
  PaintFlags blue_flags;
  blue_flags.setColor(SK_ColorBLUE);

  {
    PaintOpBuffer* buffer = list->StartPaint();
    for (int i = 0; i < kNumPaintOps; i++)
      buffer->push<DrawRectOp>(SkRect::MakeWH(1, 1), blue_flags);
    list->EndPaintOfUnpaired(layer_rect);
  }

  memory_usage = list->BytesUsed();
  EXPECT_GE(memory_usage, sizeof(DrawRectOp) * kNumPaintOps);
  EXPECT_LE(memory_usage, 2 * sizeof(DrawRectOp) * kNumPaintOps);
}

TEST(DisplayItemListTest, AsValueWithNoOps) {
  auto list = make_scoped_refptr(new DisplayItemList);
  list->Finalize();

  // Pass |true| to ask for PaintOps even though there are none.
  std::unique_ptr<base::Value> root =
      list->CreateTracedValue(true)->ToBaseValue();
  const base::DictionaryValue* root_dict;
  ASSERT_TRUE(root->GetAsDictionary(&root_dict));
  // The traced value has a params dictionary as its root.
  {
    const base::DictionaryValue* params_dict;
    ASSERT_TRUE(root_dict->GetDictionary("params", &params_dict));

    // The real contents of the traced value is in here.
    {
      const base::ListValue* list;
      double d;

      // The layer_rect field is present by empty.
      ASSERT_TRUE(params_dict->GetList("layer_rect", &list));
      ASSERT_EQ(4u, list->GetSize());
      EXPECT_TRUE(list->GetDouble(0, &d) && d == 0) << d;
      EXPECT_TRUE(list->GetDouble(1, &d) && d == 0) << d;
      EXPECT_TRUE(list->GetDouble(2, &d) && d == 0) << d;
      EXPECT_TRUE(list->GetDouble(3, &d) && d == 0) << d;

      // The items list is there but empty.
      ASSERT_TRUE(params_dict->GetList("items", &list));
      EXPECT_EQ(0u, list->GetSize());
    }
  }

  // Pass |false| to not include PaintOps.
  root = list->CreateTracedValue(false)->ToBaseValue();
  ASSERT_TRUE(root->GetAsDictionary(&root_dict));
  // The traced value has a params dictionary as its root.
  {
    const base::DictionaryValue* params_dict;
    ASSERT_TRUE(root_dict->GetDictionary("params", &params_dict));

    // The real contents of the traced value is in here.
    {
      const base::ListValue* list;
      double d;

      // The layer_rect field is present by empty.
      ASSERT_TRUE(params_dict->GetList("layer_rect", &list));
      ASSERT_EQ(4u, list->GetSize());
      EXPECT_TRUE(list->GetDouble(0, &d) && d == 0) << d;
      EXPECT_TRUE(list->GetDouble(1, &d) && d == 0) << d;
      EXPECT_TRUE(list->GetDouble(2, &d) && d == 0) << d;
      EXPECT_TRUE(list->GetDouble(3, &d) && d == 0) << d;

      // The items list is not there since we asked for no ops.
      ASSERT_FALSE(params_dict->GetList("items", &list));
    }
  }
}

TEST(DisplayItemListTest, AsValueWithOps) {
  gfx::Rect layer_rect = gfx::Rect(1, 2, 8, 9);
  auto list = make_scoped_refptr(new DisplayItemList);
  gfx::Transform transform;
  transform.Translate(6.f, 7.f);

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ConcatOp>(static_cast<SkMatrix>(transform.matrix()));
    list->EndPaintOfPairedBegin();
  }

  gfx::Point offset(2, 3);
  gfx::Rect bounds(offset, layer_rect.size());
  {
    PaintOpBuffer* buffer = list->StartPaint();

    PaintFlags red_paint;
    red_paint.setColor(SK_ColorRED);

    buffer->push<SaveOp>();
    buffer->push<TranslateOp>(static_cast<float>(offset.x()),
                              static_cast<float>(offset.y()));
    buffer->push<DrawRectOp>(SkRect::MakeWH(4, 4), red_paint);

    list->EndPaintOfUnpaired(bounds);
  }

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  list->Finalize();

  // Pass |true| to ask for PaintOps to be included.
  std::unique_ptr<base::Value> root =
      list->CreateTracedValue(true)->ToBaseValue();
  const base::DictionaryValue* root_dict;
  ASSERT_TRUE(root->GetAsDictionary(&root_dict));
  // The traced value has a params dictionary as its root.
  {
    const base::DictionaryValue* params_dict;
    ASSERT_TRUE(root_dict->GetDictionary("params", &params_dict));

    // The real contents of the traced value is in here.
    {
      const base::ListValue* list;
      double d;

      // The layer_rect field is present and has the bounds of the rtree.
      ASSERT_TRUE(params_dict->GetList("layer_rect", &list));
      ASSERT_EQ(4u, list->GetSize());
      EXPECT_TRUE(list->GetDouble(0, &d) && d == 2) << d;
      EXPECT_TRUE(list->GetDouble(1, &d) && d == 3) << d;
      EXPECT_TRUE(list->GetDouble(2, &d) && d == 8) << d;
      EXPECT_TRUE(list->GetDouble(3, &d) && d == 9) << d;

      // The items list has 3 things in it since we built 3 visual rects.
      ASSERT_TRUE(params_dict->GetList("items", &list));
      EXPECT_EQ(6u, list->GetSize());

      for (int i = 0; i < 6; ++i) {
        const base::DictionaryValue* item_dict;

        ASSERT_TRUE(list->GetDictionary(i, &item_dict));

        // The SkPicture for each item exists.
        EXPECT_TRUE(
            item_dict->GetString("skp64", static_cast<std::string*>(nullptr)));
      }
    }
  }

  // Pass |false| to not include PaintOps.
  root = list->CreateTracedValue(false)->ToBaseValue();
  ASSERT_TRUE(root->GetAsDictionary(&root_dict));
  // The traced value has a params dictionary as its root.
  {
    const base::DictionaryValue* params_dict;
    ASSERT_TRUE(root_dict->GetDictionary("params", &params_dict));

    // The real contents of the traced value is in here.
    {
      const base::ListValue* list;
      double d;

      // The layer_rect field is present and has the bounds of the rtree.
      ASSERT_TRUE(params_dict->GetList("layer_rect", &list));
      ASSERT_EQ(4u, list->GetSize());
      EXPECT_TRUE(list->GetDouble(0, &d) && d == 2) << d;
      EXPECT_TRUE(list->GetDouble(1, &d) && d == 3) << d;
      EXPECT_TRUE(list->GetDouble(2, &d) && d == 8) << d;
      EXPECT_TRUE(list->GetDouble(3, &d) && d == 9) << d;

      // The items list is not present since we asked for no ops.
      ASSERT_FALSE(params_dict->GetList("items", &list));
    }
  }
}

TEST(DisplayItemListTest, SizeEmpty) {
  auto list = make_scoped_refptr(new DisplayItemList);
  EXPECT_EQ(0u, list->op_count());
}

TEST(DisplayItemListTest, SizeOne) {
  auto list = make_scoped_refptr(new DisplayItemList);
  gfx::Rect drawing_bounds(5, 6, 1, 1);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_bounds);
  }
  EXPECT_EQ(1u, list->op_count());
}

TEST(DisplayItemListTest, SizeMultiple) {
  auto list = make_scoped_refptr(new DisplayItemList);
  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds),
                             SkClipOp::kIntersect, false);
    list->EndPaintOfPairedBegin();
  }
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  EXPECT_EQ(3u, list->op_count());
}

TEST(DisplayItemListTest, AppendVisualRectSimple) {
  auto list = make_scoped_refptr(new DisplayItemList);

  // One drawing: D.

  gfx::Rect drawing_bounds(5, 6, 7, 8);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_bounds);
  }

  EXPECT_EQ(1u, list->op_count());
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(0));
}

TEST(DisplayItemListTest, AppendVisualRectEmptyBlock) {
  auto list = make_scoped_refptr(new DisplayItemList);

  // One block: B1, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds),
                             SkClipOp::kIntersect, false);
    list->EndPaintOfPairedBegin();
  }
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(3u, list->op_count());
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(2));
}

TEST(DisplayItemListTest, AppendVisualRectEmptyBlockContainingEmptyBlock) {
  auto list = make_scoped_refptr(new DisplayItemList);

  // Two nested blocks: B1, B2, E2, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds),
                             SkClipOp::kIntersect, false);
    list->EndPaintOfPairedBegin();
  }

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    list->EndPaintOfPairedBegin();
  }
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(5u, list->op_count());
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(3));
  EXPECT_RECT_EQ(gfx::Rect(), list->VisualRectForTesting(4));
}

TEST(DisplayItemListTest, AppendVisualRectBlockContainingDrawing) {
  auto list = make_scoped_refptr(new DisplayItemList);

  // One block with one drawing: B1, Da, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds),
                             SkClipOp::kIntersect, false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_bounds(5, 6, 1, 1);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_bounds);
  }

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(4u, list->op_count());
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(3));
}

TEST(DisplayItemListTest, AppendVisualRectBlockContainingEscapedDrawing) {
  auto list = make_scoped_refptr(new DisplayItemList);

  // One block with one drawing: B1, Da (escapes), E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds),
                             SkClipOp::kIntersect, false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_bounds(1, 2, 3, 4);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_bounds);
  }

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(4u, list->op_count());
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(drawing_bounds, list->VisualRectForTesting(3));
}

TEST(DisplayItemListTest,
     AppendVisualRectDrawingFollowedByBlockContainingEscapedDrawing) {
  auto list = make_scoped_refptr(new DisplayItemList);

  // One drawing followed by one block with one drawing: Da, B1, Db (escapes),
  // E1.

  gfx::Rect drawing_a_bounds(1, 2, 3, 4);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_a_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_a_bounds);
  }

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds),
                             SkClipOp::kIntersect, false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_b_bounds(13, 14, 1, 1);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_b_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_b_bounds);
  }

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(5u, list->op_count());
  EXPECT_RECT_EQ(drawing_a_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
}

TEST(DisplayItemListTest, AppendVisualRectTwoBlocksTwoDrawings) {
  auto list = make_scoped_refptr(new DisplayItemList);

  // Multiple nested blocks with drawings amidst: B1, Da, B2, Db, E2, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds),
                             SkClipOp::kIntersect, false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_a_bounds(5, 6, 1, 1);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_a_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_a_bounds);
  }

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ConcatOp>(SkMatrix::I());
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_b_bounds(7, 8, 1, 1);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_b_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_b_bounds);
  }

  // End transform.
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  // End clip.
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(8u, list->op_count());
  gfx::Rect merged_drawing_bounds = gfx::Rect(drawing_a_bounds);
  merged_drawing_bounds.Union(drawing_b_bounds);
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_a_bounds, list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(5));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(6));
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(7));
}

TEST(DisplayItemListTest,
     AppendVisualRectTwoBlocksTwoDrawingsInnerDrawingEscaped) {
  auto list = make_scoped_refptr(new DisplayItemList);

  // Multiple nested blocks with drawings amidst: B1, Da, B2, Db (escapes), E2,
  // E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds),
                             SkClipOp::kIntersect, false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_a_bounds(5, 6, 1, 1);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_a_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_a_bounds);
  }

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ConcatOp>(SkMatrix::I());
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_b_bounds(1, 2, 3, 4);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_b_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_b_bounds);
  }

  // End transform.
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  // End clip.
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(8u, list->op_count());
  gfx::Rect merged_drawing_bounds = gfx::Rect(drawing_a_bounds);
  merged_drawing_bounds.Union(drawing_b_bounds);
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_a_bounds, list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(5));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(6));
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(7));
}

TEST(DisplayItemListTest,
     AppendVisualRectTwoBlocksTwoDrawingsOuterDrawingEscaped) {
  auto list = make_scoped_refptr(new DisplayItemList);

  // Multiple nested blocks with drawings amidst: B1, Da (escapes), B2, Db, E2,
  // E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds),
                             SkClipOp::kIntersect, false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_a_bounds(1, 2, 3, 4);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_a_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_a_bounds);
  }

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ConcatOp>(SkMatrix::I());
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_b_bounds(7, 8, 1, 1);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_b_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_b_bounds);
  }

  // End transform.
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  // End clip.
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(8u, list->op_count());
  gfx::Rect merged_drawing_bounds = gfx::Rect(drawing_a_bounds);
  merged_drawing_bounds.Union(drawing_b_bounds);
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_a_bounds, list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(5));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(6));
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(7));
}

TEST(DisplayItemListTest,
     AppendVisualRectTwoBlocksTwoDrawingsBothDrawingsEscaped) {
  auto list = make_scoped_refptr(new DisplayItemList);

  // Multiple nested blocks with drawings amidst:
  // B1, Da (escapes to the right), B2, Db (escapes to the left), E2, E1.

  gfx::Rect clip_bounds(5, 6, 7, 8);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ClipRectOp>(gfx::RectToSkRect(clip_bounds),
                             SkClipOp::kIntersect, false);
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_a_bounds(13, 14, 1, 1);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_a_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_a_bounds);
  }

  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<SaveOp>();
    buffer->push<ConcatOp>(SkMatrix::I());
    list->EndPaintOfPairedBegin();
  }

  gfx::Rect drawing_b_bounds(1, 2, 3, 4);
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<DrawRectOp>(gfx::RectToSkRect(drawing_b_bounds), PaintFlags());
    list->EndPaintOfUnpaired(drawing_b_bounds);
  }

  // End transform.
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }
  // End clip.
  {
    PaintOpBuffer* buffer = list->StartPaint();
    buffer->push<RestoreOp>();
    list->EndPaintOfPairedEnd();
  }

  EXPECT_EQ(8u, list->op_count());
  gfx::Rect merged_drawing_bounds = gfx::Rect(drawing_a_bounds);
  merged_drawing_bounds.Union(drawing_b_bounds);
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(0));
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(1));
  EXPECT_RECT_EQ(drawing_a_bounds, list->VisualRectForTesting(2));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(3));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(4));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(5));
  EXPECT_RECT_EQ(drawing_b_bounds, list->VisualRectForTesting(6));
  EXPECT_RECT_EQ(merged_drawing_bounds, list->VisualRectForTesting(7));
}

}  // namespace cc
