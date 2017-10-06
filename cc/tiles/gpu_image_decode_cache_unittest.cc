// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/gpu_image_decode_cache.h"

#include "cc/paint/draw_image.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/test/fake_paint_image_generator.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_tile_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
namespace {

gfx::ColorSpace DefaultColorSpace() {
  return gfx::ColorSpace::CreateSRGB();
}

size_t kGpuMemoryLimitBytes = 96 * 1024 * 1024;
class TestGpuImageDecodeCache : public GpuImageDecodeCache {
 public:
  explicit TestGpuImageDecodeCache(viz::ContextProvider* context,
                                   SkColorType color_type)
      : GpuImageDecodeCache(context,
                            color_type,
                            kGpuMemoryLimitBytes,
                            kGpuMemoryLimitBytes) {}
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

using GpuImageDecodeCacheTest = ::testing::TestWithParam<SkColorType>;

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageSameImage) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  DrawImage another_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()), quality,
      CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult another_result = cache.GetTaskForImageAndRef(
      another_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(result.task.get() == another_result.task.get());

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  cache.UnrefImage(draw_image);
  cache.UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageSmallerScale) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  DrawImage another_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()), quality,
      CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult another_result = cache.GetTaskForImageAndRef(
      another_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(result.task.get() == another_result.task.get());

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  cache.UnrefImage(draw_image);
  cache.UnrefImage(another_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageLowerQuality) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  bool is_decomposable = true;
  SkMatrix matrix = CreateMatrix(SkSize::Make(0.4f, 0.4f), is_decomposable);

  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       kHigh_SkFilterQuality, matrix,
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  DrawImage another_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()),
      kLow_SkFilterQuality, matrix, PaintImage::kDefaultFrameIndex,
      DefaultColorSpace());
  ImageDecodeCache::TaskResult another_result = cache.GetTaskForImageAndRef(
      another_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(result.task.get() == another_result.task.get());

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  cache.UnrefImage(draw_image);
  cache.UnrefImage(another_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageDifferentImage) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage first_image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache.GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  PaintImage second_image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage second_draw_image(
      second_image,
      SkIRect::MakeWH(second_image.width(), second_image.height()), quality,
      CreateMatrix(SkSize::Make(0.25f, 0.25f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache.GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());
  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache.UnrefImage(first_draw_image);
  cache.UnrefImage(second_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageLargerScale) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage first_image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache.GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());

  cache.UnrefImage(first_draw_image);

  DrawImage second_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache.GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  DrawImage third_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult third_result = cache.GetTaskForImageAndRef(
      third_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task.get() == second_result.task.get());

  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache.UnrefImage(second_draw_image);
  cache.UnrefImage(third_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageLargerScaleNoReuse) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage first_image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache.GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  DrawImage second_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache.GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  DrawImage third_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult third_result = cache.GetTaskForImageAndRef(
      third_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task.get() == first_result.task.get());

  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());
  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache.UnrefImage(first_draw_image);
  cache.UnrefImage(second_draw_image);
  cache.UnrefImage(third_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageHigherQuality) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkMatrix matrix = CreateMatrix(SkSize::Make(0.4f, 0.4f), is_decomposable);

  PaintImage first_image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      kLow_SkFilterQuality, matrix, PaintImage::kDefaultFrameIndex,
      DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache.GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());

  cache.UnrefImage(first_draw_image);

  DrawImage second_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      kHigh_SkFilterQuality, matrix, PaintImage::kDefaultFrameIndex,
      DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache.GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache.UnrefImage(second_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageAlreadyDecodedAndLocked) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  EXPECT_EQ(result.task->dependencies().size(), 1u);
  EXPECT_TRUE(result.task->dependencies()[0]);

  // Run the decode but don't complete it (this will keep the decode locked).
  TestTileTaskRunner::ScheduleTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::RunTask(result.task->dependencies()[0].get());

  // Cancel the upload.
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  // Get the image again - we should have an upload task, but no dependent
  // decode task, as the decode was already locked.
  ImageDecodeCache::TaskResult another_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(another_result.task);
  EXPECT_EQ(another_result.task->dependencies().size(), 0u);

  TestTileTaskRunner::ProcessTask(another_result.task.get());

  // Finally, complete the original decode task.
  TestTileTaskRunner::CompleteTask(result.task->dependencies()[0].get());

  cache.UnrefImage(draw_image);
  cache.UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageAlreadyDecodedNotLocked) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  EXPECT_EQ(result.task->dependencies().size(), 1u);
  EXPECT_TRUE(result.task->dependencies()[0]);

  // Run the decode.
  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());

  // Cancel the upload.
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  // Unref the image.
  cache.UnrefImage(draw_image);

  // Get the image again - we should have an upload task and a dependent decode
  // task - this dependent task will typically just re-lock the image.
  ImageDecodeCache::TaskResult another_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(another_result.task);
  EXPECT_EQ(another_result.task->dependencies().size(), 1u);
  EXPECT_TRUE(result.task->dependencies()[0]);

  TestTileTaskRunner::ProcessTask(another_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(another_result.task.get());

  cache.UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageAlreadyUploaded) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  EXPECT_EQ(result.task->dependencies().size(), 1u);
  EXPECT_TRUE(result.task->dependencies()[0]);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ScheduleTask(result.task.get());
  TestTileTaskRunner::RunTask(result.task.get());

  ImageDecodeCache::TaskResult another_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_FALSE(another_result.task);

  TestTileTaskRunner::CompleteTask(result.task.get());

  cache.UnrefImage(draw_image);
  cache.UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageCanceledGetsNewTask) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());

  ImageDecodeCache::TaskResult another_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(another_result.task.get() == result.task.get());

  // Didn't run the task, so cancel it.
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  // Fully cancel everything (so the raster would unref things).
  cache.UnrefImage(draw_image);
  cache.UnrefImage(draw_image);

  // Here a new task is created.
  ImageDecodeCache::TaskResult third_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task);
  EXPECT_FALSE(third_result.task.get() == result.task.get());

  TestTileTaskRunner::ProcessTask(third_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(third_result.task.get());

  cache.UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageCanceledWhileReffedGetsNewTask) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  ASSERT_GT(result.task->dependencies().size(), 0u);
  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());

  ImageDecodeCache::TaskResult another_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_TRUE(another_result.task.get() == result.task.get());

  // Didn't run the task, so cancel it.
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  // 2 Unrefs, so that the decode is unlocked as well.
  cache.UnrefImage(draw_image);
  cache.UnrefImage(draw_image);

  // Note that here, everything is reffed, but a new task is created. This is
  // possible with repeated schedule/cancel operations.
  ImageDecodeCache::TaskResult third_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task);
  EXPECT_FALSE(third_result.task.get() == result.task.get());

  ASSERT_GT(third_result.task->dependencies().size(), 0u);
  TestTileTaskRunner::ProcessTask(third_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(third_result.task.get());

  // Unref!
  cache.UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, NoTaskForImageAlreadyFailedDecoding) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  // Didn't run the task, so cancel it.
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  cache.SetImageDecodingFailedForTesting(draw_image);

  ImageDecodeCache::TaskResult another_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(another_result.need_unref);
  EXPECT_EQ(another_result.task.get(), nullptr);

  cache.UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetDecodedImageForDraw) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_draw_image.filter_quality(), kMedium_SkFilterQuality);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(decoded_draw_image.is_at_raster_decode());
  EXPECT_FALSE(cache.DiscardableIsLockedForTesting(draw_image));

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetLargeDecodedImageForDraw) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(1, 24000));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_draw_image.filter_quality(), kMedium_SkFilterQuality);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_FALSE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(decoded_draw_image.is_at_raster_decode());
  EXPECT_TRUE(cache.DiscardableIsLockedForTesting(draw_image));

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
  EXPECT_FALSE(cache.DiscardableIsLockedForTesting(draw_image));
}

