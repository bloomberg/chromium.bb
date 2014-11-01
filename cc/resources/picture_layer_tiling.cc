// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_layer_tiling.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include "base/debug/trace_event.h"
#include "base/debug/trace_event_argument.h"
#include "base/logging.h"
#include "cc/base/math_util.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {
namespace {

const float kSoonBorderDistanceInScreenPixels = 312.f;

class TileEvictionOrder {
 public:
  explicit TileEvictionOrder(TreePriority tree_priority)
      : tree_priority_(tree_priority) {}
  ~TileEvictionOrder() {}

  bool operator()(const Tile* a, const Tile* b) {
    const TilePriority& a_priority =
        a->priority_for_tree_priority(tree_priority_);
    const TilePriority& b_priority =
        b->priority_for_tree_priority(tree_priority_);

    DCHECK(a_priority.priority_bin == b_priority.priority_bin);
    DCHECK(a->required_for_activation() == b->required_for_activation());

    // Or if a is occluded and b is unoccluded.
    bool a_is_occluded = a->is_occluded_for_tree_priority(tree_priority_);
    bool b_is_occluded = b->is_occluded_for_tree_priority(tree_priority_);
    if (a_is_occluded != b_is_occluded)
      return a_is_occluded;

    // Or if a is farther away from visible.
    return a_priority.distance_to_visible > b_priority.distance_to_visible;
  }

 private:
  TreePriority tree_priority_;
};

}  // namespace

scoped_ptr<PictureLayerTiling> PictureLayerTiling::Create(
    float contents_scale,
    const gfx::Size& layer_bounds,
    PictureLayerTilingClient* client) {
  return make_scoped_ptr(new PictureLayerTiling(contents_scale,
                                                layer_bounds,
                                                client));
}

PictureLayerTiling::PictureLayerTiling(float contents_scale,
                                       const gfx::Size& layer_bounds,
                                       PictureLayerTilingClient* client)
    : contents_scale_(contents_scale),
      layer_bounds_(layer_bounds),
      resolution_(NON_IDEAL_RESOLUTION),
      client_(client),
      tiling_data_(gfx::Size(), gfx::Size(), kBorderTexels),
      last_impl_frame_time_in_seconds_(0.0),
      content_to_screen_scale_(0.f),
      can_require_tiles_for_activation_(false),
      has_visible_rect_tiles_(false),
      has_skewport_rect_tiles_(false),
      has_soon_border_rect_tiles_(false),
      has_eventually_rect_tiles_(false),
      eviction_tiles_cache_valid_(false),
      eviction_cache_tree_priority_(SAME_PRIORITY_FOR_BOTH_TREES) {
  gfx::Size content_bounds =
      gfx::ToCeiledSize(gfx::ScaleSize(layer_bounds, contents_scale));
  gfx::Size tile_size = client_->CalculateTileSize(content_bounds);
  if (tile_size.IsEmpty()) {
    layer_bounds_ = gfx::Size();
    content_bounds = gfx::Size();
  }

  DCHECK(!gfx::ToFlooredSize(
      gfx::ScaleSize(layer_bounds, contents_scale)).IsEmpty()) <<
      "Tiling created with scale too small as contents become empty." <<
      " Layer bounds: " << layer_bounds.ToString() <<
      " Contents scale: " << contents_scale;

  tiling_data_.SetTilingSize(content_bounds);
  tiling_data_.SetMaxTextureSize(tile_size);
}

PictureLayerTiling::~PictureLayerTiling() {
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it)
    it->second->set_shared(false);
}

void PictureLayerTiling::SetClient(PictureLayerTilingClient* client) {
  client_ = client;
}

Tile* PictureLayerTiling::CreateTile(int i,
                                     int j,
                                     const PictureLayerTiling* twin_tiling) {
  TileMapKey key(i, j);
  DCHECK(tiles_.find(key) == tiles_.end());

  gfx::Rect paint_rect = tiling_data_.TileBoundsWithBorder(i, j);
  gfx::Rect tile_rect = paint_rect;
  tile_rect.set_size(tiling_data_.max_texture_size());

  // Check our twin for a valid tile.
  if (twin_tiling &&
      tiling_data_.max_texture_size() ==
      twin_tiling->tiling_data_.max_texture_size()) {
    if (Tile* candidate_tile = twin_tiling->TileAt(i, j)) {
      gfx::Rect rect =
          gfx::ScaleToEnclosingRect(paint_rect, 1.0f / contents_scale_);
      const Region* invalidation = client_->GetPendingInvalidation();
      if (!invalidation || !invalidation->Intersects(rect)) {
        DCHECK(!candidate_tile->is_shared());
        DCHECK_EQ(i, candidate_tile->tiling_i_index());
        DCHECK_EQ(j, candidate_tile->tiling_j_index());
        candidate_tile->set_shared(true);
        tiles_[key] = candidate_tile;
        return candidate_tile;
      }
    }
  }

  // Create a new tile because our twin didn't have a valid one.
  scoped_refptr<Tile> tile = client_->CreateTile(this, tile_rect);
  if (tile.get()) {
    DCHECK(!tile->is_shared());
    tile->set_tiling_index(i, j);
    tiles_[key] = tile;
  }
  eviction_tiles_cache_valid_ = false;
  return tile.get();
}

void PictureLayerTiling::CreateMissingTilesInLiveTilesRect() {
  const PictureLayerTiling* twin_tiling =
      client_->GetPendingOrActiveTwinTiling(this);
  bool include_borders = false;
  for (TilingData::Iterator iter(
           &tiling_data_, live_tiles_rect_, include_borders);
       iter;
       ++iter) {
    TileMapKey key = iter.index();
    TileMap::iterator find = tiles_.find(key);
    if (find != tiles_.end())
      continue;
    CreateTile(key.first, key.second, twin_tiling);
  }

  VerifyLiveTilesRect();
}

