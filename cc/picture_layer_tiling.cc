// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_layer_tiling.h"

#include <cmath>

#include "base/debug/trace_event.h"
#include "cc/math_util.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/safe_integer_conversions.h"
#include "ui/gfx/size_conversions.h"

namespace cc {

scoped_ptr<PictureLayerTiling> PictureLayerTiling::Create(
    float contents_scale) {
  return make_scoped_ptr(new PictureLayerTiling(contents_scale));
}

scoped_ptr<PictureLayerTiling> PictureLayerTiling::Clone() const {
  return make_scoped_ptr(new PictureLayerTiling(*this));
}

PictureLayerTiling::PictureLayerTiling(float contents_scale)
    : client_(NULL),
      contents_scale_(contents_scale),
      tiling_data_(gfx::Size(), gfx::Size(), true),
      resolution_(NON_IDEAL_RESOLUTION),
      last_source_frame_number_(0),
      last_impl_frame_time_(0) {
}

PictureLayerTiling::~PictureLayerTiling() {
}

void PictureLayerTiling::SetClient(PictureLayerTilingClient* client) {
  client_ = client;
}

gfx::Rect PictureLayerTiling::ContentRect() const {
  return gfx::Rect(tiling_data_.total_size());
}

gfx::SizeF PictureLayerTiling::ContentSizeF() const {
  return gfx::ScaleSize(layer_bounds_, contents_scale_);
}

Tile* PictureLayerTiling::TileAt(int i, int j) const {
  TileMap::const_iterator iter = tiles_.find(TileMapKey(i, j));
  if (iter == tiles_.end())
    return NULL;
  return iter->second.get();
}

void PictureLayerTiling::CreateTile(int i, int j) {
  gfx::Rect tile_rect = tiling_data_.TileBoundsWithBorder(i, j);
  tile_rect.set_size(tiling_data_.max_texture_size());
  TileMapKey key(i, j);
  DCHECK(tiles_.find(key) == tiles_.end());
  scoped_refptr<Tile> tile = client_->CreateTile(this, tile_rect);
  if (tile)
    tiles_[key] = tile;
}

Region PictureLayerTiling::OpaqueRegionInContentRect(
    const gfx::Rect& content_rect) const {
  Region opaque_region;
  // TODO(enne): implement me
  return opaque_region;
}

void PictureLayerTiling::SetLayerBounds(gfx::Size layer_bounds) {
  if (layer_bounds_ == layer_bounds)
    return;

  gfx::Size old_layer_bounds = layer_bounds_;
  layer_bounds_ = layer_bounds;
  gfx::Size old_content_bounds = tiling_data_.total_size();
  gfx::Size content_bounds =
      gfx::ToCeiledSize(gfx::ScaleSize(layer_bounds_, contents_scale_));

  tiling_data_.SetTotalSize(content_bounds);
  if (layer_bounds_.IsEmpty()) {
    tiles_.clear();
    return;
  }

  gfx::Size tile_size = client_->CalculateTileSize(
      tiling_data_.max_texture_size(),
      content_bounds);
  if (tile_size != tiling_data_.max_texture_size()) {
    tiling_data_.SetMaxTextureSize(tile_size);
    tiles_.clear();
    CreateTilesFromLayerRect(gfx::Rect(layer_bounds_));
    return;
  }

  // Any tiles outside our new bounds are invalid and should be dropped.
  if (old_content_bounds.width() > content_bounds.width() ||
      old_content_bounds.height() > content_bounds.height()) {
    int right =
        tiling_data_.TileXIndexFromSrcCoord(content_bounds.width() - 1);
    int bottom =
        tiling_data_.TileYIndexFromSrcCoord(content_bounds.height() - 1);

    std::vector<TileMapKey> invalid_tile_keys;
    for (TileMap::const_iterator it = tiles_.begin();
         it != tiles_.end(); ++it) {
      if (it->first.first > right || it->first.second > bottom)
        invalid_tile_keys.push_back(it->first);
    }
    for (size_t i = 0; i < invalid_tile_keys.size(); ++i)
      tiles_.erase(invalid_tile_keys[i]);
  }

  // Create tiles for newly exposed areas.
  Region layer_region((gfx::Rect(layer_bounds_)));
  layer_region.Subtract(gfx::Rect(old_layer_bounds));
  for (Region::Iterator iter(layer_region); iter.has_rect(); iter.next()) {
    Invalidate(iter.rect());
    CreateTilesFromLayerRect(iter.rect());
  }
}

void PictureLayerTiling::Invalidate(const Region& layer_invalidation) {
  std::vector<TileMapKey> new_tiles;

  for (Region::Iterator region_iter(layer_invalidation);
       region_iter.has_rect();
       region_iter.next()) {

    gfx::Rect layer_invalidation = region_iter.rect();
    layer_invalidation.Intersect(gfx::Rect(layer_bounds_));
    gfx::Rect rect =
        gfx::ToEnclosingRect(ScaleRect(layer_invalidation, contents_scale_));

    for (PictureLayerTiling::Iterator tile_iter(this, contents_scale_, rect,
                                                PictureLayerTiling::LayerDeviceAlignmentUnknown);
         tile_iter;
         ++tile_iter) {
      TileMapKey key(tile_iter.tile_i_, tile_iter.tile_j_);
      TileMap::iterator found = tiles_.find(key);
      if (found == tiles_.end())
        continue;

      tiles_.erase(found);
      new_tiles.push_back(key);
    }
  }

  for (size_t i = 0; i < new_tiles.size(); ++i)
    CreateTile(new_tiles[i].first, new_tiles[i].second);
}

void PictureLayerTiling::CreateTilesFromLayerRect(gfx::Rect layer_rect) {
  gfx::Rect content_rect =
      gfx::ToEnclosingRect(ScaleRect(layer_rect, contents_scale_));
  CreateTilesFromContentRect(content_rect);
}

void PictureLayerTiling::CreateTilesFromContentRect(gfx::Rect content_rect) {
  for (TilingData::Iterator iter(&tiling_data_, content_rect); iter; ++iter) {
    TileMap::iterator found =
        tiles_.find(TileMapKey(iter.index_x(), iter.index_y()));
    // Ignore any tiles that already exist.
    if (found != tiles_.end())
      continue;
    CreateTile(iter.index_x(), iter.index_y());
  }
}

PictureLayerTiling::Iterator::Iterator()
    : tiling_(NULL),
      current_tile_(NULL),
      tile_i_(0),
      tile_j_(0),
      left_(0),
      top_(0),
      right_(-1),
      bottom_(-1) {
}

PictureLayerTiling::Iterator::Iterator(const PictureLayerTiling* tiling,
                                       float dest_scale,
                                       gfx::Rect dest_rect,
                                       LayerDeviceAlignment layerDeviceAlignment)
    : tiling_(tiling),
      dest_rect_(dest_rect),
      current_tile_(NULL),
      dest_to_content_scale_(0),
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
  // This is the maximum size that the dest rect can be, given the content size.
  gfx::Size dest_content_size = gfx::ToCeiledSize(gfx::ScaleSize(
      tiling_->ContentRect().size(),
      1 / dest_to_content_scale_,
      1 / dest_to_content_scale_));

