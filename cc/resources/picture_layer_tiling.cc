// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture_layer_tiling.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/debug/trace_event.h"
#include "base/debug/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "cc/trees/occlusion_tracker.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/safe_integer_conversions.h"
#include "ui/gfx/size_conversions.h"

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

    // Evict a before b if their priority bins differ and a has the higher
    // priority bin.
    if (a_priority.priority_bin != b_priority.priority_bin)
      return a_priority.priority_bin > b_priority.priority_bin;

    // Or if a is not required and b is required.
    if (a->required_for_activation() != b->required_for_activation())
      return b->required_for_activation();

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

void ReleaseTile(Tile* tile, WhichTree tree) {
  // Reset priority as tile is ref-counted and might still be used
  // even though we no longer hold a reference to it here anymore.
  tile->SetPriority(tree, TilePriority());
}

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
      tiling_data_(gfx::Size(), gfx::Size(), true),
      last_impl_frame_time_in_seconds_(0.0),
      has_visible_rect_tiles_(false),
      has_skewport_rect_tiles_(false),
      has_soon_border_rect_tiles_(false),
      has_eventually_rect_tiles_(false),
      eviction_tiles_cache_valid_(false),
      eviction_cache_tree_priority_(SAME_PRIORITY_FOR_BOTH_TREES) {
  gfx::Size content_bounds =
      gfx::ToCeiledSize(gfx::ScaleSize(layer_bounds, contents_scale));
  gfx::Size tile_size = client_->CalculateTileSize(content_bounds);

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
    ReleaseTile(it->second.get(), client_->GetTree());
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
      if (!client_->GetInvalidation()->Intersects(rect)) {
        tiles_[key] = candidate_tile;
        return candidate_tile;
      }
    }
  }

  // Create a new tile because our twin didn't have a valid one.
  scoped_refptr<Tile> tile = client_->CreateTile(this, tile_rect);
  if (tile.get())
    tiles_[key] = tile;
  return tile.get();
}

void PictureLayerTiling::CreateMissingTilesInLiveTilesRect() {
  const PictureLayerTiling* twin_tiling = client_->GetTwinTiling(this);
  bool include_borders = true;
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
}

