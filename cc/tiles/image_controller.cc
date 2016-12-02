// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/image_controller.h"

namespace cc {

ImageController::ImageController() = default;
ImageController::~ImageController() = default;

void ImageController::SetImageDecodeCache(ImageDecodeCache* cache) {
  // We can only switch from null to non-null and back.
  // CHECK to debug crbug.com/650234.
  CHECK(cache || cache_);
  CHECK(!cache || !cache_);

  if (!cache) {
    SetPredecodeImages(std::vector<DrawImage>(),
                       ImageDecodeCache::TracingInfo());
  }
  cache_ = cache;
  // Debugging information for crbug.com/650234.
  ++num_times_cache_was_set_;
}

void ImageController::GetTasksForImagesAndRef(
    std::vector<DrawImage>* images,
    std::vector<scoped_refptr<TileTask>>* tasks,
    const ImageDecodeCache::TracingInfo& tracing_info) {
  DCHECK(cache_);
  for (auto it = images->begin(); it != images->end();) {
    scoped_refptr<TileTask> task;
    bool need_to_unref_when_finished =
        cache_->GetTaskForImageAndRef(*it, tracing_info, &task);
    if (task)
      tasks->push_back(std::move(task));

    if (need_to_unref_when_finished)
      ++it;
    else
      it = images->erase(it);
  }
}

void ImageController::UnrefImages(const std::vector<DrawImage>& images) {
  // Debugging information for crbug.com/650234.
  CHECK(cache_) << num_times_cache_was_set_;
  for (auto image : images)
    cache_->UnrefImage(image);
}

void ImageController::ReduceMemoryUsage() {
  DCHECK(cache_);
  cache_->ReduceCacheUsage();
}

std::vector<scoped_refptr<TileTask>> ImageController::SetPredecodeImages(
    std::vector<DrawImage> images,
    const ImageDecodeCache::TracingInfo& tracing_info) {
  std::vector<scoped_refptr<TileTask>> new_tasks;
  GetTasksForImagesAndRef(&images, &new_tasks, tracing_info);
  UnrefImages(predecode_locked_images_);
  predecode_locked_images_ = std::move(images);
  return new_tasks;
}

}  // namespace cc
