// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CCLayerTilingData_h
#define CCLayerTilingData_h

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/hash_pair.h"
#include "cc/scoped_ptr_hash_map.h"
#include "ui/gfx/rect.h"
#include "IntRect.h"
#include "Region.h"
#include "TilingData.h"

namespace cc {

class LayerTilingData {
public:
    enum BorderTexelOption { HasBorderTexels, NoBorderTexels };

    ~LayerTilingData();

    static scoped_ptr<LayerTilingData> create(const gfx::Size& tileSize, BorderTexelOption);

    bool hasEmptyBounds() const { return m_tilingData.hasEmptyBounds(); }
    int numTilesX() const { return m_tilingData.numTilesX(); }
    int numTilesY() const { return m_tilingData.numTilesY(); }
    gfx::Rect tileBounds(int i, int j) const { return cc::IntRect(m_tilingData.tileBounds(i, j)); }
    gfx::Vector2d textureOffset(int xIndex, int yIndex) const {
      cc::IntPoint p(m_tilingData.textureOffset(xIndex, yIndex));
      return gfx::Vector2d(p.x(), p.y());
    }

    // Change the tile size. This may invalidate all the existing tiles.
    void setTileSize(const gfx::Size&);
    gfx::Size tileSize() const;
    // Change the border texel setting. This may invalidate all existing tiles.
    void setBorderTexelOption(BorderTexelOption);
    bool hasBorderTexels() const { return m_tilingData.borderTexels(); }

    bool isEmpty() const { return hasEmptyBounds() || !tiles().size(); }

    const LayerTilingData& operator=(const LayerTilingData&);

    class Tile {
    public:
        Tile() : m_i(-1), m_j(-1) { }
        virtual ~Tile() { }

        int i() const { return m_i; }
        int j() const { return m_j; }
        void moveTo(int i, int j) { m_i = i; m_j = j; }

        const gfx::Rect& opaqueRect() const { return m_opaqueRect; }
        void setOpaqueRect(const gfx::Rect& opaqueRect) { m_opaqueRect = opaqueRect; }
    private:
        int m_i;
        int m_j;
        gfx::Rect m_opaqueRect;
        DISALLOW_COPY_AND_ASSIGN(Tile);
    };
    typedef std::pair<int, int> TileMapKey;
    typedef ScopedPtrHashMap<TileMapKey, Tile> TileMap;

    void addTile(scoped_ptr<Tile>, int, int);
    scoped_ptr<Tile> takeTile(int, int);
    Tile* tileAt(int, int) const;
    const TileMap& tiles() const { return m_tiles; }

    void setBounds(const gfx::Size&);
    gfx::Size bounds() const;

    void contentRectToTileIndices(const gfx::Rect&, int &left, int &top, int &right, int &bottom) const;
    gfx::Rect tileRect(const Tile*) const;

    Region opaqueRegionInContentRect(const gfx::Rect&) const;

    void reset();

protected:
    LayerTilingData(const gfx::Size& tileSize, BorderTexelOption);

    TileMap m_tiles;
    TilingData m_tilingData;
};

}

#endif