void PictureLayerTiling::UpdateTilesToCurrentPile(
    const Region& layer_invalidation,
    const gfx::Size& new_layer_bounds) {
  DCHECK(!new_layer_bounds.IsEmpty());

  gfx::Size tile_size = tiling_data_.max_texture_size();

  if (new_layer_bounds != layer_bounds_) {
    gfx::Size content_bounds =
        gfx::ToCeiledSize(gfx::ScaleSize(new_layer_bounds, contents_scale_));

    tile_size = client_->CalculateTileSize(content_bounds);
    if (tile_size.IsEmpty()) {
      layer_bounds_ = gfx::Size();
      content_bounds = gfx::Size();
    } else {
      layer_bounds_ = new_layer_bounds;
    }

    // The SetLiveTilesRect() method would drop tiles outside the new bounds,
    // but may do so incorrectly if resizing the tiling causes the number of
    // tiles in the tiling_data_ to change.
    gfx::Rect content_rect(content_bounds);
    int before_left = tiling_data_.TileXIndexFromSrcCoord(live_tiles_rect_.x());
    int before_top = tiling_data_.TileYIndexFromSrcCoord(live_tiles_rect_.y());
    int before_right =
        tiling_data_.TileXIndexFromSrcCoord(live_tiles_rect_.right() - 1);
    int before_bottom =
        tiling_data_.TileYIndexFromSrcCoord(live_tiles_rect_.bottom() - 1);

    // The live_tiles_rect_ is clamped to stay within the tiling size as we
    // change it.
    live_tiles_rect_.Intersect(content_rect);
    tiling_data_.SetTilingSize(content_bounds);

    int after_right = -1;
    int after_bottom = -1;
    if (!live_tiles_rect_.IsEmpty()) {
      after_right =
          tiling_data_.TileXIndexFromSrcCoord(live_tiles_rect_.right() - 1);
      after_bottom =
          tiling_data_.TileYIndexFromSrcCoord(live_tiles_rect_.bottom() - 1);
    }

    // There is no recycled twin since this is run on the pending tiling.
    PictureLayerTiling* recycled_twin = NULL;
    DCHECK_EQ(recycled_twin, client_->GetRecycledTwinTiling(this));
    DCHECK_EQ(PENDING_TREE, client_->GetTree());

    // Drop tiles outside the new layer bounds if the layer shrank.
    for (int i = after_right + 1; i <= before_right; ++i) {
      for (int j = before_top; j <= before_bottom; ++j)
        RemoveTileAt(i, j, recycled_twin);
    }
    for (int i = before_left; i <= after_right; ++i) {
      for (int j = after_bottom + 1; j <= before_bottom; ++j)
        RemoveTileAt(i, j, recycled_twin);
    }

    // If the layer grew, the live_tiles_rect_ is not changed, but a new row
    // and/or column of tiles may now exist inside the same live_tiles_rect_.
    const PictureLayerTiling* twin_tiling =
        client_->GetPendingOrActiveTwinTiling(this);
    if (after_right > before_right) {
      DCHECK_EQ(after_right, before_right + 1);
      for (int j = before_top; j <= after_bottom; ++j)
        CreateTile(after_right, j, twin_tiling);
    }
    if (after_bottom > before_bottom) {
      DCHECK_EQ(after_bottom, before_bottom + 1);
      for (int i = before_left; i <= before_right; ++i)
        CreateTile(i, after_bottom, twin_tiling);
    }
  }

  if (tile_size != tiling_data_.max_texture_size()) {
    tiling_data_.SetMaxTextureSize(tile_size);
    // When the tile size changes, the TilingData positions no longer work
    // as valid keys to the TileMap, so just drop all tiles.
    Reset();
  } else {
    Invalidate(layer_invalidation);
  }

  RasterSource* raster_source = client_->GetRasterSource();
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it)
    it->second->set_raster_source(raster_source);
  VerifyLiveTilesRect();
}

void PictureLayerTiling::RemoveTilesInRegion(const Region& layer_region) {
  bool recreate_invalidated_tiles = false;
  DoInvalidate(layer_region, recreate_invalidated_tiles);
}

void PictureLayerTiling::Invalidate(const Region& layer_region) {
  bool recreate_invalidated_tiles = true;
  DoInvalidate(layer_region, recreate_invalidated_tiles);
}

void PictureLayerTiling::DoInvalidate(const Region& layer_region,
                                      bool recreate_invalidated_tiles) {
  std::vector<TileMapKey> new_tile_keys;
  gfx::Rect expanded_live_tiles_rect =
      tiling_data_.ExpandRectIgnoringBordersToTileBounds(live_tiles_rect_);
  for (Region::Iterator iter(layer_region); iter.has_rect(); iter.next()) {
    gfx::Rect layer_rect = iter.rect();
    gfx::Rect content_rect =
        gfx::ScaleToEnclosingRect(layer_rect, contents_scale_);
    // Consider tiles inside the live tiles rect even if only their border
    // pixels intersect the invalidation. But don't consider tiles outside
    // the live tiles rect with the same conditions, as they won't exist.
    int border_pixels = tiling_data_.border_texels();
    content_rect.Inset(-border_pixels, -border_pixels);
    // Avoid needless work by not bothering to invalidate where there aren't
    // tiles.
    content_rect.Intersect(expanded_live_tiles_rect);
    if (content_rect.IsEmpty())
      continue;
    // Since the content_rect includes border pixels already, don't include
    // borders when iterating to avoid double counting them.
    bool include_borders = false;
    for (TilingData::Iterator iter(
             &tiling_data_, content_rect, include_borders);
         iter;
         ++iter) {
      // There is no recycled twin since this is run on the pending tiling.
      PictureLayerTiling* recycled_twin = NULL;
      DCHECK_EQ(recycled_twin, client_->GetRecycledTwinTiling(this));
      DCHECK_EQ(PENDING_TREE, client_->GetTree());
      if (RemoveTileAt(iter.index_x(), iter.index_y(), recycled_twin))
        new_tile_keys.push_back(iter.index());
    }
  }

  if (recreate_invalidated_tiles && !new_tile_keys.empty()) {
    for (size_t i = 0; i < new_tile_keys.size(); ++i) {
      // Don't try to share a tile with the twin layer, it's been invalidated so
      // we have to make our own tile here.
      const PictureLayerTiling* twin_tiling = NULL;
      CreateTile(new_tile_keys[i].first, new_tile_keys[i].second, twin_tiling);
    }
  }
}

