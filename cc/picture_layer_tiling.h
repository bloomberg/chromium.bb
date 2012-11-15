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

  static scoped_ptr<PictureLayerTiling> Create(gfx::Size tile_size);
  scoped_ptr<PictureLayerTiling> Clone() const;

  const PictureLayerTiling& operator=(const PictureLayerTiling&);

  void SetBounds(gfx::Size);
  gfx::Size bounds() const { return tiling_data_.total_size(); }

  void create_tiles(gfx::Rect);
  void set_client(PictureLayerTilingClient* client);

  class Iterator {
   public:
    Iterator(PictureLayerTiling* tiling, gfx::Rect content_rect);
    ~Iterator();

    // Visible rect (no borders)
    gfx::Rect geometry_rect() const;
    // Full tile rect (not clipped, with borders)
    gfx::Rect full_tile_rect() const;
    // Texture rect (in texels) for geometry_rect
    gfx::Rect texture_rect() const;
    gfx::Rect opaque_rect() const;
    gfx::Size texture_size() const;

    Tile* operator->() const { return current_tile_; }
    Tile* operator*() const { return current_tile_; }

    Iterator& operator++();
    bool operator==(const Iterator& other) const;
    bool operator!=(const Iterator& other) const;
    operator bool() const { return current_tile_; }

   private:
    PictureLayerTiling* tiling_;
    Tile* current_tile_;
    gfx::Rect content_rect_;
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

 protected:
  typedef std::pair<int, int> TileMapKey;
  typedef base::hash_map<TileMapKey, scoped_refptr<Tile> > TileMap;

  PictureLayerTiling(gfx::Size tileSize);
  Tile* TileAt(int, int) const;

  PictureLayerTilingClient* client_;
  TileMap tiles_;
  TilingData tiling_data_;

  friend class Iterator;
};

}  // namespace cc

#endif  // CC_PICTURE_LAYER_TILING_H_
