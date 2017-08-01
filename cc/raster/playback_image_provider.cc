// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/playback_image_provider.h"

#include "base/memory/ptr_util.h"
#include "cc/tiles/image_decode_cache.h"

namespace cc {
namespace {
SkIRect RoundOutRect(const SkRect& rect) {
  SkIRect result;
  rect.roundOut(&result);
  return result;
}

void UnrefImageFromCache(DrawImage draw_image,
                         ImageDecodeCache* cache,
                         DecodedDrawImage decoded_draw_image) {
  if (decoded_draw_image.image())
    cache->DrawWithImageFinished(draw_image, decoded_draw_image);
}

}  // namespace

PlaybackImageProvider::PlaybackImageProvider(
    bool skip_all_images,
    PaintImageIdFlatSet images_to_skip,
    ImageDecodeCache* cache,
    const gfx::ColorSpace& target_color_space)
    : skip_all_images_(skip_all_images),
      images_to_skip_(std::move(images_to_skip)),
      cache_(cache),
      target_color_space_(target_color_space) {
  DCHECK(cache_);
}

PlaybackImageProvider::~PlaybackImageProvider() = default;

PlaybackImageProvider::PlaybackImageProvider(PlaybackImageProvider&& other) =
    default;

PlaybackImageProvider& PlaybackImageProvider::operator=(
    PlaybackImageProvider&& other) = default;

ImageProvider::ScopedDecodedDrawImage
PlaybackImageProvider::GetDecodedDrawImage(const PaintImage& paint_image,
                                           const SkRect& src_rect,
                                           SkFilterQuality filter_quality,
                                           const SkMatrix& matrix) {
  // Return an empty decoded images if we are skipping all images during this
  // raster.
  if (skip_all_images_)
    return ScopedDecodedDrawImage();

  if (images_to_skip_.count(paint_image.stable_id()) != 0) {
    DCHECK(paint_image.GetSkImage()->isLazyGenerated());
    return ScopedDecodedDrawImage();
  }

  if (!paint_image.GetSkImage()->isLazyGenerated()) {
    return ScopedDecodedDrawImage(
        DecodedDrawImage(paint_image.GetSkImage(), SkSize::Make(0, 0),
                         SkSize::Make(1.f, 1.f), filter_quality));
  }

  DrawImage draw_image = DrawImage(paint_image, RoundOutRect(src_rect),
                                   filter_quality, matrix, target_color_space_);
  return ScopedDecodedDrawImage(
      cache_->GetDecodedImageForDraw(draw_image),
      base::BindOnce(&UnrefImageFromCache, std::move(draw_image), cache_));
}

}  // namespace cc