TEST_P(GpuImageDecodeCacheTest, GetDecodedImageForDrawAtRasterDecode) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  cache.SetAllByteLimitsForTesting(0);

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.need_unref);
  EXPECT_FALSE(result.task);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_draw_image.filter_quality(), kMedium_SkFilterQuality);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_TRUE(decoded_draw_image.is_at_raster_decode());
  EXPECT_FALSE(cache.DiscardableIsLockedForTesting(draw_image));

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetDecodedImageForDrawLargerScale) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       kLow_SkFilterQuality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  DrawImage larger_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()), quality,
      CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult larger_result = cache.GetTaskForImageAndRef(
      larger_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(larger_result.need_unref);
  EXPECT_TRUE(larger_result.task);

  TestTileTaskRunner::ProcessTask(larger_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(larger_result.task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  // |draw_image| had a low filter quality, so expect that to be respected.
  EXPECT_EQ(decoded_draw_image.filter_quality(), kLow_SkFilterQuality);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(decoded_draw_image.is_at_raster_decode());
  EXPECT_FALSE(cache.DiscardableIsLockedForTesting(draw_image));

  DecodedDrawImage larger_decoded_draw_image =
      cache.GetDecodedImageForDraw(larger_draw_image);
  EXPECT_EQ(larger_decoded_draw_image.filter_quality(),
            kMedium_SkFilterQuality);
  EXPECT_TRUE(larger_decoded_draw_image.image());
  EXPECT_TRUE(larger_decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(larger_decoded_draw_image.is_at_raster_decode());
  EXPECT_FALSE(cache.DiscardableIsLockedForTesting(draw_image));

  EXPECT_FALSE(decoded_draw_image.image() == larger_decoded_draw_image.image());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
  cache.DrawWithImageFinished(larger_draw_image, larger_decoded_draw_image);
  cache.UnrefImage(larger_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetDecodedImageForDrawHigherQuality) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkMatrix matrix = CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable);

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       kLow_SkFilterQuality, matrix,
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  DrawImage higher_quality_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()),
      kHigh_SkFilterQuality, matrix, PaintImage::kDefaultFrameIndex,
      DefaultColorSpace());
  ImageDecodeCache::TaskResult hq_result = cache.GetTaskForImageAndRef(
      higher_quality_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(hq_result.need_unref);
  EXPECT_TRUE(hq_result.task);

  TestTileTaskRunner::ProcessTask(hq_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(hq_result.task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_draw_image.filter_quality(), kLow_SkFilterQuality);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(decoded_draw_image.is_at_raster_decode());
  EXPECT_FALSE(cache.DiscardableIsLockedForTesting(draw_image));

  DecodedDrawImage larger_decoded_draw_image =
      cache.GetDecodedImageForDraw(higher_quality_draw_image);
  EXPECT_EQ(larger_decoded_draw_image.filter_quality(),
            kMedium_SkFilterQuality);
  EXPECT_TRUE(larger_decoded_draw_image.image());
  EXPECT_TRUE(larger_decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(larger_decoded_draw_image.is_at_raster_decode());
  EXPECT_FALSE(cache.DiscardableIsLockedForTesting(draw_image));

  EXPECT_FALSE(decoded_draw_image.image() == larger_decoded_draw_image.image());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
  cache.DrawWithImageFinished(higher_quality_draw_image,
                              larger_decoded_draw_image);
  cache.UnrefImage(higher_quality_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetDecodedImageForDrawNegative) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(-0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_draw_image.filter_quality(), kMedium_SkFilterQuality);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_EQ(decoded_draw_image.image()->width(), 50);
  EXPECT_EQ(decoded_draw_image.image()->height(), 50);
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(decoded_draw_image.is_at_raster_decode());
  EXPECT_FALSE(cache.DiscardableIsLockedForTesting(draw_image));

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, GetLargeScaledDecodedImageForDraw) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(1, 48000));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  // The mip level scale should never go below 0 in any dimension.
  EXPECT_EQ(1, decoded_draw_image.image()->width());
  EXPECT_EQ(24000, decoded_draw_image.image()->height());
  EXPECT_EQ(decoded_draw_image.filter_quality(), kMedium_SkFilterQuality);
  EXPECT_FALSE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(decoded_draw_image.is_at_raster_decode());
  EXPECT_TRUE(cache.DiscardableIsLockedForTesting(draw_image));

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.UnrefImage(draw_image);
  EXPECT_FALSE(cache.DiscardableIsLockedForTesting(draw_image));
}

TEST_P(GpuImageDecodeCacheTest, AtRasterUsedDirectlyIfSpaceAllows) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  cache.SetAllByteLimitsForTesting(0);

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.need_unref);
  EXPECT_FALSE(result.task);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_draw_image.filter_quality(), kMedium_SkFilterQuality);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_TRUE(decoded_draw_image.is_at_raster_decode());
  EXPECT_FALSE(cache.DiscardableIsLockedForTesting(draw_image));

  cache.SetAllByteLimitsForTesting(96 * 1024 * 1024);

  // Finish our draw after increasing the memory limit, image should be added to
  // cache.
  cache.DrawWithImageFinished(draw_image, decoded_draw_image);

  ImageDecodeCache::TaskResult another_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(another_result.need_unref);
  EXPECT_FALSE(another_result.task);
  cache.UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest,
       GetDecodedImageForDrawAtRasterDecodeMultipleTimes) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  cache.SetAllByteLimitsForTesting(0);

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_draw_image.filter_quality(), kMedium_SkFilterQuality);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_TRUE(decoded_draw_image.is_at_raster_decode());
  EXPECT_FALSE(cache.DiscardableIsLockedForTesting(draw_image));

  DecodedDrawImage another_decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_draw_image.image()->uniqueID(),
            another_decoded_draw_image.image()->uniqueID());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  cache.DrawWithImageFinished(draw_image, another_decoded_draw_image);
}

