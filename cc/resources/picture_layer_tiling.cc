// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_layer_tiling.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {
namespace {

const float kSoonBorderDistanceViewportPercentage = 0.15f;
const float kMaxSoonBorderDistanceInScreenPixels = 312.f;

}  // namespace

scoped_ptr<PictureLayerTiling> PictureLayerTiling::Create(
    float contents_scale,
    scoped_refptr<RasterSource> raster_source,
    PictureLayerTilingClient* client,
    size_t max_tiles_for_interest_area,
    float skewport_target_time_in_seconds,
    int skewport_extrapolation_limit_in_content_pixels) {
  return make_scoped_ptr(new PictureLayerTiling(
      contents_scale, raster_source, client, max_tiles_for_interest_area,
      skewport_target_time_in_seconds,
      skewport_extrapolation_limit_in_content_pixels));
}

PictureLayerTiling::PictureLayerTiling(
    float contents_scale,
    scoped_refptr<RasterSource> raster_source,
    PictureLayerTilingClient* client,
    size_t max_tiles_for_interest_area,
    float skewport_target_time_in_seconds,
    int skewport_extrapolation_limit_in_content_pixels)
    : max_tiles_for_interest_area_(max_tiles_for_interest_area),
      skewport_target_time_in_seconds_(skewport_target_time_in_seconds),
      skewport_extrapolation_limit_in_content_pixels_(
          skewport_extrapolation_limit_in_content_pixels),
      contents_scale_(contents_scale),
      client_(client),
      raster_source_(raster_source),
      resolution_(NON_IDEAL_RESOLUTION),
      tiling_data_(gfx::Size(), gfx::Size(), kBorderTexels),
      can_require_tiles_for_activation_(false),
      current_content_to_screen_scale_(0.f),
      has_visible_rect_tiles_(false),
      has_skewport_rect_tiles_(false),
      has_soon_border_rect_tiles_(false),
      has_eventually_rect_tiles_(false) {
  DCHECK(!raster_source->IsSolidColor());
  gfx::Size content_bounds = gfx::ToCeiledSize(
      gfx::ScaleSize(raster_source_->GetSize(), contents_scale));
  gfx::Size tile_size = client_->CalculateTileSize(content_bounds);

  DCHECK(!gfx::ToFlooredSize(gfx::ScaleSize(raster_source_->GetSize(),
                                            contents_scale)).IsEmpty())
      << "Tiling created with scale too small as contents become empty."
      << " Layer bounds: " << raster_source_->GetSize().ToString()
      << " Contents scale: " << contents_scale;

  tiling_data_.SetTilingSize(content_bounds);
  tiling_data_.SetMaxTextureSize(tile_size);
}

PictureLayerTiling::~PictureLayerTiling() {
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it)
    it->second->set_shared(false);
}

// static
float PictureLayerTiling::CalculateSoonBorderDistance(
    const gfx::Rect& visible_rect_in_content_space,
    float content_to_screen_scale) {
  float max_dimension = std::max(visible_rect_in_content_space.width(),
                                 visible_rect_in_content_space.height());
  return std::min(
      kMaxSoonBorderDistanceInScreenPixels / content_to_screen_scale,
      max_dimension * kSoonBorderDistanceViewportPercentage);
}

Tile* PictureLayerTiling::CreateTile(int i,
                                     int j,
                                     const PictureLayerTiling* twin_tiling,
                                     PictureLayerTiling* recycled_twin) {
  // Can't have both a (pending or active) twin and a recycled twin tiling.
  DCHECK_IMPLIES(twin_tiling, !recycled_twin);
  DCHECK_IMPLIES(recycled_twin, !twin_tiling);
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

  if (!raster_source_->CoversRect(tile_rect, contents_scale_))
    return nullptr;

  // Create a new tile because our twin didn't have a valid one.
  scoped_refptr<Tile> tile = client_->CreateTile(contents_scale_, tile_rect);
  DCHECK(!tile->is_shared());
  tile->set_tiling_index(i, j);
  tiles_[key] = tile;

  if (recycled_twin) {
    DCHECK(recycled_twin->tiles_.find(key) == recycled_twin->tiles_.end());
    // Do what recycled_twin->CreateTile() would do.
    tile->set_shared(true);
    recycled_twin->tiles_[key] = tile;
  }
  return tile.get();
}

