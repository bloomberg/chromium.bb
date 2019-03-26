// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_image.h"

#include <utility>

#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/codec/jpeg_codec.h"

// Refcounted class that stores compressed JPEG data.
class ThumbnailImage::ThumbnailData
    : public base::RefCountedThreadSafe<ThumbnailData> {
 public:
  static gfx::ImageSkia ToImageSkia(
      scoped_refptr<ThumbnailData> representation) {
    const auto& data = representation->data_;
    gfx::ImageSkia result = gfx::ImageSkia::CreateFrom1xBitmap(
        *gfx::JPEGCodec::Decode(data.data(), data.size()));
    result.MakeThreadSafe();
    return result;
  }

  static scoped_refptr<ThumbnailData> FromSkBitmap(SkBitmap bitmap) {
    constexpr int kCompressionQuality = 97;
    std::vector<uint8_t> data;
    const bool result =
        gfx::JPEGCodec::Encode(bitmap, kCompressionQuality, &data);
    DCHECK(result);
    return scoped_refptr<ThumbnailData>(new ThumbnailData(std::move(data)));
  }

  size_t size() const { return data_.size(); }

 private:
  friend base::RefCountedThreadSafe<ThumbnailData>;

  explicit ThumbnailData(std::vector<uint8_t>&& data)
      : data_(std::move(data)) {}

  ~ThumbnailData() = default;

  std::vector<uint8_t> data_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailData);
};

ThumbnailImage::ThumbnailImage() = default;
ThumbnailImage::~ThumbnailImage() = default;
ThumbnailImage::ThumbnailImage(const ThumbnailImage& other) = default;
ThumbnailImage::ThumbnailImage(ThumbnailImage&& other) = default;
ThumbnailImage& ThumbnailImage::operator=(const ThumbnailImage& other) =
    default;
ThumbnailImage& ThumbnailImage::operator=(ThumbnailImage&& other) = default;

gfx::ImageSkia ThumbnailImage::AsImageSkia() const {
  return ThumbnailData::ToImageSkia(image_representation_);
}

bool ThumbnailImage::AsImageSkiaAsync(AsImageSkiaCallback callback) const {
  if (!HasData())
    return false;

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ThumbnailData::ToImageSkia, image_representation_),
      std::move(callback));
  return true;
}

bool ThumbnailImage::HasData() const {
  return static_cast<bool>(image_representation_);
}

size_t ThumbnailImage::GetStorageSize() const {
  return image_representation_ ? image_representation_->size() : 0;
}

bool ThumbnailImage::BackedBySameObjectAs(const ThumbnailImage& other) const {
  return image_representation_.get() == other.image_representation_.get();
}

// static
ThumbnailImage ThumbnailImage::FromSkBitmap(SkBitmap bitmap) {
  ThumbnailImage result;
  result.image_representation_ = ThumbnailData::FromSkBitmap(bitmap);
  return result;
}

// static
void ThumbnailImage::FromSkBitmapAsync(SkBitmap bitmap,
                                       CreateThumbnailCallback callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ThumbnailData::FromSkBitmap, bitmap),
      base::BindOnce(
          [](CreateThumbnailCallback callback,
             scoped_refptr<ThumbnailData> representation) {
            ThumbnailImage result;
            result.image_representation_ = representation;
            std::move(callback).Run(result);
          },
          std::move(callback)));
}