  gfx::Rect content_rect =
      gfx::ToEnclosingRect(gfx::ScaleRect(dest_rect_,
                                          dest_to_content_scale_,
                                          dest_to_content_scale_));
  // IndexFromSrcCoord clamps to valid tile ranges, so it's necessary to
  // check for non-intersection first.
  content_rect.Intersect(gfx::Rect(tiling_->tiling_data_.total_size()));
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

PictureLayerTiling::Iterator::~Iterator() {
}

PictureLayerTiling::Iterator& PictureLayerTiling::Iterator::operator++() {
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

  current_geometry_rect_ = gfx::ToEnclosingRect(
      gfx::ScaleRect(content_rect, 1 / dest_to_content_scale_,
                                   1 / dest_to_content_scale_));

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

gfx::Rect PictureLayerTiling::Iterator::geometry_rect() const {
  return current_geometry_rect_;
}

gfx::Rect PictureLayerTiling::Iterator::full_tile_geometry_rect() const {
  gfx::Rect rect = tiling_->tiling_data_.TileBoundsWithBorder(tile_i_, tile_j_);
  rect.set_size(tiling_->tiling_data_.max_texture_size());
  return rect;
}

gfx::RectF PictureLayerTiling::Iterator::texture_rect() const {
  gfx::PointF tex_origin =
      tiling_->tiling_data_.TileBoundsWithBorder(tile_i_, tile_j_).origin();

  // Convert from dest space => content space => texture space.
  gfx::RectF texture_rect(current_geometry_rect_);
  texture_rect.Scale(dest_to_content_scale_,
                     dest_to_content_scale_);
  texture_rect.Offset(-tex_origin.OffsetFromOrigin());
  texture_rect.Intersect(tiling_->ContentRect());

  return texture_rect;
}

gfx::Size PictureLayerTiling::Iterator::texture_size() const {
  return tiling_->tiling_data_.max_texture_size();
}

void PictureLayerTiling::UpdateTilePriorities(
    WhichTree tree,
    gfx::Size device_viewport,
    const gfx::RectF& viewport_in_layer_space,
    gfx::Size last_layer_bounds,
    gfx::Size current_layer_bounds,
    gfx::Size last_layer_content_bounds,
    gfx::Size current_layer_content_bounds,
    float last_layer_contents_scale,
    float current_layer_contents_scale,
    const gfx::Transform& last_screen_transform,
    const gfx::Transform& current_screen_transform,
    int current_source_frame_number,
    double current_frame_time,
    bool store_screen_space_quads_on_tiles) {
  if (ContentRect().IsEmpty())
    return;

  bool first_update_in_new_source_frame =
      current_source_frame_number != last_source_frame_number_;

  bool first_update_in_new_impl_frame =
      current_frame_time != last_impl_frame_time_;

  // In pending tree, this is always called. We update priorities:
  // - Immediately after a commit (first_update_in_new_source_frame).
  // - On animation ticks after the first frame in the tree
  //   (first_update_in_new_impl_frame).
  // In active tree, this is only called during draw. We update priorities:
  // - On draw if properties were not already computed by the pending tree
  //   and activated for the frame (first_update_in_new_impl_frame).
  if (!first_update_in_new_impl_frame && !first_update_in_new_source_frame)
    return;

  double time_delta = 0;
  if (last_impl_frame_time_ != 0 &&
      last_layer_bounds == current_layer_bounds &&
      last_layer_content_bounds == current_layer_content_bounds &&
      last_layer_contents_scale == current_layer_contents_scale) {
    time_delta = current_frame_time - last_impl_frame_time_;
  }

  gfx::Rect viewport_in_content_space =
      gfx::ToEnclosingRect(gfx::ScaleRect(viewport_in_layer_space,
                                          contents_scale_));

  gfx::Size tile_size = tiling_data_.max_texture_size();
  int64 prioritized_rect_area =
      TilePriority::kNumTilesToCoverWithInflatedViewportRectForPrioritization *
      tile_size.width() * tile_size.height();

  gfx::Rect prioritized_rect = ExpandRectEquallyToAreaBoundedBy(
      viewport_in_content_space,
      prioritized_rect_area,
      ContentRect());
  DCHECK(ContentRect().Contains(prioritized_rect));

  // Iterate through all of the tiles that were live last frame but will
  // not be live this frame, and mark them as being dead.
  for (TilingData::DifferenceIterator iter(&tiling_data_,
                                           last_prioritized_rect_,
                                           prioritized_rect);
       iter;
       ++iter) {
    TileMap::iterator find = tiles_.find(iter.index());
    if (find == tiles_.end())
      continue;

    TilePriority priority;
    DCHECK(!priority.is_live);
    Tile* tile = find->second.get();
    tile->set_priority(tree, priority);
  }
  last_prioritized_rect_ = prioritized_rect;

  gfx::Rect view_rect(device_viewport);
  float current_scale = current_layer_contents_scale / contents_scale_;
  float last_scale = last_layer_contents_scale / contents_scale_;

  // Fast path tile priority calculation when both transforms are translations.
  if (last_screen_transform.IsIdentityOrTranslation() &&
      current_screen_transform.IsIdentityOrTranslation())
  {
    gfx::Vector2dF current_offset(
        current_screen_transform.matrix().get(0, 3),
        current_screen_transform.matrix().get(1, 3));
    gfx::Vector2dF last_offset(
        last_screen_transform.matrix().get(0, 3),
        last_screen_transform.matrix().get(1, 3));

    for (TilingData::Iterator iter(&tiling_data_, prioritized_rect);
         iter; ++iter) {
      TileMap::iterator find = tiles_.find(iter.index());
      if (find == tiles_.end())
        continue;
      Tile* tile = find->second.get();

      gfx::Rect tile_bounds =
          tiling_data_.TileBounds(iter.index_x(), iter.index_y());
      gfx::RectF current_screen_rect = gfx::ScaleRect(
          tile_bounds,
          current_scale,
          current_scale) + current_offset;
      gfx::RectF last_screen_rect = gfx::ScaleRect(
          tile_bounds,
          last_scale,
          last_scale) + last_offset;

      float distance_to_visible_in_pixels =
          TilePriority::manhattanDistance(current_screen_rect, view_rect);

      float time_to_visible_in_seconds =
          TilePriority::TimeForBoundsToIntersect(
              last_screen_rect, current_screen_rect, time_delta, view_rect);
      TilePriority priority(
          resolution_,
          time_to_visible_in_seconds,
          distance_to_visible_in_pixels);
      if (store_screen_space_quads_on_tiles)
        priority.set_current_screen_quad(gfx::QuadF(current_screen_rect));
      tile->set_priority(tree, priority);
    }
  } else {
    for (TilingData::Iterator iter(&tiling_data_, prioritized_rect);
         iter; ++iter) {
      TileMap::iterator find = tiles_.find(iter.index());
      if (find == tiles_.end())
        continue;
      Tile* tile = find->second.get();

      gfx::Rect tile_bounds =
          tiling_data_.TileBounds(iter.index_x(), iter.index_y());
      gfx::RectF current_layer_content_rect = gfx::ScaleRect(
          tile_bounds,
          current_scale,
          current_scale);
      gfx::RectF current_screen_rect = MathUtil::mapClippedRect(
          current_screen_transform, current_layer_content_rect);
      gfx::RectF last_layer_content_rect = gfx::ScaleRect(
          tile_bounds,
          last_scale,
          last_scale);
      gfx::RectF last_screen_rect  = MathUtil::mapClippedRect(
          last_screen_transform, last_layer_content_rect);

      float distance_to_visible_in_pixels =
          TilePriority::manhattanDistance(current_screen_rect, view_rect);

      float time_to_visible_in_seconds =
          TilePriority::TimeForBoundsToIntersect(
              last_screen_rect, current_screen_rect, time_delta, view_rect);

      TilePriority priority(
          resolution_,
          time_to_visible_in_seconds,
          distance_to_visible_in_pixels);
      if (store_screen_space_quads_on_tiles) {
          bool clipped;
          priority.set_current_screen_quad(
            MathUtil::mapQuad(current_screen_transform,
                              gfx::QuadF(current_layer_content_rect),
                              clipped));
      }
      tile->set_priority(tree, priority);
    }
  }

  last_source_frame_number_ = current_source_frame_number;
  last_impl_frame_time_ = current_frame_time;
}

void PictureLayerTiling::DidBecomeActive() {
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    it->second->set_priority(ACTIVE_TREE, it->second->priority(PENDING_TREE));
    it->second->set_priority(PENDING_TREE, TilePriority());

    // Tile holds a ref onto a picture pile. If the tile never gets invalidated
    // and recreated, then that picture pile ref could exist indefinitely.  To
    // prevent this, ask the client to update the pile to its own ref.  This
    // will cause PicturePileImpls and their clones to get deleted once the
    // corresponding PictureLayerImpl and any in flight raster jobs go out of
    // scope.
    client_->UpdatePile(it->second);
  }
}

scoped_ptr<base::Value> PictureLayerTiling::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetInteger("num_tiles", tiles_.size());
  state->SetDouble("content_scale", contents_scale_);
  state->Set("content_bounds",
             MathUtil::asValue(ContentRect().size()).release());
  return state.PassAs<base::Value>();
}