PictureLayerTiling::CoverageIterator::CoverageIterator()
    : tiling_(NULL),
      current_tile_(NULL),
      tile_i_(0),
      tile_j_(0),
      left_(0),
      top_(0),
      right_(-1),
      bottom_(-1) {
}

PictureLayerTiling::CoverageIterator::CoverageIterator(
    const PictureLayerTiling* tiling,
    float dest_scale,
    const gfx::Rect& dest_rect)
    : tiling_(tiling),
      dest_rect_(dest_rect),
      dest_to_content_scale_(0),
      current_tile_(NULL),
      tile_i_(0),
      tile_j_(0),
      left_(0),
      top_(0),
      right_(-1),
      bottom_(-1) {
  DCHECK(tiling_);
  if (dest_rect_.IsEmpty())
    return;

  dest_to_content_scale_ = tiling_->contents_scale_ / dest_scale;

  gfx::Rect content_rect =
      gfx::ScaleToEnclosingRect(dest_rect_,
                                dest_to_content_scale_,
                                dest_to_content_scale_);
  // IndexFromSrcCoord clamps to valid tile ranges, so it's necessary to
  // check for non-intersection first.
  content_rect.Intersect(gfx::Rect(tiling_->tiling_size()));
  if (content_rect.IsEmpty())
    return;

  left_ = tiling_->tiling_data_.TileXIndexFromSrcCoord(content_rect.x());
  top_ = tiling_->tiling_data_.TileYIndexFromSrcCoord(content_rect.y());
  right_ = tiling_->tiling_data_.TileXIndexFromSrcCoord(
      content_rect.right() - 1);
  bottom_ = tiling_->tiling_data_.TileYIndexFromSrcCoord(
      content_rect.bottom() - 1);

  tile_i_ = left_ - 1;
  tile_j_ = top_;
  ++(*this);
}

PictureLayerTiling::CoverageIterator::~CoverageIterator() {
}

PictureLayerTiling::CoverageIterator&
PictureLayerTiling::CoverageIterator::operator++() {
  if (tile_j_ > bottom_)
    return *this;

  bool first_time = tile_i_ < left_;
  bool new_row = false;
  tile_i_++;
  if (tile_i_ > right_) {
    tile_i_ = left_;
    tile_j_++;
    new_row = true;
    if (tile_j_ > bottom_) {
      current_tile_ = NULL;
      return *this;
    }
  }

  current_tile_ = tiling_->TileAt(tile_i_, tile_j_);

  // Calculate the current geometry rect.  Due to floating point rounding
  // and ToEnclosingRect, tiles might overlap in destination space on the
  // edges.
  gfx::Rect last_geometry_rect = current_geometry_rect_;

  gfx::Rect content_rect = tiling_->tiling_data_.TileBounds(tile_i_, tile_j_);

  current_geometry_rect_ =
      gfx::ScaleToEnclosingRect(content_rect,
                                1 / dest_to_content_scale_,
                                1 / dest_to_content_scale_);

  current_geometry_rect_.Intersect(dest_rect_);

  if (first_time)
    return *this;

  // Iteration happens left->right, top->bottom.  Running off the bottom-right
  // edge is handled by the intersection above with dest_rect_.  Here we make
  // sure that the new current geometry rect doesn't overlap with the last.
  int min_left;
  int min_top;
  if (new_row) {
    min_left = dest_rect_.x();
    min_top = last_geometry_rect.bottom();
  } else {
    min_left = last_geometry_rect.right();
    min_top = last_geometry_rect.y();
  }

  int inset_left = std::max(0, min_left - current_geometry_rect_.x());
  int inset_top = std::max(0, min_top - current_geometry_rect_.y());
  current_geometry_rect_.Inset(inset_left, inset_top, 0, 0);

  if (!new_row) {
    DCHECK_EQ(last_geometry_rect.right(), current_geometry_rect_.x());
    DCHECK_EQ(last_geometry_rect.bottom(), current_geometry_rect_.bottom());
    DCHECK_EQ(last_geometry_rect.y(), current_geometry_rect_.y());
  }

  return *this;
}

gfx::Rect PictureLayerTiling::CoverageIterator::geometry_rect() const {
  return current_geometry_rect_;
}

gfx::Rect
PictureLayerTiling::CoverageIterator::full_tile_geometry_rect() const {
  gfx::Rect rect = tiling_->tiling_data_.TileBoundsWithBorder(tile_i_, tile_j_);
  rect.set_size(tiling_->tiling_data_.max_texture_size());
  return rect;
}

gfx::RectF PictureLayerTiling::CoverageIterator::texture_rect() const {
  gfx::PointF tex_origin =
      tiling_->tiling_data_.TileBoundsWithBorder(tile_i_, tile_j_).origin();

  // Convert from dest space => content space => texture space.
  gfx::RectF texture_rect(current_geometry_rect_);
  texture_rect.Scale(dest_to_content_scale_,
                     dest_to_content_scale_);
  texture_rect.Intersect(gfx::Rect(tiling_->tiling_size()));
  if (texture_rect.IsEmpty())
    return texture_rect;
  texture_rect.Offset(-tex_origin.OffsetFromOrigin());

  return texture_rect;
}

