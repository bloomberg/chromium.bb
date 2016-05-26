// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/gpu_image_decode_controller.h"

#include "cc/playback/draw_image.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_tile_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
namespace {

size_t kGpuMemoryLimitBytes = 96 * 1024 * 1024;
class TestGpuImageDecodeController : public GpuImageDecodeController {
 public:
  explicit TestGpuImageDecodeController(ContextProvider* context)
      : GpuImageDecodeController(context,
                                 ResourceFormat::RGBA_8888,
                                 kGpuMemoryLimitBytes) {}
};

sk_sp<SkImage> CreateImage(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  return SkImage::MakeFromBitmap(bitmap);
}

SkMatrix CreateMatrix(const SkSize& scale, bool is_decomposable) {
  SkMatrix matrix;
  matrix.setScale(scale.width(), scale.height());

  if (!is_decomposable) {
    // Perspective is not decomposable, add it.
    matrix[SkMatrix::kMPersp0] = 0.1f;
  }

  return matrix;
}

TEST(GpuImageDecodeControllerTest, GetTaskForImageSameImage) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  sk_sp<SkImage> image = CreateImage(100, 100);
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));
  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  DrawImage another_draw_image(
      image, SkIRect::MakeWH(image->width(), image->height()), quality,
      CreateMatrix(SkSize::Make(1.5f, 1.5f), is_decomposable));
  scoped_refptr<TileTask> another_task;
  need_unref = controller.GetTaskForImageAndRef(
      another_draw_image, ImageDecodeController::TracingInfo(), &another_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task.get() == another_task.get());

  TestTileTaskRunner::ProcessTask(task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(task.get());

  controller.UnrefImage(draw_image);
  controller.UnrefImage(draw_image);
}

TEST(GpuImageDecodeControllerTest, GetTaskForImageDifferentImage) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> first_image = CreateImage(100, 100);
  DrawImage first_draw_image(
      first_image, SkIRect::MakeWH(first_image->width(), first_image->height()),
      quality, CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));
  scoped_refptr<TileTask> first_task;
  bool need_unref = controller.GetTaskForImageAndRef(
      first_draw_image, ImageDecodeController::TracingInfo(), &first_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(first_task);

  sk_sp<SkImage> second_image = CreateImage(100, 100);
  DrawImage second_draw_image(
      second_image,
      SkIRect::MakeWH(second_image->width(), second_image->height()), quality,
      CreateMatrix(SkSize::Make(0.25f, 0.25f), is_decomposable));
  scoped_refptr<TileTask> second_task;
  need_unref = controller.GetTaskForImageAndRef(
      second_draw_image, ImageDecodeController::TracingInfo(), &second_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(second_task);
  EXPECT_TRUE(first_task.get() != second_task.get());

  TestTileTaskRunner::ProcessTask(first_task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(first_task.get());
  TestTileTaskRunner::ProcessTask(second_task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(second_task.get());

  controller.UnrefImage(first_draw_image);
  controller.UnrefImage(second_draw_image);
}

TEST(GpuImageDecodeControllerTest, GetTaskForImageAlreadyDecodedAndLocked) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));
  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);
  EXPECT_EQ(task->dependencies().size(), 1u);
  EXPECT_TRUE(task->dependencies()[0]);

  // Run the decode but don't complete it (this will keep the decode locked).
  TestTileTaskRunner::ScheduleTask(task->dependencies()[0].get());
  TestTileTaskRunner::RunTask(task->dependencies()[0].get());

  // Cancel the upload.
  TestTileTaskRunner::CancelTask(task.get());
  TestTileTaskRunner::CompleteTask(task.get());

  // Get the image again - we should have an upload task, but no dependent
  // decode task, as the decode was already locked.
  scoped_refptr<TileTask> another_task;
  need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &another_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(another_task);
  EXPECT_EQ(another_task->dependencies().size(), 0u);

  TestTileTaskRunner::ProcessTask(another_task.get());

  // Finally, complete the original decode task.
  TestTileTaskRunner::CompleteTask(task->dependencies()[0].get());

  controller.UnrefImage(draw_image);
  controller.UnrefImage(draw_image);
}