namespace {

int ComputeOffsetToExpand4EdgesEqually(int old_width,
                                       int old_height,
                                       int64 target_area) {
  // We need to expand the rect in 4 directions, we can compute the
  // amount to expand along each axis with a quadratic equation:
  //   (old_w + add) * (old_h + add) = target_area
  //   old_w * old_h + old_w * add + add * old_h + add * add = target_area
  //   add^2 + add * (old_w + old_h) - target_area + old_w * old_h = 0
  // Therefore, we solve the quadratic equation with:
  // a = 1
  // b = old_w + old_h
  // c = -target_area + old_w * old_h
  int a = 1;
  int64 b = old_width + old_height;
  int64 c = -target_area + old_width * old_height;
  int sqrt_part = std::sqrt(b * b - 4.0 * a * c);
  int add_each_axis = (-b + sqrt_part) / 2 / a;
  return add_each_axis / 2;
}

int ComputeOffsetToExpand3EdgesEqually(int old_width,
                                       int old_height,
                                       int64 target_area,
                                       bool left_complete,
                                       bool top_complete,
                                       bool right_complete,
                                       bool bottom_complete) {
  // We need to expand the rect in three directions, so we will have to
  // expand along one axis twice as much as the other. Otherwise, this
  // is very similar to the case where we expand in all 4 directions.

  if (left_complete || right_complete) {
    // Expanding twice as much vertically as horizontally.
    //   (old_w + add) * (old_h + add*2) = target_area
    //   old_w * old_h + old_w * add*2 + add * old_h + add * add*2 = target_area
    //   (add^2)*2 + add * (old_w*2 + old_h) - target_area + old_w * old_h = 0
    // Therefore, we solve the quadratic equation with:
    // a = 2
    // b = old_w*2 + old_h
    // c = -target_area + old_w * old_h
    int a = 2;
    int64 b = old_width * 2 + old_height;
    int64 c = -target_area + old_width * old_height;
    int sqrt_part = std::sqrt(b * b - 4.0 * a * c);
    int add_each_direction = (-b + sqrt_part) / 2 / a;
    return add_each_direction;
  } else {
    // Expanding twice as much horizontally as vertically.
    //   (old_w + add*2) * (old_h + add) = target_area
    //   old_w * old_h + old_w * add + add*2 * old_h + add*2 * add = target_area
    //   (add^2)*2 + add * (old_w + old_h*2) - target_area + old_w * old_h = 0
    // Therefore, we solve the quadratic equation with:
    // a = 2
    // b = old_w + old_h*2
    // c = -target_area + old_w * old_h
    int a = 2;
    int64 b = old_width + old_height * 2;
    int64 c = -target_area + old_width * old_height;
    int sqrt_part = std::sqrt(b * b - 4.0 * a * c);
    int add_each_direction = (-b + sqrt_part) / 2 / a;
    return add_each_direction;
  }
}

int ComputeOffsetToExpand2EdgesEqually(int old_width,
                                       int old_height,
                                       int64 target_area,
                                       bool left_complete,
                                       bool top_complete,
                                       bool right_complete,
                                       bool bottom_complete) {
  // We need to expand the rect along two directions. If the two directions
  // are opposite from each other then we only need to compute a distance
  // along a single axis.
  if (left_complete && right_complete) {
    // Expanding along the vertical axis only:
    //   old_w * (old_h + add) = target_area
    //   old_w * old_h + old_w * add = target_area
    //   add_vertically = (target_area - old_w * old_h) / old_w
    int add_vertically = target_area / old_width - old_height;
    return add_vertically / 2;
  } else if (top_complete && bottom_complete) {
    // Expanding along the horizontal axis only:
    //   (old_w + add) * old_h = target_area
    //   old_w * old_h + add * old_h = target_area
    //   add_horizontally = (target_area - old_w * old_h) / old_h
    int add_horizontally = target_area / old_height - old_width;
    return add_horizontally / 2;
  } else {
    // If we need to expand along both horizontal and vertical axes, we can use
    // the same result as if we were expanding all four edges. But we apply the
    // offset computed for opposing edges to a single edge.
    int add_each_direction = ComputeOffsetToExpand4EdgesEqually(
        old_width, old_height, target_area);
    return add_each_direction * 2;
  }
}

int ComputeOffsetToExpand1Edge(int old_width,
                               int old_height,
                               int64 target_area,
                               bool left_complete,
                               bool top_complete,
                               bool right_complete,
                               bool bottom_complete) {
  // We need to expand the rect in a single direction, so we are either
  // moving just a verical edge, or just a horizontal edge.
  if (!top_complete || !bottom_complete) {
    // Moving a vertical edge:
    //   old_w * (old_h + add) = target_area
    //   old_w * old_h + old_w * add = target_area
    //   add_vertically = (target_area - old_w * old_h) / old_w
    int add_vertically = target_area / old_width - old_height;
    return add_vertically;
  } else {
    // Moving a horizontal edge:
    //   (old_w + add) * old_h = target_area
    //   old_w * old_h + add * old_h = target_area
    //   add_horizontally = (target_area - old_w * old_h) / old_h
    int add_horizontally = target_area / old_height - old_width;
    return add_horizontally;
  }
}

}  // namespace

