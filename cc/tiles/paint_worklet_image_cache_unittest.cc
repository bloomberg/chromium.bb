// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "cc/tiles/paint_worklet_image_cache.h"

#include "cc/paint/draw_image.h"
#include "cc/raster/paint_worklet_image_provider.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_paint_worklet_input.h"
#include "cc/test/test_paint_worklet_layer_painter.h"
#include "cc/test/test_tile_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TestPaintWorkletImageCache : public PaintWorkletImageCache {
 public:
  TestPaintWorkletImageCache() {}
};

SkMatrix CreateMatrix(const SkSize& scale, bool is_decomposable) {
  SkMatrix matrix;
  matrix.setScale(scale.width(), scale.height());

  if (!is_decomposable) {
    // Perspective is not decomposable, add it.
    matrix[SkMatrix::kMPersp0] = 0.1f;
  }

  return matrix;
}

PaintImage CreatePaintImage(int width, int height) {
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(width, height));
  return CreatePaintWorkletPaintImage(input);
}

scoped_refptr<TileTask> GetTaskForPaintWorkletImage(
    const PaintImage& paint_image,
    TestPaintWorkletImageCache* cache) {
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      kNone_SkFilterQuality, CreateMatrix(SkSize::Make(1.f, 1.f), true),
      PaintImage::kDefaultFrameIndex);
  std::unique_ptr<TestPaintWorkletLayerPainter> painter =
      std::make_unique<TestPaintWorkletLayerPainter>();
  cache->SetPaintWorkletLayerPainter(std::move(painter));
  return cache->GetTaskForPaintWorkletImage(draw_image);
}

void TestPaintRecord(const PaintRecord* record) {
  EXPECT_EQ(record->total_op_count(), 1u);

  // GetOpAtForTesting check whether the type is the same as DrawImageOp or not.
  // If not, it returns a nullptr.
  auto* paint_op = record->GetOpAtForTesting<DrawImageOp>(0);
  EXPECT_TRUE(paint_op);
}

TEST(PaintWorkletImageCacheTest, GetTaskForImage) {
  TestPaintWorkletImageCache cache;
  PaintImage paint_image = CreatePaintImage(100, 100);
  scoped_refptr<TileTask> task =
      GetTaskForPaintWorkletImage(paint_image, &cache);
  EXPECT_TRUE(task);
  PaintWorkletImageProvider provider(&cache);

  TestTileTaskRunner::ProcessTask(task.get());

  {
    ImageProvider::ScopedResult result =
        provider.GetPaintRecordResult(paint_image.paint_worklet_input());
    EXPECT_TRUE(result.paint_record());
    TestPaintRecord(result.paint_record());

    base::flat_map<PaintWorkletInput*,
                   PaintWorkletImageCache::PaintWorkletImageCacheValue>
        records = cache.GetRecordsForTest();
    // Test the ref count.
    EXPECT_EQ(records[paint_image.paint_worklet_input()].used_ref_count, 1u);
  }
  base::flat_map<PaintWorkletInput*,
                 PaintWorkletImageCache::PaintWorkletImageCacheValue>
      records = cache.GetRecordsForTest();
  // Test the ref count, which should have been decremented when the result
  // goes out of the scope.
  EXPECT_EQ(records[paint_image.paint_worklet_input()].used_ref_count, 0u);

  {
    ImageProvider::ScopedResult result =
        provider.GetPaintRecordResult(paint_image.paint_worklet_input());

    base::flat_map<PaintWorkletInput*,
                   PaintWorkletImageCache::PaintWorkletImageCacheValue>
        records = cache.GetRecordsForTest();
    // Test the ref count.
    EXPECT_EQ(records[paint_image.paint_worklet_input()].used_ref_count, 1u);

    ImageProvider::ScopedResult moved_result = std::move(result);

    EXPECT_FALSE(result);

    EXPECT_TRUE(moved_result.paint_record());
    TestPaintRecord(moved_result.paint_record());

    // Once moved, the ref count from |result| should have been transferred to
    // |moved_result|, so there should be only one un-ref when they both go out
    // of scope.
    EXPECT_EQ(records[paint_image.paint_worklet_input()].used_ref_count, 1u);
  }
  EXPECT_EQ(records[paint_image.paint_worklet_input()].used_ref_count, 0u);
}

TEST(PaintWorkletImageCacheTest, MultipleRecordsInCache) {
  TestPaintWorkletImageCache cache;
  PaintImage paint_image1 = CreatePaintImage(100, 100);
  scoped_refptr<TileTask> task1 =
      GetTaskForPaintWorkletImage(paint_image1, &cache);
  EXPECT_TRUE(task1);
  PaintImage paint_image2 = CreatePaintImage(200, 200);
  scoped_refptr<TileTask> task2 =
      GetTaskForPaintWorkletImage(paint_image2, &cache);
  EXPECT_TRUE(task2);

  TestTileTaskRunner::ProcessTask(task1.get());
  TestTileTaskRunner::ProcessTask(task2.get());

  base::flat_map<PaintWorkletInput*,
                 PaintWorkletImageCache::PaintWorkletImageCacheValue>
      records = cache.GetRecordsForTest();
  EXPECT_EQ(records.size(), 2u);

  PaintRecord* record1 =
      records[paint_image1.paint_worklet_input()].record.get();
  EXPECT_TRUE(record1);
  TestPaintRecord(record1);

  PaintRecord* record2 =
      records[paint_image2.paint_worklet_input()].record.get();
  EXPECT_TRUE(record2);
  TestPaintRecord(record2);
}

}  // namespace
}  // namespace cc