TEST_P(GpuImageDecodeCacheTest,
       GetLargeDecodedImageForDrawAtRasterDecodeMultipleTimes) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(1, 24000));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_draw_image.filter_quality(), kMedium_SkFilterQuality);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_FALSE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_TRUE(decoded_draw_image.is_at_raster_decode());
  EXPECT_TRUE(cache.DiscardableIsLockedForTesting(draw_image));

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
  EXPECT_FALSE(cache.DiscardableIsLockedForTesting(draw_image));

  DecodedDrawImage second_decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(second_decoded_draw_image.filter_quality(),
            kMedium_SkFilterQuality);
  EXPECT_TRUE(second_decoded_draw_image.image());
  EXPECT_FALSE(second_decoded_draw_image.image()->isTextureBacked());
  EXPECT_TRUE(second_decoded_draw_image.is_at_raster_decode());
  EXPECT_TRUE(cache.DiscardableIsLockedForTesting(draw_image));

  cache.DrawWithImageFinished(draw_image, second_decoded_draw_image);
  EXPECT_FALSE(cache.DiscardableIsLockedForTesting(draw_image));
}

TEST_P(GpuImageDecodeCacheTest, ZeroSizedImagesAreSkipped) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.f, 0.f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.task);
  EXPECT_FALSE(result.need_unref);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_FALSE(decoded_draw_image.image());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, NonOverlappingSrcRectImagesAreSkipped) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(
      image, SkIRect::MakeXYWH(150, 150, image.width(), image.height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_FALSE(result.task);
  EXPECT_FALSE(result.need_unref);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image);
  EXPECT_FALSE(decoded_draw_image.image());

  cache.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, CanceledTasksDoNotCountAgainstBudget) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(
      image, SkIRect::MakeXYWH(0, 0, image.width(), image.height()), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_NE(0u, cache.GetBytesUsedForTesting());
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(result.need_unref);

  TestTileTaskRunner::CancelTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::CompleteTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  cache.UnrefImage(draw_image);
  EXPECT_EQ(0u, cache.GetBytesUsedForTesting());
}

