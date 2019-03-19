// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_image.h"

#include <utility>

#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/public/browser/browser_thread.h"

ThumbnailImage::ThumbnailImage() = default;
ThumbnailImage::~ThumbnailImage() = default;
ThumbnailImage::ThumbnailImage(const ThumbnailImage& other) = default;
ThumbnailImage::ThumbnailImage(ThumbnailImage&& other) = default;
ThumbnailImage& ThumbnailImage::operator=(const ThumbnailImage& other) =
    default;
ThumbnailImage& ThumbnailImage::operator=(ThumbnailImage&& other) = default;

gfx::ImageSkia ThumbnailImage::AsImageSkia() const {
  return ConvertFromRepresentation(image_representation_);
}

bool ThumbnailImage::AsImageSkiaAsync(AsImageSkiaCallback callback) const {
  if (!HasData())
    return false;

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ThumbnailImage::ConvertFromRepresentation,
                     image_representation_),
      std::move(callback));

  return true;
}

bool ThumbnailImage::HasData() const {
  return !image_representation_.isNull();
}

size_t ThumbnailImage::GetStorageSize() const {
  if (image_representation_.isNull())
    return 0;

  return image_representation_.bitmap()->computeByteSize();
}

// static
ThumbnailImage ThumbnailImage::FromSkBitmap(SkBitmap bitmap) {
  ThumbnailImage result;
  result.image_representation_ = ConvertToRepresentation(bitmap);
  return result;
}

// static
void ThumbnailImage::FromSkBitmapAsync(SkBitmap bitmap,
                                       CreateThumbnailCallback callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ThumbnailImage::ConvertToRepresentation, bitmap),
      base::BindOnce(
          [](CreateThumbnailCallback callback,
             ThumbnailRepresentation representation) {
            ThumbnailImage result;
            result.image_representation_ = representation;
            std::move(callback).Run(result);
          },
          std::move(callback)));
}

// static
gfx::ImageSkia ThumbnailImage::ConvertFromRepresentation(
    ThumbnailRepresentation representation) {
  return representation;
}

// static
ThumbnailImage::ThumbnailRepresentation ThumbnailImage::ConvertToRepresentation(
    SkBitmap bitmap) {
  ThumbnailRepresentation result = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  result.MakeThreadSafe();
  return result;
}
