// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_H_
#define CC_RESOURCES_TILE_H_

#include "base/memory/ref_counted.h"
#include "cc/resources/raster_source.h"
#include "cc/resources/tile_draw_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class PrioritizedTile;
class TileManager;
struct TilePriority;

class CC_EXPORT Tile {
 public:
  class CC_EXPORT Deleter {
   public:
    void operator()(Tile* tile) const;
  };

  enum TileRasterFlags { USE_PICTURE_ANALYSIS = 1 << 0 };

  typedef uint64 Id;

  Id id() const {
    return id_;
  }

  // TODO(vmpstr): Move this to the iterators.
  bool required_for_activation() const { return required_for_activation_; }
  void set_required_for_activation(bool is_required) {
    required_for_activation_ = is_required;
  }
  bool required_for_draw() const { return required_for_draw_; }
  void set_required_for_draw(bool is_required) {
    required_for_draw_ = is_required;
  }

  bool use_picture_analysis() const {
    return !!(flags_ & USE_PICTURE_ANALYSIS);
  }

  bool HasResource() const { return draw_info_.has_resource(); }
  bool NeedsRaster() const {
    return draw_info_.mode() == TileDrawInfo::OOM_MODE ||
           !draw_info_.IsReadyToDraw();
  }

  void AsValueInto(base::trace_event::TracedValue* value) const;

  inline bool IsReadyToDraw() const { return draw_info_.IsReadyToDraw(); }

  const TileDrawInfo& draw_info() const { return draw_info_; }

  TileDrawInfo& draw_info() { return draw_info_; }

  float contents_scale() const { return contents_scale_; }
  gfx::Rect content_rect() const { return content_rect_; }

  int layer_id() const { return layer_id_; }

  int source_frame_number() const { return source_frame_number_; }

  size_t GPUMemoryUsageInBytes() const;

  gfx::Size desired_texture_size() const { return desired_texture_size_; }

  void set_tiling_index(int i, int j) {
    tiling_i_index_ = i;
    tiling_j_index_ = j;
  }
  int tiling_i_index() const { return tiling_i_index_; }
  int tiling_j_index() const { return tiling_j_index_; }

 private:
  friend class TileManager;
  friend class FakeTileManager;
  friend class FakePictureLayerImpl;

  // Methods called by by tile manager.
  Tile(TileManager* tile_manager,
       RasterSource* raster_source,
       const gfx::Size& desired_texture_size,
       const gfx::Rect& content_rect,
       float contents_scale,
       int layer_id,
       int source_frame_number,
       int flags);
  ~Tile();

  bool HasRasterTask() const { return !!raster_task_.get(); }

  TileManager* tile_manager_;
  gfx::Size desired_texture_size_;
  gfx::Rect content_rect_;
  float contents_scale_;

  TileDrawInfo draw_info_;

  int layer_id_;
  int source_frame_number_;
  int flags_;
  int tiling_i_index_;
  int tiling_j_index_;
  bool required_for_activation_ : 1;
  bool required_for_draw_ : 1;

  Id id_;
  static Id s_next_id_;

  unsigned scheduled_priority_;

  scoped_refptr<RasterTask> raster_task_;

  DISALLOW_COPY_AND_ASSIGN(Tile);
};

using ScopedTilePtr = scoped_ptr<Tile, Tile::Deleter>;

}  // namespace cc

#endif  // CC_RESOURCES_TILE_H_