TEST_P(GpuImageDecodeCacheTest, ShouldAggressivelyFreeResources) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  {
    ImageDecodeCache::TaskResult result = cache.GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);

    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());

    cache.UnrefImage(draw_image);

    // We should now have data image in our cache.
    EXPECT_GT(cache.GetBytesUsedForTesting(), 0u);

    // Tell our cache to aggressively free resources.
    cache.SetShouldAggressivelyFreeResources(true);
    EXPECT_EQ(0u, cache.GetBytesUsedForTesting());
  }

  // Attempting to upload a new image should succeed, but the image should not
  // be cached past its use.
  {
    ImageDecodeCache::TaskResult result = cache.GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);

    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache.UnrefImage(draw_image);

    EXPECT_EQ(cache.GetBytesUsedForTesting(), 0u);
  }

  // We now tell the cache to not aggressively free resources. The image may
  // now be cached past its use.
  cache.SetShouldAggressivelyFreeResources(false);
  {
    ImageDecodeCache::TaskResult result = cache.GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);

    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache.UnrefImage(draw_image);

    EXPECT_GT(cache.GetBytesUsedForTesting(), 0u);
  }
}

TEST_P(GpuImageDecodeCacheTest, OrphanedImagesFreeOnReachingZeroRefs) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Create a downscaled image.
  PaintImage first_image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache.GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  // The budget should account for exactly one image.
  EXPECT_EQ(cache.GetBytesUsedForTesting(),
            cache.GetDrawImageSizeForTesting(first_draw_image));

  // Create a larger version of |first_image|, this should immediately free the
  // memory used by |first_image| for the smaller scale.
  DrawImage second_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache.GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache.UnrefImage(second_draw_image);

  // The budget should account for both images one image.
  EXPECT_EQ(cache.GetBytesUsedForTesting(),
            cache.GetDrawImageSizeForTesting(second_draw_image) +
                cache.GetDrawImageSizeForTesting(first_draw_image));

  // Unref the first image, it was orphaned, so it should be immediately
  // deleted.
  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());
  cache.UnrefImage(first_draw_image);

  // The budget should account for exactly one image.
  EXPECT_EQ(cache.GetBytesUsedForTesting(),
            cache.GetDrawImageSizeForTesting(second_draw_image));
}

