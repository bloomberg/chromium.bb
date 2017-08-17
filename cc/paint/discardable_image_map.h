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
class PaintOpBuffer;

// This class is used for generating discardable images data (see DrawImage
// for the type of data it stores). It allows the client to query a particular
// rect and get back a list of DrawImages in that rect.
class CC_PAINT_EXPORT DiscardableImageMap {
 public:
  DiscardableImageMap();
  ~DiscardableImageMap();

  bool empty() const { return image_id_to_rect_.empty(); }
  void GetDiscardableImagesInRect(const gfx::Rect& rect,
                                  std::vector<const DrawImage*>* images) const;
  gfx::Rect GetRectForImage(PaintImage::Id image_id) const;
  bool all_images_are_srgb() const { return all_images_are_srgb_; }

  void Reset();
  void Generate(const PaintOpBuffer* paint_op_buffer, const gfx::Rect& bounds);

 private:
  friend class ScopedMetadataGenerator;
  friend class DiscardableImageMapTest;

  std::unique_ptr<DiscardableImageStore> BeginGeneratingMetadata(
      const gfx::Size& bounds);
  void EndGeneratingMetadata(
      std::vector<std::pair<DrawImage, gfx::Rect>> images,
      base::flat_map<PaintImage::Id, gfx::Rect> image_id_to_rect);

  base::flat_map<PaintImage::Id, gfx::Rect> image_id_to_rect_;
  bool all_images_are_srgb_ = false;

  RTree<DrawImage> images_rtree_;
};

}  // namespace cc

#endif  // CC_PAINT_DISCARDABLE_IMAGE_MAP_H_