void PictureLayerTiling::CreateMissingTilesInLiveTilesRect() {
  const PictureLayerTiling* twin_tiling =
      client_->GetPendingOrActiveTwinTiling(this);
  // There is no recycled twin during commit from the main thread which is when
  // this occurs.
  PictureLayerTiling* null_recycled_twin = nullptr;
  DCHECK_EQ(null_recycled_twin, client_->GetRecycledTwinTiling(this));
  bool include_borders = false;
  for (TilingData::Iterator iter(
           &tiling_data_, live_tiles_rect_, include_borders);
       iter;
       ++iter) {
    TileMapKey key = iter.index();
    TileMap::iterator find = tiles_.find(key);
    if (find != tiles_.end())
      continue;
    CreateTile(key.first, key.second, twin_tiling, null_recycled_twin);
  }

  VerifyLiveTilesRect(false);
}

void PictureLayerTiling::CloneTilesAndPropertiesFrom(
    const PictureLayerTiling& twin_tiling) {
  DCHECK_EQ(&twin_tiling, client_->GetPendingOrActiveTwinTiling(this));

  SetRasterSourceAndResize(twin_tiling.raster_source_);
  DCHECK_EQ(twin_tiling.contents_scale_, contents_scale_);
  DCHECK_EQ(twin_tiling.raster_source_, raster_source_);
  DCHECK_EQ(twin_tiling.tile_size().ToString(), tile_size().ToString());

  resolution_ = twin_tiling.resolution_;

  SetLiveTilesRect(twin_tiling.live_tiles_rect());

  // Recreate unshared tiles.
  std::vector<TileMapKey> to_remove;
  for (const auto& tile_map_pair : tiles_) {
    TileMapKey key = tile_map_pair.first;
    Tile* tile = tile_map_pair.second.get();
    if (!tile->is_shared())
      to_remove.push_back(key);
  }
  // The recycled twin does not exist since there is a pending twin (which is
  // |twin_tiling|).
  PictureLayerTiling* null_recycled_twin = nullptr;
  DCHECK_EQ(null_recycled_twin, client_->GetRecycledTwinTiling(this));
  for (const auto& key : to_remove) {
    RemoveTileAt(key.first, key.second, null_recycled_twin);
    CreateTile(key.first, key.second, &twin_tiling, null_recycled_twin);
  }

  // Create any missing tiles from the |twin_tiling|.
  for (const auto& tile_map_pair : twin_tiling.tiles_) {
    TileMapKey key = tile_map_pair.first;
    Tile* tile = tile_map_pair.second.get();
    if (!tile->is_shared())
      CreateTile(key.first, key.second, &twin_tiling, null_recycled_twin);
  }

  DCHECK_EQ(twin_tiling.tiles_.size(), tiles_.size());
#if DCHECK_IS_ON()
  for (const auto& tile_map_pair : tiles_)
    DCHECK(tile_map_pair.second->is_shared());
  VerifyLiveTilesRect(false);
#endif

  UpdateTilePriorityRects(twin_tiling.current_content_to_screen_scale_,
                          twin_tiling.current_visible_rect_,
                          twin_tiling.current_skewport_rect_,
                          twin_tiling.current_soon_border_rect_,
                          twin_tiling.current_eventually_rect_,
                          twin_tiling.current_occlusion_in_layer_space_);
}

void PictureLayerTiling::SetRasterSourceAndResize(
    scoped_refptr<RasterSource> raster_source) {
  DCHECK(!raster_source->IsSolidColor());
  gfx::Size old_layer_bounds = raster_source_->GetSize();
  raster_source_.swap(raster_source);
  gfx::Size new_layer_bounds = raster_source_->GetSize();
  gfx::Size content_bounds =
      gfx::ToCeiledSize(gfx::ScaleSize(new_layer_bounds, contents_scale_));
  gfx::Size tile_size = client_->CalculateTileSize(content_bounds);

  if (tile_size != tiling_data_.max_texture_size()) {
    tiling_data_.SetTilingSize(content_bounds);
    tiling_data_.SetMaxTextureSize(tile_size);
    // When the tile size changes, the TilingData positions no longer work
    // as valid keys to the TileMap, so just drop all tiles and clear the live
    // tiles rect.
    Reset();
    return;
  }

  if (old_layer_bounds == new_layer_bounds)
    return;

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

  // There is no recycled twin since this is run on the pending tiling
  // during commit, and on the active tree during activate.
  PictureLayerTiling* null_recycled_twin = nullptr;
  DCHECK_EQ(null_recycled_twin, client_->GetRecycledTwinTiling(this));

  // Drop tiles outside the new layer bounds if the layer shrank.
  for (int i = after_right + 1; i <= before_right; ++i) {
    for (int j = before_top; j <= before_bottom; ++j)
      RemoveTileAt(i, j, null_recycled_twin);
  }
  for (int i = before_left; i <= after_right; ++i) {
    for (int j = after_bottom + 1; j <= before_bottom; ++j)
      RemoveTileAt(i, j, null_recycled_twin);
  }

  // If the layer grew, the live_tiles_rect_ is not changed, but a new row
  // and/or column of tiles may now exist inside the same live_tiles_rect_.
  const PictureLayerTiling* twin_tiling =
      client_->GetPendingOrActiveTwinTiling(this);
  if (after_right > before_right) {
    DCHECK_EQ(after_right, before_right + 1);
    for (int j = before_top; j <= after_bottom; ++j)
      CreateTile(after_right, j, twin_tiling, null_recycled_twin);
  }
  if (after_bottom > before_bottom) {
    DCHECK_EQ(after_bottom, before_bottom + 1);
    for (int i = before_left; i <= before_right; ++i)
      CreateTile(i, after_bottom, twin_tiling, null_recycled_twin);
  }
}

