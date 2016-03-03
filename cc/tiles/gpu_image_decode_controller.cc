// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/gpu_image_decode_controller.h"

#include "cc/debug/devtools_instrumentation.h"
#include "cc/raster/tile_task_runner.h"
#include "skia/ext/refptr.h"

namespace cc {

class ImageDecodeTaskImpl : public ImageDecodeTask {
 public:
  ImageDecodeTaskImpl(GpuImageDecodeController* controller,
                      const DrawImage& image,
                      uint64_t source_prepare_tiles_id)
      : controller_(controller),
        image_(image),
        image_ref_(skia::SharePtr(image.image())),
        source_prepare_tiles_id_(source_prepare_tiles_id) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "ImageDecodeTaskImpl::RunOnWorkerThread", "mode", "gpu",
                 "source_prepare_tiles_id", source_prepare_tiles_id_);
    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        image_ref_.get());
    controller_->DecodeImage(image_);
  }

  // Overridden from TileTask:
  void ScheduleOnOriginThread(TileTaskClient* client) override {}
  void CompleteOnOriginThread(TileTaskClient* client) override {
    controller_->RemovePendingTaskForImage(image_);
  }

 protected:
  ~ImageDecodeTaskImpl() override {}

 private:
  GpuImageDecodeController* controller_;
  DrawImage image_;
  skia::RefPtr<const SkImage> image_ref_;
  uint64_t source_prepare_tiles_id_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeTaskImpl);
};

GpuImageDecodeController::GpuImageDecodeController() {}

GpuImageDecodeController::~GpuImageDecodeController() {}

bool GpuImageDecodeController::GetTaskForImageAndRef(
    const DrawImage& image,
    uint64_t prepare_tiles_id,
    scoped_refptr<ImageDecodeTask>* task) {
  auto image_id = image.image()->uniqueID();
  base::AutoLock lock(lock_);
  if (prerolled_images_.count(image_id) != 0) {
    *task = nullptr;
    return false;
  }

  scoped_refptr<ImageDecodeTask>& existing_task =
      pending_image_tasks_[image_id];
  if (!existing_task) {
    existing_task = make_scoped_refptr(
        new ImageDecodeTaskImpl(this, image, prepare_tiles_id));
  }
  return false;
}

void GpuImageDecodeController::UnrefImage(const DrawImage& image) {
  NOTREACHED();
}

DecodedDrawImage GpuImageDecodeController::GetDecodedImageForDraw(
    const DrawImage& draw_image) {
  return DecodedDrawImage(draw_image.image(), draw_image.filter_quality());
}

void GpuImageDecodeController::DrawWithImageFinished(
    const DrawImage& image,
    const DecodedDrawImage& decoded_image) {}

void GpuImageDecodeController::ReduceCacheUsage() {}

void GpuImageDecodeController::DecodeImage(const DrawImage& image) {
  image.image()->preroll();
  base::AutoLock lock(lock_);
  prerolled_images_.insert(image.image()->uniqueID());
}

void GpuImageDecodeController::RemovePendingTaskForImage(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  pending_image_tasks_.erase(image.image()->uniqueID());
}

}  // namespace cc