TEST_P(GpuImageDecodeCacheTest, OrphanedZeroRefImagesImmediatelyDeleted) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Create a downscaled image.
  PaintImage first_image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult first_result = cache.GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());
  cache.UnrefImage(first_draw_image);

  // The budget should account for exactly one image.
  EXPECT_EQ(cache.GetBytesUsedForTesting(),
            cache.GetDrawImageSizeForTesting(first_draw_image));

  // Create a larger version of |first_image|, this should immediately free the
  // memory used by |first_image| for the smaller scale.
  DrawImage second_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult second_result = cache.GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache.UnrefImage(second_draw_image);

  // The budget should account for exactly one image.
  EXPECT_EQ(cache.GetBytesUsedForTesting(),
            cache.GetDrawImageSizeForTesting(second_draw_image));
}

TEST_P(GpuImageDecodeCacheTest, QualityCappedAtMedium) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  bool is_decomposable = true;
  SkMatrix matrix = CreateMatrix(SkSize::Make(0.4f, 0.4f), is_decomposable);

  // Create an image with kLow_FilterQuality.
  DrawImage low_draw_image(image,
                           SkIRect::MakeWH(image.width(), image.height()),
                           kLow_SkFilterQuality, matrix,
                           PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult low_result = cache.GetTaskForImageAndRef(
      low_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(low_result.need_unref);
  EXPECT_TRUE(low_result.task);

  // Get the same image at kMedium_SkFilterQuality. We can't re-use low, so we
  // should get a new task/ref.
  DrawImage medium_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()),
      kMedium_SkFilterQuality, matrix, PaintImage::kDefaultFrameIndex,
      DefaultColorSpace());
  ImageDecodeCache::TaskResult medium_result = cache.GetTaskForImageAndRef(
      medium_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(medium_result.need_unref);
  EXPECT_TRUE(medium_result.task.get());
  EXPECT_FALSE(low_result.task.get() == medium_result.task.get());

  // Get the same image at kHigh_FilterQuality. We should re-use medium.
  DrawImage large_draw_image(
      image, SkIRect::MakeWH(image.width(), image.height()),
      kHigh_SkFilterQuality, matrix, PaintImage::kDefaultFrameIndex,
      DefaultColorSpace());
  ImageDecodeCache::TaskResult large_result = cache.GetTaskForImageAndRef(
      large_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(large_result.need_unref);
  EXPECT_TRUE(medium_result.task.get() == large_result.task.get());

  TestTileTaskRunner::ProcessTask(low_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(low_result.task.get());
  TestTileTaskRunner::ProcessTask(medium_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(medium_result.task.get());

  cache.UnrefImage(low_draw_image);
  cache.UnrefImage(medium_draw_image);
  cache.UnrefImage(large_draw_image);
}

// Ensure that switching to a mipped version of an image after the initial
// cache entry creation doesn't cause a buffer overflow/crash.
TEST_P(GpuImageDecodeCacheTest, GetDecodedImageForDrawMipUsageChange) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Create an image decode task and cache entry that does not need mips.
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(4000, 4000));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  // Cancel the task without ever using it.
  TestTileTaskRunner::CancelTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::CompleteTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::CancelTask(result.task.get());
  TestTileTaskRunner::CompleteTask(result.task.get());

  cache.UnrefImage(draw_image);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());

  // Do an at-raster decode of the above image that *does* require mips.
  DrawImage draw_image_mips(
      image, SkIRect::MakeWH(image.width(), image.height()), quality,
      CreateMatrix(SkSize::Make(0.6f, 0.6f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  DecodedDrawImage decoded_draw_image =
      cache.GetDecodedImageForDraw(draw_image_mips);
  cache.DrawWithImageFinished(draw_image_mips, decoded_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, MemoryStateSuspended) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());

  // First Insert an image into our cache.
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(1, 1));
  bool is_decomposable = true;
  SkMatrix matrix = CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable);
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       kLow_SkFilterQuality, matrix,
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());
  cache.UnrefImage(draw_image);

  // The image should be cached.
  EXPECT_GT(cache.GetBytesUsedForTesting(), 0u);
  EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), 1u);

  // Set us to the not visible state (prerequisite for SUSPENDED).
  cache.SetShouldAggressivelyFreeResources(true);

  // Image should be cached, but not using memory budget.
  EXPECT_EQ(cache.GetBytesUsedForTesting(), 0u);
  EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), 1u);

  // Set us to the SUSPENDED state with purging.
  cache.OnPurgeMemory();
  cache.OnMemoryStateChange(base::MemoryState::SUSPENDED);

  // Nothing should be cached.
  EXPECT_EQ(cache.GetBytesUsedForTesting(), 0u);
  EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), 0u);

  // Attempts to get a task for the image will still succeed, as SUSPENDED
  // doesn't impact working set size.
  result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());
  cache.UnrefImage(draw_image);

  // Nothing should be cached.
  EXPECT_EQ(cache.GetBytesUsedForTesting(), 0u);
  EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), 0u);

  // Restore us to visible and NORMAL memory state.
  cache.OnMemoryStateChange(base::MemoryState::NORMAL);
  cache.SetShouldAggressivelyFreeResources(false);

  // We should now be able to create a task again (space available).
  result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);

  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());
  cache.UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, OutOfRasterDecodeTask) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(1, 1));
  bool is_decomposable = true;
  SkMatrix matrix = CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable);
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       kLow_SkFilterQuality, matrix,
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  ImageDecodeCache::TaskResult result =
      cache.GetOutOfRasterDecodeTaskForImageAndRef(draw_image);
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  EXPECT_TRUE(cache.IsInInUseCacheForTesting(draw_image));

  // Run the decode task.
  TestTileTaskRunner::ProcessTask(result.task.get());

  // The image should remain in the cache till we unref it.
  EXPECT_TRUE(cache.IsInInUseCacheForTesting(draw_image));
  cache.UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, ZeroCacheNormalWorkingSet) {
  // Setup - Image cache has a normal working set, but zero cache size.
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  GpuImageDecodeCache cache(context_provider.get(), kN32_SkColorType,
                            kGpuMemoryLimitBytes, 0);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Add an image to the cache. Due to normal working set, this should produce
  // a task and a ref.
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  EXPECT_EQ(result.task->dependencies().size(), 1u);
  EXPECT_TRUE(result.task->dependencies()[0]);

  // Run the task.
  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  // Request the same image - it should be cached.
  ImageDecodeCache::TaskResult second_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_FALSE(second_result.task);

  // Unref both images.
  cache.UnrefImage(draw_image);
  cache.UnrefImage(draw_image);

  // Get the image again. As it was fully unreffed, it is no longer in the
  // working set and will be evicted due to 0 cache size.
  ImageDecodeCache::TaskResult third_result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task);
  EXPECT_EQ(third_result.task->dependencies().size(), 1u);
  EXPECT_TRUE(third_result.task->dependencies()[0]);

  TestTileTaskRunner::ProcessTask(third_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(third_result.task.get());

  cache.UnrefImage(draw_image);
}