void PictureLayerTiling::UpdateTilesToCurrentPile(
    const Region& layer_invalidation,
    const gfx::Size& new_layer_bounds) {
  DCHECK(!new_layer_bounds.IsEmpty());

  gfx::Size old_layer_bounds = layer_bounds_;
  layer_bounds_ = new_layer_bounds;

  gfx::Size content_bounds =
      gfx::ToCeiledSize(gfx::ScaleSize(layer_bounds_, contents_scale_));
  gfx::Size tile_size = tiling_data_.max_texture_size();

  if (layer_bounds_ != old_layer_bounds) {
    // Drop tiles outside the new layer bounds if the layer shrank.
    SetLiveTilesRect(
        gfx::IntersectRects(live_tiles_rect_, gfx::Rect(content_bounds)));
    tiling_data_.SetTilingSize(content_bounds);
    tile_size = client_->CalculateTileSize(content_bounds);
  }

  if (tile_size != tiling_data_.max_texture_size()) {
    tiling_data_.SetMaxTextureSize(tile_size);
    // When the tile size changes, the TilingData positions no longer work
    // as valid keys to the TileMap, so just drop all tiles.
    Reset();
  } else {
    Invalidate(layer_invalidation);
  }

  PicturePileImpl* pile = client_->GetPile();
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it)
    it->second->set_picture_pile(pile);
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
      tiling_data_.ExpandRectIgnoringBordersToTileBoundsWithBorders(
          live_tiles_rect_);
  for (Region::Iterator iter(layer_region); iter.has_rect(); iter.next()) {
    gfx::Rect layer_rect = iter.rect();
    gfx::Rect content_rect =
        gfx::ScaleToEnclosingRect(layer_rect, contents_scale_);
    // Avoid needless work by not bothering to invalidate where there aren't
    // tiles.
    content_rect.Intersect(expanded_live_tiles_rect);
    if (content_rect.IsEmpty())
      continue;
    bool include_borders = true;
    for (TilingData::Iterator iter(
             &tiling_data_, content_rect, include_borders);
         iter;
         ++iter) {
      TileMapKey key(iter.index());
      TileMap::iterator find = tiles_.find(key);
      if (find == tiles_.end())
        continue;

      ReleaseTile(find->second.get(), client_->GetTree());

      tiles_.erase(find);
      new_tile_keys.push_back(key);
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

void PictureLayerTiling::Reset() {
  live_tiles_rect_ = gfx::Rect();
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it)
    ReleaseTile(it->second.get(), client_->GetTree());
  tiles_.clear();
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

void PictureLayerTiling::UpdateTilePriorities(
    WhichTree tree,
    const gfx::Rect& visible_layer_rect,
    float ideal_contents_scale,
    double current_frame_time_in_seconds,
    const OcclusionTracker<LayerImpl>* occlusion_tracker,
    const LayerImpl* render_target,
    const gfx::Transform& draw_transform) {
  if (!NeedsUpdateForFrameAtTime(current_frame_time_in_seconds)) {
    // This should never be zero for the purposes of has_ever_been_updated().
    DCHECK_NE(current_frame_time_in_seconds, 0.0);
    return;
  }

  gfx::Rect visible_rect_in_content_space =
      gfx::ScaleToEnclosingRect(visible_layer_rect, contents_scale_);

  if (tiling_size().IsEmpty()) {
    last_impl_frame_time_in_seconds_ = current_frame_time_in_seconds;
    last_visible_rect_in_content_space_ = visible_rect_in_content_space;
    return;
  }

  size_t max_tiles_for_interest_area = client_->GetMaxTilesForInterestArea();

  gfx::Size tile_size = tiling_data_.max_texture_size();
  int64 eventually_rect_area =
      max_tiles_for_interest_area * tile_size.width() * tile_size.height();

  gfx::Rect skewport = ComputeSkewport(current_frame_time_in_seconds,
                                       visible_rect_in_content_space);
  DCHECK(skewport.Contains(visible_rect_in_content_space));

  gfx::Rect eventually_rect =
      ExpandRectEquallyToAreaBoundedBy(visible_rect_in_content_space,
                                       eventually_rect_area,
                                       gfx::Rect(tiling_size()),
                                       &expansion_cache_);

  DCHECK(eventually_rect.IsEmpty() ||
         gfx::Rect(tiling_size()).Contains(eventually_rect))
      << "tiling_size: " << tiling_size().ToString()
      << " eventually_rect: " << eventually_rect.ToString();

  SetLiveTilesRect(eventually_rect);

  last_impl_frame_time_in_seconds_ = current_frame_time_in_seconds;
  last_visible_rect_in_content_space_ = visible_rect_in_content_space;

  eviction_tiles_cache_valid_ = false;

  TilePriority now_priority(resolution_, TilePriority::NOW, 0);
  float content_to_screen_scale =
      1.0f / (contents_scale_ * ideal_contents_scale);

  // Assign now priority to all visible tiles.
  bool include_borders = true;
  has_visible_rect_tiles_ = false;
  for (TilingData::Iterator iter(
           &tiling_data_, visible_rect_in_content_space, include_borders);
       iter;
       ++iter) {
    TileMap::iterator find = tiles_.find(iter.index());
    if (find == tiles_.end())
      continue;
    has_visible_rect_tiles_ = true;
    Tile* tile = find->second.get();

    tile->SetPriority(tree, now_priority);

    // Set whether tile is occluded or not.
    bool is_occluded = false;
    if (occlusion_tracker) {
      gfx::Rect tile_query_rect = ScaleToEnclosingRect(
          IntersectRects(tile->content_rect(), visible_rect_in_content_space),
          1.0f / contents_scale_);
      // TODO(vmpstr): Remove render_target and draw_transform from the
      // parameters so they can be hidden from the tiling.
      is_occluded = occlusion_tracker->Occluded(
          render_target, tile_query_rect, draw_transform);
    }
    tile->set_is_occluded(tree, is_occluded);
  }

  // Assign soon priority to skewport tiles.
  has_skewport_rect_tiles_ = false;
  for (TilingData::DifferenceIterator iter(
           &tiling_data_, skewport, visible_rect_in_content_space);
       iter;
       ++iter) {
    TileMap::iterator find = tiles_.find(iter.index());
    if (find == tiles_.end())
      continue;
    has_skewport_rect_tiles_ = true;
    Tile* tile = find->second.get();

    gfx::Rect tile_bounds =
        tiling_data_.TileBounds(iter.index_x(), iter.index_y());

    float distance_to_visible =
        visible_rect_in_content_space.ManhattanInternalDistance(tile_bounds) *
        content_to_screen_scale;

    TilePriority priority(resolution_, TilePriority::SOON, distance_to_visible);
    tile->SetPriority(tree, priority);
  }

  // Assign eventually priority to interest rect tiles.
  has_eventually_rect_tiles_ = false;
  for (TilingData::DifferenceIterator iter(
           &tiling_data_, eventually_rect, skewport);
       iter;
       ++iter) {
    TileMap::iterator find = tiles_.find(iter.index());
    if (find == tiles_.end())
      continue;
    has_eventually_rect_tiles_ = true;
    Tile* tile = find->second.get();

    gfx::Rect tile_bounds =
        tiling_data_.TileBounds(iter.index_x(), iter.index_y());

    float distance_to_visible =
        visible_rect_in_content_space.ManhattanInternalDistance(tile_bounds) *
        content_to_screen_scale;
    TilePriority priority(
        resolution_, TilePriority::EVENTUALLY, distance_to_visible);
    tile->SetPriority(tree, priority);
  }

  // Upgrade the priority on border tiles to be SOON.
  gfx::Rect soon_border_rect = visible_rect_in_content_space;
  float border = kSoonBorderDistanceInScreenPixels / content_to_screen_scale;
  soon_border_rect.Inset(-border, -border, -border, -border);
  has_soon_border_rect_tiles_ = false;
  for (TilingData::DifferenceIterator iter(
           &tiling_data_, soon_border_rect, skewport);
       iter;
       ++iter) {
    TileMap::iterator find = tiles_.find(iter.index());
    if (find == tiles_.end())
      continue;
    has_soon_border_rect_tiles_ = true;
    Tile* tile = find->second.get();

    TilePriority priority(resolution_,
                          TilePriority::SOON,
                          tile->priority(tree).distance_to_visible);
    tile->SetPriority(tree, priority);
  }

  // Update iteration rects.
  current_visible_rect_ = visible_rect_in_content_space;
  current_skewport_rect_ = skewport;
  current_soon_border_rect_ = soon_border_rect;
  current_eventually_rect_ = eventually_rect;
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
  for (TilingData::DifferenceIterator iter(&tiling_data_,
                                           live_tiles_rect_,
                                           new_live_tiles_rect);
       iter;
       ++iter) {
    TileMapKey key(iter.index());
    TileMap::iterator found = tiles_.find(key);
    // If the tile was outside of the recorded region, it won't exist even
    // though it was in the live rect.
    if (found != tiles_.end()) {
      ReleaseTile(found->second.get(), client_->GetTree());
      tiles_.erase(found);
    }
  }

  const PictureLayerTiling* twin_tiling = client_->GetTwinTiling(this);

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
}

void PictureLayerTiling::DidBecomeRecycled() {
  // DidBecomeActive below will set the active priority for tiles that are
  // still in the tree. Calling this first on an active tiling that is becoming
  // recycled takes care of tiles that are no longer in the active tree (eg.
  // due to a pending invalidation).
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    it->second->SetPriority(ACTIVE_TREE, TilePriority());
  }
}

void PictureLayerTiling::DidBecomeActive() {
  PicturePileImpl* active_pile = client_->GetPile();
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    it->second->SetPriority(ACTIVE_TREE, it->second->priority(PENDING_TREE));
    it->second->SetPriority(PENDING_TREE, TilePriority());

    // Tile holds a ref onto a picture pile. If the tile never gets invalidated
    // and recreated, then that picture pile ref could exist indefinitely.  To
    // prevent this, ask the client to update the pile to its own ref.  This
    // will cause PicturePileImpls and their clones to get deleted once the
    // corresponding PictureLayerImpl and any in flight raster jobs go out of
    // scope.
    it->second->set_picture_pile(active_pile);
  }
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
  return a == 0 ? -c / b :
     (-b + static_cast<int>(
         std::sqrt(static_cast<int64>(b) * b - 4.0 * a * c))) / (2 * a);
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

  eventually_eviction_tiles_.clear();
  soon_eviction_tiles_.clear();
  now_eviction_tiles_.clear();
  now_and_required_for_activation_eviction_tiles_.clear();

  for (TileMap::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    // TODO(vmpstr): This should update the priority if UpdateTilePriorities
    // changes not to do this.
    Tile* tile = it->second;
    const TilePriority& priority =
        tile->priority_for_tree_priority(tree_priority);
    switch (priority.priority_bin) {
      case TilePriority::EVENTUALLY:
        eventually_eviction_tiles_.push_back(tile);
        break;
      case TilePriority::SOON:
        soon_eviction_tiles_.push_back(tile);
        break;
      case TilePriority::NOW:
        if (tile->required_for_activation())
          now_and_required_for_activation_eviction_tiles_.push_back(tile);
        else
          now_eviction_tiles_.push_back(tile);
        break;
    }
  }

  // TODO(vmpstr): Do this lazily. One option is to have a "sorted" flag that
  // can be updated for each of the queues.
  TileEvictionOrder sort_order(tree_priority);
  std::sort(eventually_eviction_tiles_.begin(),
            eventually_eviction_tiles_.end(),
            sort_order);
  std::sort(
      soon_eviction_tiles_.begin(), soon_eviction_tiles_.end(), sort_order);
  std::sort(now_eviction_tiles_.begin(), now_eviction_tiles_.end(), sort_order);
  std::sort(now_and_required_for_activation_eviction_tiles_.begin(),
            now_and_required_for_activation_eviction_tiles_.end(),
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
      return &eventually_eviction_tiles_;
    case SOON:
      return &soon_eviction_tiles_;
    case NOW:
      return &now_eviction_tiles_;
    case NOW_AND_REQUIRED_FOR_ACTIVATION:
      return &now_and_required_for_activation_eviction_tiles_;
  }
  NOTREACHED();
  return &eventually_eviction_tiles_;
}

PictureLayerTiling::TilingRasterTileIterator::TilingRasterTileIterator()
    : tiling_(NULL), current_tile_(NULL) {}

PictureLayerTiling::TilingRasterTileIterator::TilingRasterTileIterator(
    PictureLayerTiling* tiling,
    WhichTree tree)
    : tiling_(tiling), phase_(VISIBLE_RECT), tree_(tree), current_tile_(NULL) {
  if (!tiling_->has_visible_rect_tiles_) {
    AdvancePhase();
    return;
  }

  visible_iterator_ = TilingData::Iterator(&tiling_->tiling_data_,
                                           tiling_->current_visible_rect_,
                                           true /* include_borders */);
  if (!visible_iterator_) {
    AdvancePhase();
    return;
  }

  current_tile_ =
      tiling_->TileAt(visible_iterator_.index_x(), visible_iterator_.index_y());
  if (!current_tile_ || !TileNeedsRaster(current_tile_))
    ++(*this);
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