TEST(GpuImageDecodeControllerTest, GetTaskForImageAlreadyDecodedNotLocked) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));
  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);
  EXPECT_EQ(task->dependencies().size(), 1u);
  EXPECT_TRUE(task->dependencies()[0]);

  // Run the decode.
  TestTileTaskRunner::ProcessTask(task->dependencies()[0].get());

  // Cancel the upload.
  TestTileTaskRunner::CancelTask(task.get());
  TestTileTaskRunner::CompleteTask(task.get());

  // Unref the image.
  controller.UnrefImage(draw_image);

  // Get the image again - we should have an upload task and a dependent decode
  // task - this dependent task will typically just re-lock the image.
  scoped_refptr<TileTask> another_task;
  need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &another_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(another_task);
  EXPECT_EQ(another_task->dependencies().size(), 1u);
  EXPECT_TRUE(task->dependencies()[0]);

  TestTileTaskRunner::ProcessTask(another_task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(another_task.get());

  controller.UnrefImage(draw_image);
}

TEST(GpuImageDecodeControllerTest, GetTaskForImageAlreadyUploaded) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));
  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);
  EXPECT_EQ(task->dependencies().size(), 1u);
  EXPECT_TRUE(task->dependencies()[0]);

  TestTileTaskRunner::ProcessTask(task->dependencies()[0].get());
  TestTileTaskRunner::ScheduleTask(task.get());
  TestTileTaskRunner::RunTask(task.get());

  scoped_refptr<TileTask> another_task;
  need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &another_task);
  EXPECT_TRUE(need_unref);
  EXPECT_FALSE(another_task);

  TestTileTaskRunner::CompleteTask(task.get());

  controller.UnrefImage(draw_image);
  controller.UnrefImage(draw_image);
}

TEST(GpuImageDecodeControllerTest, GetTaskForImageCanceledGetsNewTask) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));
  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  TestTileTaskRunner::ProcessTask(task->dependencies()[0].get());

  scoped_refptr<TileTask> another_task;
  need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &another_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(another_task.get() == task.get());

  // Didn't run the task, so cancel it.
  TestTileTaskRunner::CancelTask(task.get());
  TestTileTaskRunner::CompleteTask(task.get());

  // Fully cancel everything (so the raster would unref things).
  controller.UnrefImage(draw_image);
  controller.UnrefImage(draw_image);

  // Here a new task is created.
  scoped_refptr<TileTask> third_task;
  need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &third_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(third_task);
  EXPECT_FALSE(third_task.get() == task.get());

  TestTileTaskRunner::ProcessTask(third_task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(third_task.get());

  controller.UnrefImage(draw_image);
}

TEST(GpuImageDecodeControllerTest,
     GetTaskForImageCanceledWhileReffedGetsNewTask) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));
  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  TestTileTaskRunner::ProcessTask(task->dependencies()[0].get());

  scoped_refptr<TileTask> another_task;
  need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &another_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(another_task.get() == task.get());

  // Didn't run the task, so cancel it.
  TestTileTaskRunner::CancelTask(task.get());
  TestTileTaskRunner::CompleteTask(task.get());

  // Note that here, everything is reffed, but a new task is created. This is
  // possible with repeated schedule/cancel operations.
  scoped_refptr<TileTask> third_task;
  need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &third_task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(third_task);
  EXPECT_FALSE(third_task.get() == task.get());

  TestTileTaskRunner::ProcessTask(third_task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(third_task.get());

  // 3 Unrefs!
  controller.UnrefImage(draw_image);
  controller.UnrefImage(draw_image);
  controller.UnrefImage(draw_image);
}

TEST(GpuImageDecodeControllerTest, NoTaskForImageAlreadyFailedDecoding) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));
  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  TestTileTaskRunner::ProcessTask(task->dependencies()[0].get());
  // Didn't run the task, so cancel it.
  TestTileTaskRunner::CancelTask(task.get());
  TestTileTaskRunner::CompleteTask(task.get());

  controller.SetImageDecodingFailedForTesting(draw_image);

  scoped_refptr<TileTask> another_task;
  need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &another_task);
  EXPECT_FALSE(need_unref);
  EXPECT_EQ(another_task.get(), nullptr);

  controller.UnrefImage(draw_image);
}