gfx::Size PictureLayerTiling::CoverageIterator::texture_size() const {
  return tiling_->tiling_data_.max_texture_size();
}

bool PictureLayerTiling::RemoveTileAt(int i,
                                      int j,
                                      PictureLayerTiling* recycled_twin) {
  TileMap::iterator found = tiles_.find(TileMapKey(i, j));
  if (found == tiles_.end())
    return false;
  found->second->set_shared(false);
  tiles_.erase(found);
  eviction_tiles_cache_valid_ = false;
  if (recycled_twin) {
    // Recycled twin does not also have a recycled twin, so pass NULL.
    recycled_twin->RemoveTileAt(i, j, NULL);
  }
  return true;
}

void PictureLayerTiling::Reset() {
  live_tiles_rect_ = gfx::Rect();
  PictureLayerTiling* recycled_twin = client_->GetRecycledTwinTiling(this);
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    it->second->set_shared(false);
    if (recycled_twin)
      recycled_twin->RemoveTileAt(it->first.first, it->first.second, NULL);
  }
  tiles_.clear();
  eviction_tiles_cache_valid_ = false;
}

gfx::Rect PictureLayerTiling::ComputeSkewport(
    double current_frame_time_in_seconds,
    const gfx::Rect& visible_rect_in_content_space) const {
  gfx::Rect skewport = visible_rect_in_content_space;
  if (last_impl_frame_time_in_seconds_ == 0.0)
    return skewport;

  double time_delta =
      current_frame_time_in_seconds - last_impl_frame_time_in_seconds_;
  if (time_delta == 0.0)
    return skewport;

  float skewport_target_time_in_seconds =
      client_->GetSkewportTargetTimeInSeconds();
  double extrapolation_multiplier =
      skewport_target_time_in_seconds / time_delta;

  int old_x = last_visible_rect_in_content_space_.x();
  int old_y = last_visible_rect_in_content_space_.y();
  int old_right = last_visible_rect_in_content_space_.right();
  int old_bottom = last_visible_rect_in_content_space_.bottom();

  int new_x = visible_rect_in_content_space.x();
  int new_y = visible_rect_in_content_space.y();
  int new_right = visible_rect_in_content_space.right();
  int new_bottom = visible_rect_in_content_space.bottom();

  int skewport_limit = client_->GetSkewportExtrapolationLimitInContentPixels();

  // Compute the maximum skewport based on |skewport_limit|.
  gfx::Rect max_skewport = skewport;
  max_skewport.Inset(
      -skewport_limit, -skewport_limit, -skewport_limit, -skewport_limit);

  // Inset the skewport by the needed adjustment.
  skewport.Inset(extrapolation_multiplier * (new_x - old_x),
                 extrapolation_multiplier * (new_y - old_y),
                 extrapolation_multiplier * (old_right - new_right),
                 extrapolation_multiplier * (old_bottom - new_bottom));

  // Clip the skewport to |max_skewport|.
  skewport.Intersect(max_skewport);

  // Finally, ensure that visible rect is contained in the skewport.
  skewport.Union(visible_rect_in_content_space);
  return skewport;
}

void PictureLayerTiling::ComputeTilePriorityRects(
    WhichTree tree,
    const gfx::Rect& viewport_in_layer_space,
    float ideal_contents_scale,
    double current_frame_time_in_seconds,
    const Occlusion& occlusion_in_layer_space) {
  if (!NeedsUpdateForFrameAtTimeAndViewport(current_frame_time_in_seconds,
                                            viewport_in_layer_space)) {
    // This should never be zero for the purposes of has_ever_been_updated().
    DCHECK_NE(current_frame_time_in_seconds, 0.0);
    return;
  }

  gfx::Rect visible_rect_in_content_space =
      gfx::ScaleToEnclosingRect(viewport_in_layer_space, contents_scale_);

  if (tiling_size().IsEmpty()) {
    last_impl_frame_time_in_seconds_ = current_frame_time_in_seconds;
    last_viewport_in_layer_space_ = viewport_in_layer_space;
    last_visible_rect_in_content_space_ = visible_rect_in_content_space;
    return;
  }

  // Calculate the skewport.
  gfx::Rect skewport = ComputeSkewport(current_frame_time_in_seconds,
                                       visible_rect_in_content_space);
  DCHECK(skewport.Contains(visible_rect_in_content_space));

  // Calculate the eventually/live tiles rect.
  size_t max_tiles_for_interest_area = client_->GetMaxTilesForInterestArea();

  gfx::Size tile_size = tiling_data_.max_texture_size();
  int64 eventually_rect_area =
      max_tiles_for_interest_area * tile_size.width() * tile_size.height();

  gfx::Rect eventually_rect =
      ExpandRectEquallyToAreaBoundedBy(visible_rect_in_content_space,
                                       eventually_rect_area,
                                       gfx::Rect(tiling_size()),
                                       &expansion_cache_);

  DCHECK(eventually_rect.IsEmpty() ||
         gfx::Rect(tiling_size()).Contains(eventually_rect))
      << "tiling_size: " << tiling_size().ToString()
      << " eventually_rect: " << eventually_rect.ToString();

  // Calculate the soon border rect.
  content_to_screen_scale_ = ideal_contents_scale / contents_scale_;
  gfx::Rect soon_border_rect = visible_rect_in_content_space;
  float border = kSoonBorderDistanceInScreenPixels / content_to_screen_scale_;
  soon_border_rect.Inset(-border, -border, -border, -border);

  // Update the tiling state.
  SetLiveTilesRect(eventually_rect);

  last_impl_frame_time_in_seconds_ = current_frame_time_in_seconds;
  last_viewport_in_layer_space_ = viewport_in_layer_space;
  last_visible_rect_in_content_space_ = visible_rect_in_content_space;

  eviction_tiles_cache_valid_ = false;

  current_visible_rect_ = visible_rect_in_content_space;
  current_skewport_rect_ = skewport;
  current_soon_border_rect_ = soon_border_rect;
  current_eventually_rect_ = eventually_rect;
  current_occlusion_in_layer_space_ = occlusion_in_layer_space;

  // Update has_*_tiles state.
  gfx::Rect tiling_rect(tiling_size());

  has_visible_rect_tiles_ = tiling_rect.Intersects(current_visible_rect_);
  has_skewport_rect_tiles_ = tiling_rect.Intersects(current_skewport_rect_);
  has_soon_border_rect_tiles_ =
      tiling_rect.Intersects(current_soon_border_rect_);
  has_eventually_rect_tiles_ = tiling_rect.Intersects(current_eventually_rect_);
}