void PictureLayerTiling::Invalidate(const Region& layer_invalidation) {
  if (live_tiles_rect_.IsEmpty())
    return;
  std::vector<TileMapKey> new_tile_keys;
  gfx::Rect expanded_live_tiles_rect =
      tiling_data_.ExpandRectIgnoringBordersToTileBounds(live_tiles_rect_);
  for (Region::Iterator iter(layer_invalidation); iter.has_rect();
       iter.next()) {
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
      // There is no recycled twin for the pending tree during commit, or for
      // the active tree during activation.
      PictureLayerTiling* null_recycled_twin = nullptr;
      DCHECK_EQ(null_recycled_twin, client_->GetRecycledTwinTiling(this));
      if (RemoveTileAt(iter.index_x(), iter.index_y(), null_recycled_twin))
        new_tile_keys.push_back(iter.index());
    }
  }

  if (!new_tile_keys.empty()) {
    // During commit from the main thread, invalidations can never be shared
    // with the active tree since the active tree has different content there.
    // And when invalidating an active-tree tiling, it means there was no
    // pending tiling to clone from.
    const PictureLayerTiling* null_twin_tiling = nullptr;
    PictureLayerTiling* null_recycled_twin = nullptr;
    DCHECK_EQ(null_recycled_twin, client_->GetRecycledTwinTiling(this));
    for (size_t i = 0; i < new_tile_keys.size(); ++i) {
      CreateTile(new_tile_keys[i].first, new_tile_keys[i].second,
                 null_twin_tiling, null_recycled_twin);
    }
  }
}

void PictureLayerTiling::SetRasterSourceOnTiles() {
  // Shared (ie. non-invalidated) tiles on the pending tree are updated to use
  // the new raster source. When this raster source is activated, the raster
  // source will remain valid for shared tiles in the active tree.
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it)
    it->second->set_raster_source(raster_source_);
  VerifyLiveTilesRect(false);
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

bool PictureLayerTiling::RemoveTileAt(int i,
                                      int j,
                                      PictureLayerTiling* recycled_twin) {
  TileMap::iterator found = tiles_.find(TileMapKey(i, j));
  if (found == tiles_.end())
    return false;
  found->second->set_shared(false);
  tiles_.erase(found);
  if (recycled_twin) {
    // Recycled twin does not also have a recycled twin, so pass null.
    recycled_twin->RemoveTileAt(i, j, nullptr);
  }
  return true;
}

void PictureLayerTiling::Reset() {
  live_tiles_rect_ = gfx::Rect();
  PictureLayerTiling* recycled_twin = client_->GetRecycledTwinTiling(this);
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    it->second->set_shared(false);
    if (recycled_twin)
      recycled_twin->RemoveTileAt(it->first.first, it->first.second, nullptr);
  }
  tiles_.clear();
}

