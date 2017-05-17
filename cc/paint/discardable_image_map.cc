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
  DCHECK(all_images_.empty());
  return base::MakeUnique<DiscardableImageStore>(
      bounds.width(), bounds.height(), &all_images_, &image_id_to_rect_);
}

void DiscardableImageMap::EndGeneratingMetadata() {
  images_rtree_.Build(all_images_,
                      [](const std::pair<DrawImage, gfx::Rect>& image) {
                        return image.second;
                      });
}

void DiscardableImageMap::GetDiscardableImagesInRect(
    const gfx::Rect& rect,
    float contents_scale,
    const gfx::ColorSpace& target_color_space,
    std::vector<DrawImage>* images) const {
  for (size_t index : images_rtree_.Search(rect)) {
    images->push_back(all_images_[index]
                          .first.ApplyScale(contents_scale)
                          .ApplyTargetColorSpace(target_color_space));
  }
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
  image_map_->EndGeneratingMetadata();
}

}  // namespace cc