void PictureLayerTiling::SetLiveTilesRect(
    const gfx::Rect& new_live_tiles_rect) {
  DCHECK(new_live_tiles_rect.IsEmpty() ||
         gfx::Rect(tiling_size()).Contains(new_live_tiles_rect))
      << "tiling_size: " << tiling_size().ToString()
      << " new_live_tiles_rect: " << new_live_tiles_rect.ToString();
  if (live_tiles_rect_ == new_live_tiles_rect)
    return;

  // Iterate to delete all tiles outside of our new live_tiles rect.
  PictureLayerTiling* recycled_twin = client_->GetRecycledTwinTiling(this);
  for (TilingData::DifferenceIterator iter(&tiling_data_,
                                           live_tiles_rect_,
                                           new_live_tiles_rect);
       iter;
       ++iter) {
    RemoveTileAt(iter.index_x(), iter.index_y(), recycled_twin);
  }

  const PictureLayerTiling* twin_tiling =
      client_->GetPendingOrActiveTwinTiling(this);

  // Iterate to allocate new tiles for all regions with newly exposed area.
  for (TilingData::DifferenceIterator iter(&tiling_data_,
                                           new_live_tiles_rect,
                                           live_tiles_rect_);
       iter;
       ++iter) {
    TileMapKey key(iter.index());
    CreateTile(key.first, key.second, twin_tiling);
  }

  live_tiles_rect_ = new_live_tiles_rect;
  VerifyLiveTilesRect();
}

void PictureLayerTiling::VerifyLiveTilesRect() {
#if DCHECK_IS_ON
  for (TileMap::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    if (!it->second.get())
      continue;
    DCHECK(it->first.first < tiling_data_.num_tiles_x())
        << this << " " << it->first.first << "," << it->first.second
        << " num_tiles_x " << tiling_data_.num_tiles_x() << " live_tiles_rect "
        << live_tiles_rect_.ToString();
    DCHECK(it->first.second < tiling_data_.num_tiles_y())
        << this << " " << it->first.first << "," << it->first.second
        << " num_tiles_y " << tiling_data_.num_tiles_y() << " live_tiles_rect "
        << live_tiles_rect_.ToString();
    DCHECK(tiling_data_.TileBounds(it->first.first, it->first.second)
               .Intersects(live_tiles_rect_))
        << this << " " << it->first.first << "," << it->first.second
        << " tile bounds "
        << tiling_data_.TileBounds(it->first.first, it->first.second).ToString()
        << " live_tiles_rect " << live_tiles_rect_.ToString();
  }
#endif
}

bool PictureLayerTiling::IsTileOccluded(const Tile* tile) const {
  DCHECK(tile);

  if (!current_occlusion_in_layer_space_.HasOcclusion())
    return false;

  gfx::Rect tile_query_rect =
      gfx::IntersectRects(tile->content_rect(), current_visible_rect_);

  // Explicitly check if the tile is outside the viewport. If so, we need to
  // return false, since occlusion for this tile is unknown.
  // TODO(vmpstr): Since the current visible rect is really a viewport in
  // layer space, we should probably clip tile query rect to tiling bounds
  // or live tiles rect.
  if (tile_query_rect.IsEmpty())
    return false;

  if (contents_scale_ != 1.f) {
    tile_query_rect =
        gfx::ScaleToEnclosingRect(tile_query_rect, 1.0f / contents_scale_);
  }

  return current_occlusion_in_layer_space_.IsOccluded(tile_query_rect);
}

bool PictureLayerTiling::IsTileRequiredForActivation(const Tile* tile) const {
  DCHECK_EQ(PENDING_TREE, client_->GetTree());

  // Note that although this function will determine whether tile is required
  // for activation assuming that it is in visible (ie in the viewport). That is
  // to say, even if the tile is outside of the viewport, it will be treated as
  // if it was inside (there are no explicit checks for this). Hence, this
  // function is only called for visible tiles to ensure we don't block
  // activation on tiles outside of the viewport.

  // If we are not allowed to mark tiles as required for activation, then don't
  // do it.
  if (!can_require_tiles_for_activation_)
    return false;

  if (resolution_ != HIGH_RESOLUTION)
    return false;

  if (IsTileOccluded(tile))
    return false;

  if (client_->RequiresHighResToDraw())
    return true;

  const PictureLayerTiling* twin_tiling =
      client_->GetPendingOrActiveTwinTiling(this);
  if (!twin_tiling)
    return true;

  if (twin_tiling->layer_bounds() != layer_bounds())
    return true;

  if (twin_tiling->current_visible_rect_ != current_visible_rect_)
    return true;

  Tile* twin_tile =
      twin_tiling->TileAt(tile->tiling_i_index(), tile->tiling_j_index());
  // If twin tile is missing, it might not have a recording, so we don't need
  // this tile to be required for activation.
  if (!twin_tile)
    return false;

  return true;
}