gfx::Rect PictureLayerTiling::ComputeSkewport(
    double current_frame_time_in_seconds,
    const gfx::Rect& visible_rect_in_content_space) const {
  gfx::Rect skewport = visible_rect_in_content_space;
  if (skewport.IsEmpty())
    return skewport;

  if (visible_rect_history_[1].frame_time_in_seconds == 0.0)
    return skewport;

  double time_delta = current_frame_time_in_seconds -
                      visible_rect_history_[1].frame_time_in_seconds;
  if (time_delta == 0.0)
    return skewport;

  double extrapolation_multiplier =
      skewport_target_time_in_seconds_ / time_delta;

  int old_x = visible_rect_history_[1].visible_rect_in_content_space.x();
  int old_y = visible_rect_history_[1].visible_rect_in_content_space.y();
  int old_right =
      visible_rect_history_[1].visible_rect_in_content_space.right();
  int old_bottom =
      visible_rect_history_[1].visible_rect_in_content_space.bottom();

  int new_x = visible_rect_in_content_space.x();
  int new_y = visible_rect_in_content_space.y();
  int new_right = visible_rect_in_content_space.right();
  int new_bottom = visible_rect_in_content_space.bottom();

  // Compute the maximum skewport based on
  // |skewport_extrapolation_limit_in_content_pixels_|.
  gfx::Rect max_skewport = skewport;
  max_skewport.Inset(-skewport_extrapolation_limit_in_content_pixels_,
                     -skewport_extrapolation_limit_in_content_pixels_);

  // Inset the skewport by the needed adjustment.
  skewport.Inset(extrapolation_multiplier * (new_x - old_x),
                 extrapolation_multiplier * (new_y - old_y),
                 extrapolation_multiplier * (old_right - new_right),
                 extrapolation_multiplier * (old_bottom - new_bottom));

  // Ensure that visible rect is contained in the skewport.
  skewport.Union(visible_rect_in_content_space);

  // Clip the skewport to |max_skewport|. This needs to happen after the
  // union in case intersecting would have left the empty rect.
  skewport.Intersect(max_skewport);

  return skewport;
}

bool PictureLayerTiling::ComputeTilePriorityRects(
    const gfx::Rect& viewport_in_layer_space,
    float ideal_contents_scale,
    double current_frame_time_in_seconds,
    const Occlusion& occlusion_in_layer_space) {
  if (!NeedsUpdateForFrameAtTimeAndViewport(current_frame_time_in_seconds,
                                            viewport_in_layer_space)) {
    // This should never be zero for the purposes of has_ever_been_updated().
    DCHECK_NE(current_frame_time_in_seconds, 0.0);
    return false;
  }

  gfx::Rect visible_rect_in_content_space =
      gfx::ScaleToEnclosingRect(viewport_in_layer_space, contents_scale_);

  if (tiling_size().IsEmpty()) {
    UpdateVisibleRectHistory(current_frame_time_in_seconds,
                             visible_rect_in_content_space);
    last_viewport_in_layer_space_ = viewport_in_layer_space;
    return false;
  }

  // Calculate the skewport.
  gfx::Rect skewport = ComputeSkewport(current_frame_time_in_seconds,
                                       visible_rect_in_content_space);
  DCHECK(skewport.Contains(visible_rect_in_content_space));

  // Calculate the eventually/live tiles rect.
  gfx::Size tile_size = tiling_data_.max_texture_size();
  int64 eventually_rect_area =
      max_tiles_for_interest_area_ * tile_size.width() * tile_size.height();

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
  float content_to_screen_scale = ideal_contents_scale / contents_scale_;
  gfx::Rect soon_border_rect = visible_rect_in_content_space;
  float border = CalculateSoonBorderDistance(visible_rect_in_content_space,
                                             content_to_screen_scale);
  soon_border_rect.Inset(-border, -border, -border, -border);

  UpdateVisibleRectHistory(current_frame_time_in_seconds,
                           visible_rect_in_content_space);
  last_viewport_in_layer_space_ = viewport_in_layer_space;

  SetLiveTilesRect(eventually_rect);
  UpdateTilePriorityRects(
      content_to_screen_scale, visible_rect_in_content_space, skewport,
      soon_border_rect, eventually_rect, occlusion_in_layer_space);
  return true;
}

void PictureLayerTiling::UpdateTilePriorityRects(
    float content_to_screen_scale,
    const gfx::Rect& visible_rect_in_content_space,
    const gfx::Rect& skewport,
    const gfx::Rect& soon_border_rect,
    const gfx::Rect& eventually_rect,
    const Occlusion& occlusion_in_layer_space) {
  current_visible_rect_ = visible_rect_in_content_space;
  current_skewport_rect_ = skewport;
  current_soon_border_rect_ = soon_border_rect;
  current_eventually_rect_ = eventually_rect;
  current_occlusion_in_layer_space_ = occlusion_in_layer_space;
  current_content_to_screen_scale_ = content_to_screen_scale;

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

  PictureLayerTiling* recycled_twin = client_->GetRecycledTwinTiling(this);

  // Iterate to delete all tiles outside of our new live_tiles rect.
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
    CreateTile(key.first, key.second, twin_tiling, recycled_twin);
  }

  live_tiles_rect_ = new_live_tiles_rect;
  VerifyLiveTilesRect(false);
  if (recycled_twin) {
    recycled_twin->live_tiles_rect_ = live_tiles_rect_;
    recycled_twin->VerifyLiveTilesRect(true);
  }
}

