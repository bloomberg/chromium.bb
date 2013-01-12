// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PICTURE_LAYER_TILING_H_
#define CC_PICTURE_LAYER_TILING_H_

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/hash_pair.h"
#include "cc/region.h"
#include "cc/tile.h"
#include "cc/tile_priority.h"
#include "cc/tiling_data.h"
#include "ui/gfx/rect.h"

namespace cc {

class PictureLayerTiling;

class PictureLayerTilingClient {
  public:
   virtual scoped_refptr<Tile> CreateTile(PictureLayerTiling*, gfx::Rect) = 0;
};

class CC_EXPORT PictureLayerTiling {
 public:
  ~PictureLayerTiling();

  static scoped_ptr<PictureLayerTiling> Create(float contents_scale,
                                               gfx::Size tile_size);
  scoped_ptr<PictureLayerTiling> Clone() const;

  const PictureLayerTiling& operator=(const PictureLayerTiling&);

  void SetLayerBounds(gfx::Size layer_bounds);
  void Invalidate(const Region& layer_invalidation);

  void SetClient(PictureLayerTilingClient* client);
  void set_resolution(TileResolution resolution) { resolution_ = resolution; }
  TileResolution resolution() const { return resolution_; }

  gfx::Rect ContentRect() const;
  float contents_scale() const { return contents_scale_; }

  // Iterate over all tiles to fill content_rect.  Even if tiles are invalid
  // (i.e. no valid resource) this tiling should still iterate over them.
  // The union of all geometry_rect calls for each element iterated over should
  // exactly equal content_rect and no two geometry_rects should intersect.
  class CC_EXPORT Iterator {
   public:
    Iterator();
    Iterator(
        const PictureLayerTiling* tiling,
        float dest_scale,
        gfx::Rect rect);
    ~Iterator();

    // Visible rect (no borders), always in the space of content_rect,
    // regardless of the contents scale of the tiling.
    gfx::Rect geometry_rect() const;
    // Texture rect (in texels) for geometry_rect
    gfx::RectF texture_rect() const;
    gfx::Size texture_size() const;

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
      const gfx::Size& device_viewport,
      float last_contents_scale,
      float current_contents_scale,
      const gfx::Transform& last_screen_transform,
      const gfx::Transform& current_screen_transform,
      double time_delta);

 protected:
  typedef std::pair<int, int> TileMapKey;
  typedef base::hash_map<TileMapKey, scoped_refptr<Tile> > TileMap;

  PictureLayerTiling(float contents_scale, gfx::Size tileSize);
  Tile* TileAt(int, int) const;
  void CreateTile(int i, int j);

  PictureLayerTilingClient* client_;
  float contents_scale_;
  gfx::Size layer_bounds_;
  TileMap tiles_;
  TilingData tiling_data_;
  TileResolution resolution_;

  friend class Iterator;
};

}  // namespace cc

#endif  // CC_PICTURE_LAYER_TILING_H_
