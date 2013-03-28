// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PICTURE_LAYER_TILING_H_
#define CC_RESOURCES_PICTURE_LAYER_TILING_H_

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/base/hash_pair.h"
#include "cc/base/region.h"
#include "cc/base/tiling_data.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "ui/gfx/rect.h"

namespace cc {

class PictureLayerTiling;

class PictureLayerTilingClient {
 public:
  // Create a tile at the given content_rect (in the contents scale of the
  // tiling) This might return null if the client cannot create such a tile.
  virtual scoped_refptr<Tile> CreateTile(
    PictureLayerTiling* tiling,
    gfx::Rect content_rect) = 0;
  virtual void UpdatePile(Tile* tile) = 0;
  virtual gfx::Size CalculateTileSize(
    gfx::Size current_tile_size,
    gfx::Size content_bounds) = 0;

 protected:
  virtual ~PictureLayerTilingClient() {}
};

class CC_EXPORT PictureLayerTiling {
 public:
  ~PictureLayerTiling();

  // Create a tiling with no tiles.  CreateTiles must be called to add some.
  static scoped_ptr<PictureLayerTiling> Create(float contents_scale);
  scoped_ptr<PictureLayerTiling> Clone() const;

  gfx::Size layer_bounds() const { return layer_bounds_; }
  void SetLayerBounds(gfx::Size layer_bounds);
  void Invalidate(const Region& layer_invalidation);

  // Add any tiles that intersect with |layer_rect|.  If any tiles already
  // exist, then this leaves them as-is.
  void CreateTilesFromLayerRect(gfx::Rect layer_rect);

  void SetClient(PictureLayerTilingClient* client);
  void set_resolution(TileResolution resolution) { resolution_ = resolution; }
  TileResolution resolution() const { return resolution_; }

  gfx::Rect ContentRect() const;
  gfx::SizeF ContentSizeF() const;
  float contents_scale() const { return contents_scale_; }

  std::vector<Tile*> AllTilesForTesting() const {
    std::vector<Tile*> all_tiles;
    for (TileMap::const_iterator it = tiles_.begin();
         it != tiles_.end(); ++it)
      all_tiles.push_back(it->second);
    return all_tiles;
  }

  static gfx::Rect ExpandRectEquallyToAreaBoundedBy(
      gfx::Rect starting_rect,
      int64 target_area,
      gfx::Rect bounding_rect);

  // Iterate over all tiles to fill content_rect.  Even if tiles are invalid
  // (i.e. no valid resource) this tiling should still iterate over them.
  // The union of all geometry_rect calls for each element iterated over should
  // exactly equal content_rect and no two geometry_rects should intersect.
  class CC_EXPORT Iterator {
   public:
    Iterator();
    Iterator(const PictureLayerTiling* tiling,
        float dest_scale,
        gfx::Rect rect);
    ~Iterator();

    // Visible rect (no borders), always in the space of content_rect,
    // regardless of the contents scale of the tiling.
    gfx::Rect geometry_rect() const;
    // Texture rect (in texels) for geometry_rect
    gfx::RectF texture_rect() const;
    gfx::Size texture_size() const;

    // Full rect (including borders) of the current tile, always in the space
    // of content_rect, regardless of the contents scale of the tiling.
    gfx::Rect full_tile_geometry_rect() const;

    Tile* operator->() const { return current_tile_; }
    Tile* operator*() const { return current_tile_; }

    Iterator& operator++();
    operator bool() const { return tile_j_ <= bottom_; }

   private:
    const PictureLayerTiling* tiling_;
    gfx::Rect dest_rect_;
    float dest_to_content_scale_;

    Tile* current_tile_;
    gfx::Rect current_geometry_rect_;
    int tile_i_;
    int tile_j_;
    int left_;
    int top_;
    int right_;
    int bottom_;

    friend class PictureLayerTiling;
  };

  Region OpaqueRegionInContentRect(const gfx::Rect&) const;

  void Reset() { return tiles_.clear(); }

  void UpdateTilePriorities(
      WhichTree tree,
      gfx::Size device_viewport,
      const gfx::RectF& viewport_in_layer_space,
      gfx::Size last_layer_bounds,
      gfx::Size current_layer_bounds,
      float last_layer_contents_scale,
      float current_layer_contents_scale,
      const gfx::Transform& last_screen_transform,
      const gfx::Transform& current_screen_transform,
      int current_source_frame_number,
      double current_frame_time,
      bool store_screen_space_quads_on_tiles,
      size_t max_tiles_for_interest_area);

  // Copies the src_tree priority into the dst_tree priority for all tiles.
  // The src_tree priority is reset to the lowest priority possible.  This
  // also updates the pile on each tile to be the current client's pile.
  void DidBecomeActive();

  scoped_ptr<base::Value> AsValue() const;

 protected:
  typedef std::pair<int, int> TileMapKey;
  typedef base::hash_map<TileMapKey, scoped_refptr<Tile> > TileMap;

  explicit PictureLayerTiling(float contents_scale);
  Tile* TileAt(int, int) const;
  void CreateTilesFromContentRect(gfx::Rect layer_rect);
  void CreateTile(int i, int j);

  PictureLayerTilingClient* client_;
  float contents_scale_;
  gfx::Size layer_bounds_;
  gfx::Rect last_prioritized_rect_;
  // It is not legal to have a NULL tile in the tiles_ map.
  TileMap tiles_;
  TilingData tiling_data_;
  TileResolution resolution_;
  int last_source_frame_number_;
  double last_impl_frame_time_;

  friend class Iterator;

 private:
  DISALLOW_ASSIGN(PictureLayerTiling);
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_LAYER_TILING_H_
