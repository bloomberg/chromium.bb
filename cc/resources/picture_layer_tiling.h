// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PICTURE_LAYER_TILING_H_
#define CC_RESOURCES_PICTURE_LAYER_TILING_H_

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/base/region.h"
#include "cc/base/tiling_data.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_bundle.h"
#include "cc/resources/tile_priority.h"
#include "ui/gfx/rect.h"

namespace cc {

class PictureLayerTiling;

class CC_EXPORT PictureLayerTilingClient {
 public:
  // Create a tile at the given content_rect (in the contents scale of the
  // tiling) This might return null if the client cannot create such a tile.
  virtual scoped_refptr<Tile> CreateTile(
    PictureLayerTiling* tiling,
    gfx::Rect content_rect) = 0;
  virtual scoped_refptr<TileBundle> CreateTileBundle(int offset_x,
                                                     int offset_y,
                                                     int width,
                                                     int height) = 0;
  virtual void UpdatePile(Tile* tile) = 0;
  virtual gfx::Size CalculateTileSize(
    gfx::Size content_bounds) const = 0;
  virtual const Region* GetInvalidation() = 0;
  virtual const PictureLayerTiling* GetTwinTiling(
      const PictureLayerTiling* tiling) const = 0;

 protected:
  virtual ~PictureLayerTilingClient() {}
};

class CC_EXPORT PictureLayerTiling {
 public:
  ~PictureLayerTiling();

  // Create a tiling with no tiles.  CreateTiles must be called to add some.
  static scoped_ptr<PictureLayerTiling> Create(
      float contents_scale,
      gfx::Size layer_bounds,
      PictureLayerTilingClient* client);
  gfx::Size layer_bounds() const { return layer_bounds_; }
  void SetLayerBounds(gfx::Size layer_bounds);
  void Invalidate(const Region& layer_region);
  void CreateMissingTilesInLiveTilesRect();

  void SetCanUseLCDText(bool can_use_lcd_text);

  void SetClient(PictureLayerTilingClient* client);
  void set_resolution(TileResolution resolution) { resolution_ = resolution; }
  TileResolution resolution() const { return resolution_; }

  gfx::Rect ContentRect() const;
  gfx::SizeF ContentSizeF() const;
  gfx::Rect live_tiles_rect() const { return live_tiles_rect_; }
  gfx::Size tile_size() const { return tiling_data_.max_texture_size(); }
  float contents_scale() const { return contents_scale_; }

  Tile* TileAt(WhichTree tree, int, int) const;

  void SetTreeForTesting(WhichTree tree) {
    current_tree_ = tree;
  }
  void CreateAllTilesForTesting() {
    current_tree_ = ACTIVE_TREE;
    SetLiveTilesRect(gfx::Rect(tiling_data_.total_size()));
    live_tiles_rect_ = gfx::Rect();
    current_tree_ = PENDING_TREE;
    SetLiveTilesRect(gfx::Rect(tiling_data_.total_size()));
  }
  void CreateTilesForTesting(WhichTree tree) {
    current_tree_ = tree;
    SetLiveTilesRect(gfx::Rect(tiling_data_.total_size()));
  }
  std::vector<Tile*> AllTilesForTesting() const {
    std::vector<Tile*> all_tiles;
    for (TileBundleMap::const_iterator it = tile_bundles_.begin();
         it != tile_bundles_.end(); ++it) {
      for (TileBundle::Iterator tile_it(it->second.get()); tile_it; ++tile_it)
        all_tiles.push_back(*tile_it);
    }
    return all_tiles;
  }
  std::vector<TileBundle*> AllTileBundlesForTesting() const {
    std::vector<TileBundle*> all_bundles;
    for (TileBundleMap::const_iterator it = tile_bundles_.begin();
         it != tile_bundles_.end(); ++it) {
      all_bundles.push_back(it->second.get());
    }
    return all_bundles;
  }
  std::vector<Tile*> TilesForTesting(WhichTree tree) const {
    std::vector<Tile*> all_tiles;
    for (TileBundleMap::const_iterator it = tile_bundles_.begin();
         it != tile_bundles_.end(); ++it) {
      for (TileBundle::Iterator tile_it(it->second.get(), tree);
           tile_it;
           ++tile_it) {
        all_tiles.push_back(*tile_it);
      }
    }
    return all_tiles;
  }

  Tile* TileAt(int i, int j) const;

