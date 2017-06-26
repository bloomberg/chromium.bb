// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/discardable_image_map.h"

#include <stddef.h>

#include <algorithm>
#include <limits>

#include "base/memory/ptr_util.h"
#include "cc/paint/discardable_image_store.h"

namespace cc {

DiscardableImageMap::DiscardableImageMap() {}

DiscardableImageMap::~DiscardableImageMap() {}

std::unique_ptr<DiscardableImageStore>
DiscardableImageMap::BeginGeneratingMetadata(const gfx::Size& bounds) {
  return base::MakeUnique<DiscardableImageStore>(bounds.width(),
                                                 bounds.height());
}

void DiscardableImageMap::EndGeneratingMetadata(
    std::vector<std::pair<DrawImage, gfx::Rect>> images,
    base::flat_map<PaintImage::Id, gfx::Rect> image_id_to_rect) {
  images_rtree_.Build(
      images,
      [](const std::vector<std::pair<DrawImage, gfx::Rect>>& items,
         size_t index) { return items[index].second; },
      [](const std::vector<std::pair<DrawImage, gfx::Rect>>& items,
         size_t index) { return items[index].first; });
  image_id_to_rect_ = std::move(image_id_to_rect);
}

void DiscardableImageMap::GetDiscardableImagesInRect(
    const gfx::Rect& rect,
    float contents_scale,
    const gfx::ColorSpace& target_color_space,
    std::vector<DrawImage>* images) const {
  *images = images_rtree_.Search(rect);
  // TODO(vmpstr): Remove the second pass and do this in TileManager.
  // crbug.com/727772.
  std::transform(
      images->begin(), images->end(), images->begin(),
      [&contents_scale, &target_color_space](const DrawImage& image) {
        return image.ApplyScale(contents_scale)
            .ApplyTargetColorSpace(target_color_space);
      });
}

gfx::Rect DiscardableImageMap::GetRectForImage(PaintImage::Id image_id) const {
  const auto& it = image_id_to_rect_.find(image_id);
  return it == image_id_to_rect_.end() ? gfx::Rect() : it->second;
}

DiscardableImageMap::ScopedMetadataGenerator::ScopedMetadataGenerator(
    DiscardableImageMap* image_map,
    const gfx::Size& bounds)
    : image_map_(image_map),
      image_store_(image_map->BeginGeneratingMetadata(bounds)) {}

DiscardableImageMap::ScopedMetadataGenerator::~ScopedMetadataGenerator() {
  image_map_->EndGeneratingMetadata(image_store_->TakeImages(),
                                    image_store_->TakeImageIdToRectMap());
}

void DiscardableImageMap::Reset() {
  image_id_to_rect_.clear();
  images_rtree_.Reset();
}

}  // namespace cc