TEST_P(GpuImageDecodeCacheTest, SmallCacheNormalWorkingSet) {
  // Cache will fit one (but not two) 100x100 images.
  size_t cache_size = 190 * 100 * 4;

  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  GpuImageDecodeCache cache(context_provider.get(), kN32_SkColorType,
                            kGpuMemoryLimitBytes, cache_size);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  PaintImage image2 = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image2(
      image2, SkIRect::MakeWH(image2.width(), image2.height()), quality,
      CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, DefaultColorSpace());

  // Add an image to the cache and un-ref it.
  {
    ImageDecodeCache::TaskResult result = cache.GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);
    EXPECT_EQ(result.task->dependencies().size(), 1u);
    EXPECT_TRUE(result.task->dependencies()[0]);

    // Run the task and unref the image.
    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache.UnrefImage(draw_image);
  }

  // Request the same image - it should be cached.
  {
    ImageDecodeCache::TaskResult result = cache.GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_FALSE(result.task);
    cache.UnrefImage(draw_image);
  }

  // Add a new image to the cache. It should push out the old one.
  {
    ImageDecodeCache::TaskResult result = cache.GetTaskForImageAndRef(
        draw_image2, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);
    EXPECT_EQ(result.task->dependencies().size(), 1u);
    EXPECT_TRUE(result.task->dependencies()[0]);

    // Run the task and unref the image.
    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache.UnrefImage(draw_image2);
  }

  // Request the second image - it should be cached.
  {
    ImageDecodeCache::TaskResult result = cache.GetTaskForImageAndRef(
        draw_image2, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_FALSE(result.task);
    cache.UnrefImage(draw_image2);
  }

  // Request the first image - it should have been evicted and return a new
  // task.
  {
    ImageDecodeCache::TaskResult result = cache.GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);
    EXPECT_EQ(result.task->dependencies().size(), 1u);
    EXPECT_TRUE(result.task->dependencies()[0]);

    // Run the task and unref the image.
    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache.UnrefImage(draw_image);
  }
}