// static
gfx::Rect PictureLayerTiling::ExpandRectEquallyToAreaBoundedBy(
    gfx::Rect starting_rect,
    int64 target_area,
    gfx::Rect bounding_rect) {

  bool left_complete = false;
  bool top_complete = false;
  bool right_complete = false;
  bool bottom_complete = false;
  int num_edges_complete = 0;

  gfx::Rect working_rect = starting_rect;
  for (int i = 0; i < 4; ++i) {
    if (num_edges_complete != i)
      continue;
    int offset_for_each_edge = 0;
    switch (num_edges_complete) {
      case 0:
        offset_for_each_edge = ComputeOffsetToExpand4EdgesEqually(
            working_rect.width(),
            working_rect.height(),
            target_area);
        break;
      case 1:
        offset_for_each_edge = ComputeOffsetToExpand3EdgesEqually(
            working_rect.width(),
            working_rect.height(),
            target_area,
            left_complete,
            top_complete,
            right_complete,
            bottom_complete);
        break;
      case 2:
        offset_for_each_edge = ComputeOffsetToExpand2EdgesEqually(
            working_rect.width(),
            working_rect.height(),
            target_area,
            left_complete,
            top_complete,
            right_complete,
            bottom_complete);
        break;
      case 3:
        offset_for_each_edge = ComputeOffsetToExpand1Edge(
            working_rect.width(),
            working_rect.height(),
            target_area,
            left_complete,
            top_complete,
            right_complete,
            bottom_complete);
    }

    working_rect.Inset((left_complete ? 0 : -offset_for_each_edge),
                       (top_complete ? 0 : -offset_for_each_edge),
                       (right_complete ? 0 : -offset_for_each_edge),
                       (bottom_complete ? 0 : -offset_for_each_edge));

    if (bounding_rect.Contains(working_rect))
      return working_rect;
    working_rect.Intersect(bounding_rect);

    if (working_rect.x() == bounding_rect.x()) left_complete = true;
    if (working_rect.y() == bounding_rect.y()) top_complete = true;
    if (working_rect.right() == bounding_rect.right()) right_complete = true;
    if (working_rect.bottom() == bounding_rect.bottom()) bottom_complete = true;

    num_edges_complete = (left_complete ? 1 : 0) +
                         (top_complete ? 1 : 0) +
                         (right_complete ? 1 : 0) +
                         (bottom_complete ? 1 : 0);
    if (num_edges_complete == 4)
      return working_rect;
  }

  NOTREACHED();
  return starting_rect;
}

}  // namespace cc