TEST(GpuImageDecodeControllerTest, GetDecodedImageForDraw) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));
  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  TestTileTaskRunner::ProcessTask(task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(decoded_draw_image.is_at_raster_decode());
  EXPECT_FALSE(controller.DiscardableIsLockedForTesting(draw_image));

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
  controller.UnrefImage(draw_image);
}

TEST(GpuImageDecodeControllerTest, GetLargeDecodedImageForDraw) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> image = CreateImage(1, 24000);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));
  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_TRUE(need_unref);
  EXPECT_TRUE(task);

  TestTileTaskRunner::ProcessTask(task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(task.get());

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_FALSE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_FALSE(decoded_draw_image.is_at_raster_decode());
  EXPECT_TRUE(controller.DiscardableIsLockedForTesting(draw_image));

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
  controller.UnrefImage(draw_image);
  EXPECT_FALSE(controller.DiscardableIsLockedForTesting(draw_image));
}

TEST(GpuImageDecodeControllerTest, GetDecodedImageForDrawAtRasterDecode) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  controller.SetCachedItemLimitForTesting(0);
  controller.SetCachedBytesLimitForTesting(0);

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));

  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_FALSE(need_unref);
  EXPECT_FALSE(task);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_TRUE(decoded_draw_image.is_at_raster_decode());
  EXPECT_FALSE(controller.DiscardableIsLockedForTesting(draw_image));

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST(GpuImageDecodeControllerTest, AtRasterUsedDirectlyIfSpaceAllows) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  controller.SetCachedItemLimitForTesting(0);
  controller.SetCachedBytesLimitForTesting(0);

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));

  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_FALSE(need_unref);
  EXPECT_FALSE(task);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_TRUE(decoded_draw_image.is_at_raster_decode());
  EXPECT_FALSE(controller.DiscardableIsLockedForTesting(draw_image));

  controller.SetCachedItemLimitForTesting(1000);
  controller.SetCachedBytesLimitForTesting(96 * 1024 * 1024);

  // Finish our draw after increasing the memory limit, image should be added to
  // cache.
  controller.DrawWithImageFinished(draw_image, decoded_draw_image);

  scoped_refptr<TileTask> another_task;
  bool another_task_needs_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_TRUE(another_task_needs_unref);
  EXPECT_FALSE(another_task);
  controller.UnrefImage(draw_image);
}

TEST(GpuImageDecodeControllerTest,
     GetDecodedImageForDrawAtRasterDecodeMultipleTimes) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  controller.SetCachedItemLimitForTesting(0);
  controller.SetCachedBytesLimitForTesting(0);

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_TRUE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_TRUE(decoded_draw_image.is_at_raster_decode());
  EXPECT_FALSE(controller.DiscardableIsLockedForTesting(draw_image));

  DecodedDrawImage another_decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_EQ(decoded_draw_image.image()->uniqueID(),
            another_decoded_draw_image.image()->uniqueID());

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
  controller.DrawWithImageFinished(draw_image, another_decoded_draw_image);
}

TEST(GpuImageDecodeControllerTest,
     GetLargeDecodedImageForDrawAtRasterDecodeMultipleTimes) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> image = CreateImage(1, 24000);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(decoded_draw_image.image());
  EXPECT_FALSE(decoded_draw_image.image()->isTextureBacked());
  EXPECT_TRUE(decoded_draw_image.is_at_raster_decode());
  EXPECT_TRUE(controller.DiscardableIsLockedForTesting(draw_image));

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
  EXPECT_FALSE(controller.DiscardableIsLockedForTesting(draw_image));

  DecodedDrawImage second_decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_TRUE(second_decoded_draw_image.image());
  EXPECT_FALSE(second_decoded_draw_image.image()->isTextureBacked());
  EXPECT_TRUE(second_decoded_draw_image.is_at_raster_decode());
  EXPECT_TRUE(controller.DiscardableIsLockedForTesting(draw_image));

  controller.DrawWithImageFinished(draw_image, second_decoded_draw_image);
  EXPECT_FALSE(controller.DiscardableIsLockedForTesting(draw_image));
}

