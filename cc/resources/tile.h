// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_H_
#define CC_RESOURCES_TILE_H_

#include "base/memory/ref_counted.h"
#include "cc/base/ref_counted_managed.h"
#include "cc/resources/raster_source.h"
#include "cc/resources/tile_draw_info.h"
#include "cc/resources/tile_priority.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class TileManager;

class CC_EXPORT Tile : public RefCountedManaged<Tile> {
 public:
  enum TileRasterFlags { USE_PICTURE_ANALYSIS = 1 << 0 };

  typedef uint64 Id;

  Id id() const {
    return id_;
  }

  RasterSource* raster_source() { return raster_source_.get(); }

  const RasterSource* raster_source() const { return raster_source_.get(); }

  const TilePriority& priority(WhichTree tree) const {
    return priority_[tree];
  }

  TilePriority priority_for_tree_priority(TreePriority tree_priority) const {
    switch (tree_priority) {
      case SMOOTHNESS_TAKES_PRIORITY:
        return priority_[ACTIVE_TREE];
      case NEW_CONTENT_TAKES_PRIORITY:
        return priority_[PENDING_TREE];
      case SAME_PRIORITY_FOR_BOTH_TREES:
        return combined_priority();
      default:
        NOTREACHED();
        return TilePriority();
    }
  }

  TilePriority combined_priority() const {
    return TilePriority(priority_[ACTIVE_TREE],
                        priority_[PENDING_TREE]);
  }

  void SetPriority(WhichTree tree, const TilePriority& priority) {
    priority_[tree] = priority;
  }

  // TODO(vmpstr): Move this to the iterators.
  void set_is_occluded(WhichTree tree, bool is_occluded) {
    is_occluded_[tree] = is_occluded;
  }

  bool is_occluded(WhichTree tree) const { return is_occluded_[tree]; }

  void set_shared(bool is_shared) { is_shared_ = is_shared; }
  bool is_shared() const { return is_shared_; }

  bool is_occluded_combined() const {
    return is_occluded_[ACTIVE_TREE] && is_occluded_[PENDING_TREE];
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

  void AsValueInto(base::trace_event::TracedValue* dict) const;

  inline bool IsReadyToDraw() const { return draw_info_.IsReadyToDraw(); }

  const TileDrawInfo& draw_info() const { return draw_info_; }

  TileDrawInfo& draw_info() { return draw_info_; }

  float contents_scale() const { return contents_scale_; }
  gfx::Rect content_rect() const { return content_rect_; }

  int layer_id() const { return layer_id_; }

  int source_frame_number() const { return source_frame_number_; }

  void set_raster_source(scoped_refptr<RasterSource> raster_source) {
    DCHECK(raster_source->CoversRect(content_rect_, contents_scale_))
        << "Recording rect: "
        << gfx::ScaleToEnclosingRect(content_rect_, 1.f / contents_scale_)
               .ToString();
    raster_source_ = raster_source;
  }

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
  friend class PrioritizedTileSet;
  friend class FakeTileManager;
  friend class BinComparator;
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

  scoped_refptr<RasterSource> raster_source_;
  gfx::Size desired_texture_size_;
  gfx::Rect content_rect_;
  float contents_scale_;
  bool is_occluded_[LAST_TREE + 1];

  TilePriority priority_[LAST_TREE + 1];
  TileDrawInfo draw_info_;

  int layer_id_;
  int source_frame_number_;
  int flags_;
  int tiling_i_index_;
  int tiling_j_index_;
  bool is_shared_ : 1;
  bool required_for_activation_ : 1;
  bool required_for_draw_ : 1;

  Id id_;
  static Id s_next_id_;

  unsigned scheduled_priority_;

  scoped_refptr<RasterTask> raster_task_;

  DISALLOW_COPY_AND_ASSIGN(Tile);
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_H_
