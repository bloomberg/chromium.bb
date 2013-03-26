// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PICTURE_PILE_BASE_H_
#define CC_RESOURCES_PICTURE_PILE_BASE_H_

#include <list>
#include <utility>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "cc/base/hash_pair.h"
#include "cc/base/region.h"
#include "cc/base/tiling_data.h"
#include "cc/resources/picture.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT PicturePileBase : public base::RefCounted<PicturePileBase> {
 public:
  PicturePileBase();
  explicit PicturePileBase(const PicturePileBase* other);
  PicturePileBase(const PicturePileBase* other, unsigned thread_index);

  void Resize(gfx::Size size);
  gfx::Size size() const { return tiling_.total_size(); }
  void SetMinContentsScale(float min_contents_scale);

  void UpdateRecordedRegion();
  const Region& recorded_region() const { return recorded_region_; }

  int num_tiles_x() const { return tiling_.num_tiles_x(); }
  int num_tiles_y() const { return tiling_.num_tiles_y(); }
  gfx::Rect tile_bounds(int x, int y) const { return tiling_.TileBounds(x, y); }
  bool HasRecordingAt(int x, int y);
  bool CanRaster(float contents_scale, gfx::Rect content_rect);

  void SetTileGridSize(const gfx::Size& tile_grid_size);

 protected:
  virtual ~PicturePileBase();

  int num_raster_threads() { return num_raster_threads_; }
  int buffer_pixels() const { return tiling_.border_texels(); }
  void Clear();

  typedef std::pair<int, int> PictureListMapKey;
  typedef std::list<scoped_refptr<Picture> > PictureList;
  typedef base::hash_map<PictureListMapKey, PictureList> PictureListMap;

  // A picture pile is a tiled set of picture lists.  The picture list map
  // is a map of tile indices to picture lists.
  PictureListMap picture_list_map_;
  TilingData tiling_;
  Region recorded_region_;
  float min_contents_scale_;
  SkTileGridPicture::TileGridInfo tile_grid_info_;
  SkColor background_color_;
  int slow_down_raster_scale_factor_for_debug_;
  int num_raster_threads_;

 private:
  void SetBufferPixels(int buffer_pixels);

  friend class base::RefCounted<PicturePileBase>;
  DISALLOW_COPY_AND_ASSIGN(PicturePileBase);
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_PILE_BASE_H_