  // Iterate over all tiles to fill content_rect.  Even if tiles are invalid
  // (i.e. no valid resource) this tiling should still iterate over them.
  // The union of all geometry_rect calls for each element iterated over should
  // exactly equal content_rect and no two geometry_rects should intersect.
  class CC_EXPORT CoverageIterator {
   public:
    CoverageIterator();
    CoverageIterator(const PictureLayerTiling* tiling,
        float dest_scale,
        gfx::Rect rect);
    ~CoverageIterator();

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

    TilePriority priority();
    void SetPriorityForTesting(const TilePriority& priority);

    CoverageIterator& operator++();
    operator bool() const { return tile_j_ <= bottom_; }

    int i() const { return tile_i_; }
    int j() const { return tile_j_; }

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
    WhichTree tree_;

    friend class PictureLayerTiling;
  };

  Region OpaqueRegionInContentRect(gfx::Rect content_rect) const;

  void Reset();

  void UpdateTilePriorities(
      WhichTree tree,
      gfx::Size device_viewport,
      gfx::Rect viewport_in_layer_space,
      gfx::Rect visible_layer_rect,
      gfx::Size last_layer_bounds,
      gfx::Size current_layer_bounds,
      float last_layer_contents_scale,
      float current_layer_contents_scale,
      const gfx::Transform& last_screen_transform,
      const gfx::Transform& current_screen_transform,
      double current_frame_time_in_seconds,
      size_t max_tiles_for_interest_area);

  // Copies the src_tree priority into the dst_tree priority for all tiles.
  // The src_tree priority is reset to the lowest priority possible.  This
  // also updates the pile on each tile to be the current client's pile.
  void DidBecomeActive();

  // Resets the active priority for all tiles in a tiling, when an active
  // tiling is becoming recycled. This may include some tiles which are
  // not in the the pending tiling (due to invalidations). This must
  // be called before DidBecomeActive, as it resets the active priority
  // while DidBecomeActive promotes pending priority on a similar set of tiles.
  void DidBecomeRecycled();

  void UpdateTilesToCurrentPile();

  bool NeedsUpdateForFrameAtTime(double frame_time_in_seconds) {
    return frame_time_in_seconds != last_impl_frame_time_in_seconds_;
  }

  scoped_ptr<base::Value> AsValue() const;
  size_t GPUMemoryUsageInBytes() const;

  struct RectExpansionCache {
    RectExpansionCache();

    gfx::Rect previous_start;
    gfx::Rect previous_bounds;
    gfx::Rect previous_result;
    int64 previous_target;
  };

  static
  gfx::Rect ExpandRectEquallyToAreaBoundedBy(
      gfx::Rect starting_rect,
      int64 target_area,
      gfx::Rect bounding_rect,
      RectExpansionCache* cache);

  bool has_ever_been_updated() const {
    return last_impl_frame_time_in_seconds_ != 0.0;
  }

 protected:
  friend class CoverageIterator;
  friend class TileBundle;

  typedef std::pair<int, int> TileBundleMapKey;
  typedef base::hash_map<TileBundleMapKey, scoped_refptr<TileBundle> >
      TileBundleMap;

  PictureLayerTiling(float contents_scale,
                     gfx::Size layer_bounds,
                     PictureLayerTilingClient* client);
  void SetLiveTilesRect(gfx::Rect live_tiles_rect);
  void CreateTile(WhichTree tree,
                  int i,
                  int j,
                  const PictureLayerTiling* twin_tiling);
  bool RemoveTile(WhichTree tree, int i, int j);
  void RemoveBundleContainingTileAtIfEmpty(int i, int j);
  TileBundle* TileBundleContainingTileAt(int, int) const;
  TileBundle* CreateBundleForTileAt(int,
                                    int,
                                    const PictureLayerTiling* twin_tiling);
  TileBundle* TileBundleAt(int, int) const;

  // Given properties.
  float contents_scale_;
  gfx::Size layer_bounds_;
  TileResolution resolution_;
  PictureLayerTilingClient* client_;

  // Internal data.
  TilingData tiling_data_;
  TilingData bundle_tiling_data_;
  // It is not legal to have a NULL tile bundle in the tile_bundles_ map.
  TileBundleMap tile_bundles_;
  gfx::Rect live_tiles_rect_;
  WhichTree current_tree_;

  // State saved for computing velocities based upon finite differences.
  double last_impl_frame_time_in_seconds_;

 private:
  DISALLOW_ASSIGN(PictureLayerTiling);

  RectExpansionCache expansion_cache_;
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_LAYER_TILING_H_