TEST_P(GpuImageDecodeCacheTest, ClearCache) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  for (int i = 0; i < 10; ++i) {
    PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
    DrawImage draw_image(
        image, SkIRect::MakeWH(image.width(), image.height()), quality,
        CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
        PaintImage::kDefaultFrameIndex, DefaultColorSpace());
    ImageDecodeCache::TaskResult result = cache.GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);
    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache.UnrefImage(draw_image);
  }

  // We should now have data image in our cache.
  EXPECT_GT(cache.GetBytesUsedForTesting(), 0u);
  EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), 10u);

  // Tell our cache to clear resources.
  cache.ClearCache();

  // We should now have nothing in our cache.
  EXPECT_EQ(cache.GetBytesUsedForTesting(), 0u);
  EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), 0u);
}

TEST_P(GpuImageDecodeCacheTest, ClearCacheInUse) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  // Create an image but keep it reffed so it can't be immediately freed.
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       PaintImage::kDefaultFrameIndex, DefaultColorSpace());
  ImageDecodeCache::TaskResult result =
      cache.GetTaskForImageAndRef(draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(result.need_unref);
  EXPECT_TRUE(result.task);
  TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(result.task.get());

  // We should now have data image in our cache.
  EXPECT_GT(cache.GetBytesUsedForTesting(), 0u);
  EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), 1u);

  // Tell our cache to clear resources.
  cache.ClearCache();
  // We should still have data, as we can't clear the in-use entry.
  EXPECT_GT(cache.GetBytesUsedForTesting(), 0u);
  // But the num (persistent) entries should be 0, as the entry is orphaned.
  EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), 0u);

  // Unref the image, it should immidiately delete, leaving our cache empty.
  cache.UnrefImage(draw_image);
  EXPECT_EQ(cache.GetBytesUsedForTesting(), 0u);
  EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), 0u);
}