void PictureLayerTiling::VerifyLiveTilesRect(bool is_on_recycle_tree) const {
#if DCHECK_IS_ON()
  for (auto it = tiles_.begin(); it != tiles_.end(); ++it) {
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
    DCHECK_IMPLIES(is_on_recycle_tree, it->second->is_shared());
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

bool PictureLayerTiling::IsTileRequiredForActivationIfVisible(
    const Tile* tile) const {
  DCHECK_EQ(PENDING_TREE, client_->GetTree());

  // This function assumes that the tile is visible (i.e. in the viewport).  The
  // caller needs to make sure that this condition is met to ensure we don't
  // block activation on tiles outside of the viewport.

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

  if (twin_tiling->raster_source()->GetSize() != raster_source()->GetSize())
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

bool PictureLayerTiling::IsTileRequiredForDrawIfVisible(
    const Tile* tile) const {
  DCHECK_EQ(ACTIVE_TREE, client_->GetTree());

  // This function assumes that the tile is visible (i.e. in the viewport).

  if (resolution_ != HIGH_RESOLUTION)
    return false;

  if (IsTileOccluded(tile))
    return false;

  return true;
}

void PictureLayerTiling::UpdateTileAndTwinPriority(Tile* tile) const {
  WhichTree tree = client_->GetTree();
  WhichTree twin_tree = tree == ACTIVE_TREE ? PENDING_TREE : ACTIVE_TREE;

  UpdateTilePriorityForTree(tile, tree);

  const PictureLayerTiling* twin_tiling =
      client_->GetPendingOrActiveTwinTiling(this);
  if (!tile->is_shared() || !twin_tiling) {
    tile->SetPriority(twin_tree, TilePriority());
    tile->set_is_occluded(twin_tree, false);
    if (twin_tree == PENDING_TREE)
      tile->set_required_for_activation(false);
    else
      tile->set_required_for_draw(false);
    return;
  }

  twin_tiling->UpdateTilePriorityForTree(tile, twin_tree);
}

void PictureLayerTiling::UpdateTilePriorityForTree(Tile* tile,
                                                   WhichTree tree) const {
  // TODO(vmpstr): This code should return the priority instead of setting it on
  // the tile. This should be a part of the change to move tile priority from
  // tiles into iterators.
  TilePriority::PriorityBin max_tile_priority_bin =
      client_->GetMaxTilePriorityBin();

  DCHECK_EQ(TileAt(tile->tiling_i_index(), tile->tiling_j_index()), tile);
  gfx::Rect tile_bounds =
      tiling_data_.TileBounds(tile->tiling_i_index(), tile->tiling_j_index());

  if (max_tile_priority_bin <= TilePriority::NOW &&
      current_visible_rect_.Intersects(tile_bounds)) {
    tile->SetPriority(tree, TilePriority(resolution_, TilePriority::NOW, 0));
    if (tree == PENDING_TREE) {
      tile->set_required_for_activation(
          IsTileRequiredForActivationIfVisible(tile));
    } else {
      tile->set_required_for_draw(IsTileRequiredForDrawIfVisible(tile));
    }
    tile->set_is_occluded(tree, IsTileOccluded(tile));
    return;
  }

  if (tree == PENDING_TREE)
    tile->set_required_for_activation(false);
  else
    tile->set_required_for_draw(false);
  tile->set_is_occluded(tree, false);

  DCHECK_GT(current_content_to_screen_scale_, 0.f);
  float distance_to_visible =
      current_visible_rect_.ManhattanInternalDistance(tile_bounds) *
      current_content_to_screen_scale_;

  if (max_tile_priority_bin <= TilePriority::SOON &&
      (current_soon_border_rect_.Intersects(tile_bounds) ||
       current_skewport_rect_.Intersects(tile_bounds))) {
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

void PictureLayerTiling::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetInteger("num_tiles", tiles_.size());
  state->SetDouble("content_scale", contents_scale_);
  MathUtil::AddToTracedValue("visible_rect", current_visible_rect_, state);
  MathUtil::AddToTracedValue("skewport_rect", current_skewport_rect_, state);
  MathUtil::AddToTracedValue("soon_rect", current_soon_border_rect_, state);
  MathUtil::AddToTracedValue("eventually_rect", current_eventually_rect_,
                             state);
  MathUtil::AddToTracedValue("tiling_size", tiling_size(), state);
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

}  // namespace cc
