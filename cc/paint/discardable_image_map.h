// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DISCARDABLE_IMAGE_MAP_H_
#define CC_PAINT_DISCARDABLE_IMAGE_MAP_H_

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "cc/base/rtree.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/image_id.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class DiscardableImageStore;

// This class is used for generating discardable images data (see DrawImage
// for the type of data it stores). It allows the client to query a particular
// rect and get back a list of DrawImages in that rect.
class CC_PAINT_EXPORT DiscardableImageMap {
 public:
  class CC_PAINT_EXPORT ScopedMetadataGenerator {
   public:
    ScopedMetadataGenerator(DiscardableImageMap* image_map,
                            const gfx::Size& bounds);
    ~ScopedMetadataGenerator();

    DiscardableImageStore* image_store() { return image_store_.get(); }

   private:
    DiscardableImageMap* image_map_;
    std::unique_ptr<DiscardableImageStore> image_store_;
  };

  DiscardableImageMap();
  ~DiscardableImageMap();

  bool empty() const { return all_images_.empty(); }
  void GetDiscardableImagesInRect(const gfx::Rect& rect,
                                  float contents_scale,
                                  const gfx::ColorSpace& target_color_space,
                                  std::vector<DrawImage>* images) const;
  gfx::Rect GetRectForImage(PaintImage::Id image_id) const;

  void Reset();

 private:
  friend class ScopedMetadataGenerator;
  friend class DiscardableImageMapTest;

  std::unique_ptr<DiscardableImageStore> BeginGeneratingMetadata(
      const gfx::Size& bounds);
  void EndGeneratingMetadata();

  std::vector<std::pair<DrawImage, gfx::Rect>> all_images_;
  base::flat_map<PaintImage::Id, gfx::Rect> image_id_to_rect_;

  RTree<size_t> images_rtree_;
};

}  // namespace cc

#endif  // CC_PAINT_DISCARDABLE_IMAGE_MAP_H_
