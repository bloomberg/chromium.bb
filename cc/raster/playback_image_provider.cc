// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/playback_image_provider.h"

#include "base/memory/ptr_util.h"
#include "cc/tiles/image_decode_cache.h"

namespace cc {
namespace {
void UnrefImageFromCache(DrawImage draw_image,
                         ImageDecodeCache* cache,
                         DecodedDrawImage decoded_draw_image) {
  cache->DrawWithImageFinished(draw_image, decoded_draw_image);
}

}  // namespace

PlaybackImageProvider::PlaybackImageProvider(
    ImageDecodeCache* cache,
    const gfx::ColorSpace& target_color_space,
    base::Optional<Settings> settings)
    : cache_(cache),
      target_color_space_(target_color_space),
      settings_(std::move(settings)) {
  DCHECK(cache_);
}

PlaybackImageProvider::~PlaybackImageProvider() {
  DCHECK(!in_raster_);
}

PlaybackImageProvider::PlaybackImageProvider(PlaybackImageProvider&& other) =
    default;

PlaybackImageProvider& PlaybackImageProvider::operator=(
    PlaybackImageProvider&& other) = default;

void PlaybackImageProvider::BeginRaster() {
  DCHECK(decoded_at_raster_.empty());
  DCHECK(!in_raster_);
  in_raster_ = true;

  if (!settings_.has_value())
    return;

  for (auto& draw_image : settings_->at_raster_images)
    decoded_at_raster_.push_back(GetDecodedDrawImage(draw_image));
}

void PlaybackImageProvider::EndRaster() {
  DCHECK(in_raster_);
  decoded_at_raster_.clear();
  in_raster_ = false;
}

ImageProvider::ScopedDecodedDrawImage
PlaybackImageProvider::GetDecodedDrawImage(const DrawImage& draw_image) {
  DCHECK(in_raster_);

  // Return an empty decoded image if we are skipping all images during this
  // raster.
  if (!settings_.has_value())
    return ScopedDecodedDrawImage();

  const PaintImage& paint_image = draw_image.paint_image();

  if (settings_->images_to_skip.count(paint_image.stable_id()) != 0) {
    DCHECK(paint_image.GetSkImage()->isLazyGenerated());
    return ScopedDecodedDrawImage();
  }

  if (!paint_image.GetSkImage()->isLazyGenerated()) {
    return ScopedDecodedDrawImage(
        DecodedDrawImage(paint_image.GetSkImage(), SkSize::Make(0, 0),
                         SkSize::Make(1.f, 1.f), draw_image.filter_quality()));
  }

  const auto& it =
      settings_->image_to_current_frame_index.find(paint_image.stable_id());
  size_t frame_index = it == settings_->image_to_current_frame_index.end()
                           ? paint_image.frame_index()
                           : it->second;

  DrawImage adjusted_image(draw_image, 1.f, frame_index, target_color_space_);
  auto decoded_draw_image = cache_->GetDecodedImageForDraw(adjusted_image);

  return ScopedDecodedDrawImage(
      decoded_draw_image,
      base::BindOnce(&UnrefImageFromCache, std::move(adjusted_image), cache_));
}

PlaybackImageProvider::Settings::Settings() = default;
PlaybackImageProvider::Settings::Settings(const Settings& other) = default;
PlaybackImageProvider::Settings::~Settings() = default;

}  // namespace cc