void PictureLayerTiling::UpdateTileAndTwinPriority(Tile* tile) const {
  UpdateTilePriority(tile);

  const PictureLayerTiling* twin_tiling =
      client_->GetPendingOrActiveTwinTiling(this);
  if (!tile->is_shared() || !twin_tiling) {
    WhichTree tree = client_->GetTree();
    WhichTree twin_tree = tree == ACTIVE_TREE ? PENDING_TREE : ACTIVE_TREE;
    tile->SetPriority(twin_tree, TilePriority());
    tile->set_is_occluded(twin_tree, false);
    if (twin_tree == PENDING_TREE)
      tile->set_required_for_activation(false);
    return;
  }

  twin_tiling->UpdateTilePriority(tile);
}

void PictureLayerTiling::UpdateTilePriority(Tile* tile) const {
  // TODO(vmpstr): This code should return the priority instead of setting it on
  // the tile. This should be a part of the change to move tile priority from
  // tiles into iterators.
  WhichTree tree = client_->GetTree();

  DCHECK_EQ(TileAt(tile->tiling_i_index(), tile->tiling_j_index()), tile);
  gfx::Rect tile_bounds =
      tiling_data_.TileBounds(tile->tiling_i_index(), tile->tiling_j_index());

  if (current_visible_rect_.Intersects(tile_bounds)) {
    tile->SetPriority(tree, TilePriority(resolution_, TilePriority::NOW, 0));
    if (tree == PENDING_TREE)
      tile->set_required_for_activation(IsTileRequiredForActivation(tile));
    tile->set_is_occluded(tree, IsTileOccluded(tile));
    return;
  }

  if (tree == PENDING_TREE)
    tile->set_required_for_activation(false);
  tile->set_is_occluded(tree, false);

  DCHECK_GT(content_to_screen_scale_, 0.f);
  float distance_to_visible =
      current_visible_rect_.ManhattanInternalDistance(tile_bounds) *
      content_to_screen_scale_;

  if (current_soon_border_rect_.Intersects(tile_bounds) ||
      current_skewport_rect_.Intersects(tile_bounds)) {
    tile->SetPriority(
        tree,
        TilePriority(resolution_, TilePriority::SOON, distance_to_visible));
    return;
  }

  tile->SetPriority(
      tree,
      TilePriority(resolution_, TilePriority::EVENTUALLY, distance_to_visible));
}

void PictureLayerTiling::GetAllTilesForTracing(
    std::set<const Tile*>* tiles) const {
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it)
    tiles->insert(it->second.get());
}

void PictureLayerTiling::AsValueInto(base::debug::TracedValue* state) const {
  state->SetInteger("num_tiles", tiles_.size());
  state->SetDouble("content_scale", contents_scale_);
  state->BeginDictionary("tiling_size");
  MathUtil::AddToTracedValue(tiling_size(), state);
  state->EndDictionary();
}

size_t PictureLayerTiling::GPUMemoryUsageInBytes() const {
  size_t amount = 0;
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    const Tile* tile = it->second.get();
    amount += tile->GPUMemoryUsageInBytes();
  }
  return amount;
}

PictureLayerTiling::RectExpansionCache::RectExpansionCache()
  : previous_target(0) {
}

namespace {

// This struct represents an event at which the expending rect intersects
// one of its boundaries.  4 intersection events will occur during expansion.
struct EdgeEvent {
  enum { BOTTOM, TOP, LEFT, RIGHT } edge;
  int* num_edges;
  int distance;
};

// Compute the delta to expand from edges to cover target_area.
int ComputeExpansionDelta(int num_x_edges, int num_y_edges,
                          int width, int height,
                          int64 target_area) {
  // Compute coefficients for the quadratic equation:
  //   a*x^2 + b*x + c = 0
  int a = num_y_edges * num_x_edges;
  int b = num_y_edges * width + num_x_edges * height;
  int64 c = static_cast<int64>(width) * height - target_area;

  // Compute the delta for our edges using the quadratic equation.
  int delta =
      (a == 0) ? -c / b : (-b + static_cast<int>(std::sqrt(
                                    static_cast<int64>(b) * b - 4.0 * a * c))) /
                              (2 * a);
  return std::max(0, delta);
}

}  // namespace

