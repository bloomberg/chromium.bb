// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/paint_worklet_image_cache.h"

#include "cc/paint/draw_image.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_paint_worklet_input.h"
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

TEST(PaintWorkletImageCacheTest, GetTaskForImage) {
  TestPaintWorkletImageCache cache;
  PaintImage paint_image = CreatePaintImage(100, 100);
  DrawImage draw_image(
      paint_image, SkIRect::MakeWH(paint_image.width(), paint_image.height()),
      kNone_SkFilterQuality, CreateMatrix(SkSize::Make(1.f, 1.f), true),
      PaintImage::kDefaultFrameIndex);
  scoped_refptr<TileTask> task = cache.GetTaskForPaintWorkletImage(draw_image);
  EXPECT_TRUE(task);

  TestTileTaskRunner::ProcessTask(task.get());
}

}  // namespace
}  // namespace cc