TEST(GpuImageDecodeControllerTest, ZeroSizedImagesAreSkipped) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.f, 0.f), is_decomposable));

  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_FALSE(task);
  EXPECT_FALSE(need_unref);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_FALSE(decoded_draw_image.image());

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST(GpuImageDecodeControllerTest, NonOverlappingSrcRectImagesAreSkipped) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image, SkIRect::MakeXYWH(150, 150, image->width(), image->height()),
      quality, CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable));

  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_FALSE(task);
  EXPECT_FALSE(need_unref);

  // Must hold context lock before calling GetDecodedImageForDraw /
  // DrawWithImageFinished.
  ContextProvider::ScopedContextLock context_lock(context_provider.get());
  DecodedDrawImage decoded_draw_image =
      controller.GetDecodedImageForDraw(draw_image);
  EXPECT_FALSE(decoded_draw_image.image());

  controller.DrawWithImageFinished(draw_image, decoded_draw_image);
}

TEST(GpuImageDecodeControllerTest, CanceledTasksDoNotCountAgainstBudget) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(
      image, SkIRect::MakeXYWH(0, 0, image->width(), image->height()), quality,
      CreateMatrix(SkSize::Make(1.f, 1.f), is_decomposable));

  scoped_refptr<TileTask> task;
  bool need_unref = controller.GetTaskForImageAndRef(
      draw_image, ImageDecodeController::TracingInfo(), &task);
  EXPECT_NE(0u, controller.GetBytesUsedForTesting());
  EXPECT_TRUE(task);
  EXPECT_TRUE(need_unref);

  TestTileTaskRunner::CancelTask(task->dependencies()[0].get());
  TestTileTaskRunner::CompleteTask(task->dependencies()[0].get());
  TestTileTaskRunner::CancelTask(task.get());
  TestTileTaskRunner::CompleteTask(task.get());

  controller.UnrefImage(draw_image);
  EXPECT_EQ(0u, controller.GetBytesUsedForTesting());
}

TEST(GpuImageDecodeControllerTest, ShouldAggressivelyFreeResources) {
  auto context_provider = TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  TestGpuImageDecodeController controller(context_provider.get());
  bool is_decomposable = true;
  SkFilterQuality quality = kHigh_SkFilterQuality;

  sk_sp<SkImage> image = CreateImage(100, 100);
  DrawImage draw_image(image, SkIRect::MakeWH(image->width(), image->height()),
                       quality,
                       CreateMatrix(SkSize::Make(0.5f, 0.5f), is_decomposable));
  scoped_refptr<TileTask> task;
  {
    bool need_unref = controller.GetTaskForImageAndRef(
        draw_image, ImageDecodeController::TracingInfo(), &task);
    EXPECT_TRUE(need_unref);
    EXPECT_TRUE(task);
  }

  TestTileTaskRunner::ProcessTask(task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(task.get());

  controller.UnrefImage(draw_image);

  // We should now have data image in our cache.
  DCHECK_GT(controller.GetBytesUsedForTesting(), 0u);

  // Tell our controller to aggressively free resources.
  controller.SetShouldAggressivelyFreeResources(true);
  DCHECK_EQ(0u, controller.GetBytesUsedForTesting());

  // Attempting to upload a new image should result in at-raster decode.
  {
    bool need_unref = controller.GetTaskForImageAndRef(
        draw_image, ImageDecodeController::TracingInfo(), &task);
    EXPECT_FALSE(need_unref);
    EXPECT_FALSE(task);
  }

  // We now tell the controller to not aggressively free resources. Uploads
  // should work again.
  controller.SetShouldAggressivelyFreeResources(false);
  {
    bool need_unref = controller.GetTaskForImageAndRef(
        draw_image, ImageDecodeController::TracingInfo(), &task);
    EXPECT_TRUE(need_unref);
    EXPECT_TRUE(task);
  }

  TestTileTaskRunner::ProcessTask(task->dependencies()[0].get());
  TestTileTaskRunner::ProcessTask(task.get());

  // The image should be in our cache after un-ref.
  controller.UnrefImage(draw_image);
  DCHECK_GT(controller.GetBytesUsedForTesting(), 0u);
}

}  // namespace
}  // namespace cc