gfx::Rect PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
    const gfx::Rect& starting_rect,
    int64 target_area,
    const gfx::Rect& bounding_rect,
    RectExpansionCache* cache) {
  if (starting_rect.IsEmpty())
    return starting_rect;

  if (cache &&
      cache->previous_start == starting_rect &&
      cache->previous_bounds == bounding_rect &&
      cache->previous_target == target_area)
    return cache->previous_result;

  if (cache) {
    cache->previous_start = starting_rect;
    cache->previous_bounds = bounding_rect;
    cache->previous_target = target_area;
  }

  DCHECK(!bounding_rect.IsEmpty());
  DCHECK_GT(target_area, 0);

  // Expand the starting rect to cover target_area, if it is smaller than it.
  int delta = ComputeExpansionDelta(
      2, 2, starting_rect.width(), starting_rect.height(), target_area);
  gfx::Rect expanded_starting_rect = starting_rect;
  if (delta > 0)
    expanded_starting_rect.Inset(-delta, -delta);

  gfx::Rect rect = IntersectRects(expanded_starting_rect, bounding_rect);
  if (rect.IsEmpty()) {
    // The starting_rect and bounding_rect are far away.
    if (cache)
      cache->previous_result = rect;
    return rect;
  }
  if (delta >= 0 && rect == expanded_starting_rect) {
    // The starting rect already covers the entire bounding_rect and isn't too
    // large for the target_area.
    if (cache)
      cache->previous_result = rect;
    return rect;
  }

  // Continue to expand/shrink rect to let it cover target_area.

  // These values will be updated by the loop and uses as the output.
  int origin_x = rect.x();
  int origin_y = rect.y();
  int width = rect.width();
  int height = rect.height();

  // In the beginning we will consider 2 edges in each dimension.
  int num_y_edges = 2;
  int num_x_edges = 2;

  // Create an event list.
  EdgeEvent events[] = {
    { EdgeEvent::BOTTOM, &num_y_edges, rect.y() - bounding_rect.y() },
    { EdgeEvent::TOP, &num_y_edges, bounding_rect.bottom() - rect.bottom() },
    { EdgeEvent::LEFT, &num_x_edges, rect.x() - bounding_rect.x() },
    { EdgeEvent::RIGHT, &num_x_edges, bounding_rect.right() - rect.right() }
  };

  // Sort the events by distance (closest first).
  if (events[0].distance > events[1].distance) std::swap(events[0], events[1]);
  if (events[2].distance > events[3].distance) std::swap(events[2], events[3]);
  if (events[0].distance > events[2].distance) std::swap(events[0], events[2]);
  if (events[1].distance > events[3].distance) std::swap(events[1], events[3]);
  if (events[1].distance > events[2].distance) std::swap(events[1], events[2]);

  for (int event_index = 0; event_index < 4; event_index++) {
    const EdgeEvent& event = events[event_index];

    int delta = ComputeExpansionDelta(
        num_x_edges, num_y_edges, width, height, target_area);

    // Clamp delta to our event distance.
    if (delta > event.distance)
      delta = event.distance;

    // Adjust the edge count for this kind of edge.
    --*event.num_edges;

    // Apply the delta to the edges and edge events.
    for (int i = event_index; i < 4; i++) {
      switch (events[i].edge) {
        case EdgeEvent::BOTTOM:
            origin_y -= delta;
            height += delta;
            break;
        case EdgeEvent::TOP:
            height += delta;
            break;
        case EdgeEvent::LEFT:
            origin_x -= delta;
            width += delta;
            break;
        case EdgeEvent::RIGHT:
            width += delta;
            break;
      }
      events[i].distance -= delta;
    }

    // If our delta is less then our event distance, we're done.
    if (delta < event.distance)
      break;
  }

  gfx::Rect result(origin_x, origin_y, width, height);
  if (cache)
    cache->previous_result = result;
  return result;
}

void PictureLayerTiling::UpdateEvictionCacheIfNeeded(
    TreePriority tree_priority) {
  if (eviction_tiles_cache_valid_ &&
      eviction_cache_tree_priority_ == tree_priority)
    return;

  eviction_tiles_now_.clear();
  eviction_tiles_now_and_required_for_activation_.clear();
  eviction_tiles_soon_.clear();
  eviction_tiles_soon_and_required_for_activation_.clear();
  eviction_tiles_eventually_.clear();
  eviction_tiles_eventually_and_required_for_activation_.clear();

  for (TileMap::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = it->second.get();
    UpdateTileAndTwinPriority(tile);
    const TilePriority& priority =
        tile->priority_for_tree_priority(tree_priority);
    switch (priority.priority_bin) {
      case TilePriority::EVENTUALLY:
        if (tile->required_for_activation())
          eviction_tiles_eventually_and_required_for_activation_.push_back(
              tile);
        else
          eviction_tiles_eventually_.push_back(tile);
        break;
      case TilePriority::SOON:
        if (tile->required_for_activation())
          eviction_tiles_soon_and_required_for_activation_.push_back(tile);
        else
          eviction_tiles_soon_.push_back(tile);
        break;
      case TilePriority::NOW:
        if (tile->required_for_activation())
          eviction_tiles_now_and_required_for_activation_.push_back(tile);
        else
          eviction_tiles_now_.push_back(tile);
        break;
    }
  }

  // TODO(vmpstr): Do this lazily. One option is to have a "sorted" flag that
  // can be updated for each of the queues.
  TileEvictionOrder sort_order(tree_priority);
  std::sort(eviction_tiles_now_.begin(), eviction_tiles_now_.end(), sort_order);
  std::sort(eviction_tiles_now_and_required_for_activation_.begin(),
            eviction_tiles_now_and_required_for_activation_.end(),
            sort_order);
  std::sort(
      eviction_tiles_soon_.begin(), eviction_tiles_soon_.end(), sort_order);
  std::sort(eviction_tiles_soon_and_required_for_activation_.begin(),
            eviction_tiles_soon_and_required_for_activation_.end(),
            sort_order);
  std::sort(eviction_tiles_eventually_.begin(),
            eviction_tiles_eventually_.end(),
            sort_order);
  std::sort(eviction_tiles_eventually_and_required_for_activation_.begin(),
            eviction_tiles_eventually_and_required_for_activation_.end(),
            sort_order);

  eviction_tiles_cache_valid_ = true;
  eviction_cache_tree_priority_ = tree_priority;
}

const std::vector<Tile*>* PictureLayerTiling::GetEvictionTiles(
    TreePriority tree_priority,
    EvictionCategory category) {
  UpdateEvictionCacheIfNeeded(tree_priority);
  switch (category) {
    case EVENTUALLY:
      return &eviction_tiles_eventually_;
    case EVENTUALLY_AND_REQUIRED_FOR_ACTIVATION:
      return &eviction_tiles_eventually_and_required_for_activation_;
    case SOON:
      return &eviction_tiles_soon_;
    case SOON_AND_REQUIRED_FOR_ACTIVATION:
      return &eviction_tiles_soon_and_required_for_activation_;
    case NOW:
      return &eviction_tiles_now_;
    case NOW_AND_REQUIRED_FOR_ACTIVATION:
      return &eviction_tiles_now_and_required_for_activation_;
  }
  NOTREACHED();
  return &eviction_tiles_eventually_;
}

PictureLayerTiling::TilingRasterTileIterator::TilingRasterTileIterator()
    : tiling_(NULL), current_tile_(NULL) {}