TEST_P(GpuImageDecodeCacheTest, GetTaskForImageDifferentColorSpace) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  gfx::ColorSpace color_space_a = gfx::ColorSpace::CreateSRGB();
  gfx::ColorSpace color_space_b = gfx::ColorSpace::CreateXYZD50();

  PaintImage first_image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, color_space_a);
  ImageDecodeCache::TaskResult first_result = cache.GetTaskForImageAndRef(
      first_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(first_result.need_unref);
  EXPECT_TRUE(first_result.task);

  DrawImage second_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, color_space_b);
  ImageDecodeCache::TaskResult second_result = cache.GetTaskForImageAndRef(
      second_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(second_result.need_unref);
  EXPECT_TRUE(second_result.task);
  EXPECT_TRUE(first_result.task.get() != second_result.task.get());

  DrawImage third_draw_image(
      first_image, SkIRect::MakeWH(first_image.width(), first_image.height()),
      quality, CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
      PaintImage::kDefaultFrameIndex, color_space_a);
  ImageDecodeCache::TaskResult third_result = cache.GetTaskForImageAndRef(
      third_draw_image, ImageDecodeCache::TracingInfo());
  EXPECT_TRUE(third_result.need_unref);
  EXPECT_TRUE(third_result.task.get() == first_result.task.get());

  TestTileTaskRunner::ProcessTask(first_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_result.task.get());
  TestTileTaskRunner::ProcessTask(second_result.task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_result.task.get());

  cache.UnrefImage(first_draw_image);
  cache.UnrefImage(second_draw_image);
  cache.UnrefImage(third_draw_image);
}

TEST_P(GpuImageDecodeCacheTest, RemoveUnusedImage) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;
  std::vector<PaintImage::FrameKey> frame_keys;

  for (int i = 0; i < 10; ++i) {
    PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
    DrawImage draw_image(
        image, SkIRect::MakeWH(image.width(), image.height()), quality,
        CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
        PaintImage::kDefaultFrameIndex, DefaultColorSpace());
    frame_keys.push_back(draw_image.frame_key());
    ImageDecodeCache::TaskResult result = cache.GetTaskForImageAndRef(
        draw_image, ImageDecodeCache::TracingInfo());
    EXPECT_TRUE(result.need_unref);
    EXPECT_TRUE(result.task);
    TestTileTaskRunner::ProcessTask(result.task->dependencies()[0].get());
    TestTileTaskRunner::ProcessTask(result.task.get());
    cache.UnrefImage(draw_image);
  }

  // We should now have data image in our cache.
  EXPECT_GT(cache.GetBytesUsedForTesting(), 0u);
  EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), 10u);

  // Remove unused ids.
  for (uint32_t i = 0; i < 10; ++i) {
    cache.NotifyImageUnused(frame_keys[i]);
    EXPECT_EQ(cache.GetNumCacheEntriesForTesting(), (10 - i - 1));
  }
}

TEST_P(GpuImageDecodeCacheTest, CacheDecodesExpectedFrames) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeCache cache(context_provider.get(), GetParam());

  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(4)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(5)),
  };
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(SkImageInfo::MakeN32Premul(10, 10),
                                          frames);
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(generator)
                         .set_frame_index(0u)
                         .TakePaintImage();

  viz::ContextProvider::ScopedContextLock context_lock(context_provider.get());

  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;
  DrawImage draw_image(image, SkIRect::MakeWH(image.width(), image.height()),
                       quality,
                       CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable),
                       1u, DefaultColorSpace());
  auto decoded_image = cache.GetDecodedImageForDraw(draw_image);
  ASSERT_TRUE(decoded_image.image());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(1u), 1u);
  generator->reset_frames_decoded();
  cache.DrawWithImageFinished(draw_image, decoded_image);

  // Scaled.
  DrawImage scaled_draw_image(draw_image, 0.5f, 2u,
                              draw_image.target_color_space());
  decoded_image = cache.GetDecodedImageForDraw(scaled_draw_image);
  ASSERT_TRUE(decoded_image.image());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(2u), 1u);
  generator->reset_frames_decoded();
  cache.DrawWithImageFinished(scaled_draw_image, decoded_image);

  // Subset.
  DrawImage subset_draw_image(
      image, SkIRect::MakeWH(5, 5), quality,
      CreateMatrix(SkSize::Make(1.0f, 1.0f), is_decomposable), 3u,
      DefaultColorSpace());
  decoded_image = cache.GetDecodedImageForDraw(subset_draw_image);
  ASSERT_TRUE(decoded_image.image());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(3u), 1u);
  generator->reset_frames_decoded();
  cache.DrawWithImageFinished(subset_draw_image, decoded_image);
}

INSTANTIATE_TEST_CASE_P(GpuImageDecodeCacheTests,
                        GpuImageDecodeCacheTest,
                        ::testing::Values(kN32_SkColorType,
                                          kARGB_4444_SkColorType));

}  // namespace
}  // namespace cc
