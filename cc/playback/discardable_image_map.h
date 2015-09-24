// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DISCARDABLE_IMAGE_MAP_H_
#define CC_PLAYBACK_DISCARDABLE_IMAGE_MAP_H_

#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "skia/ext/discardable_image_utils.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

class SkImage;

namespace cc {

class DisplayItemList;

typedef std::pair<int, int> ImageMapKey;
typedef std::vector<skia::PositionImage> Images;
typedef base::hash_map<ImageMapKey, Images> ImageHashmap;

// This class is used to gather images which would happen after record. It takes
// in |cell_size| to decide how big each grid cell should be.
class CC_EXPORT DiscardableImageMap {
 public:
  explicit DiscardableImageMap(const gfx::Size& cell_size);
  ~DiscardableImageMap();

  void GatherImagesFromPicture(SkPicture* picture, const gfx::Rect& layer_rect);

  bool empty() const { return data_hash_map_.empty(); }

  // This iterator imprecisely returns the set of images that are needed to
  // raster this layer rect from this picture.  Internally, images are
  // clumped into tile grid buckets, so there may be false positives.
  class CC_EXPORT Iterator {
   public:
    // Default iterator constructor that is used as place holder for invalid
    // Iterator.
    Iterator();
    Iterator(const gfx::Rect& layer_rect, const DisplayItemList* picture);
    ~Iterator();

    const skia::PositionImage* operator->() const {
      DCHECK_LT(current_index_, current_images_->size());
      return &(*current_images_)[current_index_];
    }

    const skia::PositionImage& operator*() const {
      DCHECK_LT(current_index_, current_images_->size());
      return (*current_images_)[current_index_];
    }

    Iterator& operator++();
    operator bool() const { return current_index_ < current_images_->size(); }

   private:
    void PointToFirstImage(const gfx::Rect& query_rect);

    static base::LazyInstance<Images> empty_images_;
    const DiscardableImageMap* target_image_map_;
    const Images* current_images_;
    unsigned current_index_;

    gfx::Rect map_layer_rect_;

    gfx::Point min_point_;
    gfx::Point max_point_;
    int current_x_;
    int current_y_;
  };

 private:
  gfx::Point min_pixel_cell_;
  gfx::Point max_pixel_cell_;
  gfx::Size cell_size_;

  ImageHashmap data_hash_map_;
};

}  // namespace cc

#endif  // CC_PLAYBACK_DISCARDABLE_IMAGE_MAP_H_