PictureLayerTiling::TilingRasterTileIterator::TilingRasterTileIterator(
    PictureLayerTiling* tiling)
    : tiling_(tiling), phase_(VISIBLE_RECT), current_tile_(NULL) {
  if (!tiling_->has_visible_rect_tiles_) {
    AdvancePhase();
    return;
  }

  visible_iterator_ = TilingData::Iterator(&tiling_->tiling_data_,
                                           tiling_->current_visible_rect_,
                                           false /* include_borders */);
  if (!visible_iterator_) {
    AdvancePhase();
    return;
  }

  current_tile_ =
      tiling_->TileAt(visible_iterator_.index_x(), visible_iterator_.index_y());
  if (!current_tile_ || !TileNeedsRaster(current_tile_)) {
    ++(*this);
    return;
  }
  tiling_->UpdateTileAndTwinPriority(current_tile_);
}

PictureLayerTiling::TilingRasterTileIterator::~TilingRasterTileIterator() {}

void PictureLayerTiling::TilingRasterTileIterator::AdvancePhase() {
  DCHECK_LT(phase_, EVENTUALLY_RECT);

  do {
    phase_ = static_cast<Phase>(phase_ + 1);
    switch (phase_) {
      case VISIBLE_RECT:
        NOTREACHED();
        return;
      case SKEWPORT_RECT:
        if (!tiling_->has_skewport_rect_tiles_)
          continue;

        spiral_iterator_ = TilingData::SpiralDifferenceIterator(
            &tiling_->tiling_data_,
            tiling_->current_skewport_rect_,
            tiling_->current_visible_rect_,
            tiling_->current_visible_rect_);
        break;
      case SOON_BORDER_RECT:
        if (!tiling_->has_soon_border_rect_tiles_)
          continue;

        spiral_iterator_ = TilingData::SpiralDifferenceIterator(
            &tiling_->tiling_data_,
            tiling_->current_soon_border_rect_,
            tiling_->current_skewport_rect_,
            tiling_->current_visible_rect_);
        break;
      case EVENTUALLY_RECT:
        if (!tiling_->has_eventually_rect_tiles_) {
          current_tile_ = NULL;
          return;
        }

        spiral_iterator_ = TilingData::SpiralDifferenceIterator(
            &tiling_->tiling_data_,
            tiling_->current_eventually_rect_,
            tiling_->current_skewport_rect_,
            tiling_->current_soon_border_rect_);
        break;
    }

    while (spiral_iterator_) {
      current_tile_ = tiling_->TileAt(spiral_iterator_.index_x(),
                                      spiral_iterator_.index_y());
      if (current_tile_ && TileNeedsRaster(current_tile_))
        break;
      ++spiral_iterator_;
    }

    if (!spiral_iterator_ && phase_ == EVENTUALLY_RECT) {
      current_tile_ = NULL;
      break;
    }
  } while (!spiral_iterator_);

  if (current_tile_)
    tiling_->UpdateTileAndTwinPriority(current_tile_);
}

PictureLayerTiling::TilingRasterTileIterator&
PictureLayerTiling::TilingRasterTileIterator::
operator++() {
  current_tile_ = NULL;
  while (!current_tile_ || !TileNeedsRaster(current_tile_)) {
    std::pair<int, int> next_index;
    switch (phase_) {
      case VISIBLE_RECT:
        ++visible_iterator_;
        if (!visible_iterator_) {
          AdvancePhase();
          return *this;
        }
        next_index = visible_iterator_.index();
        break;
      case SKEWPORT_RECT:
      case SOON_BORDER_RECT:
        ++spiral_iterator_;
        if (!spiral_iterator_) {
          AdvancePhase();
          return *this;
        }
        next_index = spiral_iterator_.index();
        break;
      case EVENTUALLY_RECT:
        ++spiral_iterator_;
        if (!spiral_iterator_) {
          current_tile_ = NULL;
          return *this;
        }
        next_index = spiral_iterator_.index();
        break;
    }
    current_tile_ = tiling_->TileAt(next_index.first, next_index.second);
  }

  if (current_tile_)
    tiling_->UpdateTileAndTwinPriority(current_tile_);
  return *this;
}

PictureLayerTiling::TilingEvictionTileIterator::TilingEvictionTileIterator()
    : eviction_tiles_(NULL), current_eviction_tiles_index_(0u) {
}

PictureLayerTiling::TilingEvictionTileIterator::TilingEvictionTileIterator(
    PictureLayerTiling* tiling,
    TreePriority tree_priority,
    EvictionCategory category)
    : eviction_tiles_(tiling->GetEvictionTiles(tree_priority, category)),
      // Note: initializing to "0 - 1" works as overflow is well defined for
      // unsigned integers.
      current_eviction_tiles_index_(static_cast<size_t>(0) - 1) {
  DCHECK(eviction_tiles_);
  ++(*this);
}

PictureLayerTiling::TilingEvictionTileIterator::~TilingEvictionTileIterator() {
}

PictureLayerTiling::TilingEvictionTileIterator::operator bool() const {
  return eviction_tiles_ &&
         current_eviction_tiles_index_ != eviction_tiles_->size();
}

Tile* PictureLayerTiling::TilingEvictionTileIterator::operator*() {
  DCHECK(*this);
  return (*eviction_tiles_)[current_eviction_tiles_index_];
}

const Tile* PictureLayerTiling::TilingEvictionTileIterator::operator*() const {
  DCHECK(*this);
  return (*eviction_tiles_)[current_eviction_tiles_index_];
}

PictureLayerTiling::TilingEvictionTileIterator&
PictureLayerTiling::TilingEvictionTileIterator::
operator++() {
  DCHECK(*this);
  do {
    ++current_eviction_tiles_index_;
  } while (current_eviction_tiles_index_ != eviction_tiles_->size() &&
           !(*eviction_tiles_)[current_eviction_tiles_index_]->HasResources());

  return *this;
}

}  // namespace cc
