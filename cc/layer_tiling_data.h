// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CCLayerTilingData_h
#define CCLayerTilingData_h

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/hash_pair.h"
#include "cc/scoped_ptr_hash_map.h"
#include "IntRect.h"
#include "Region.h"
#include "TilingData.h"

namespace cc {

class LayerTilingData {
public:
    enum BorderTexelOption { HasBorderTexels, NoBorderTexels };

    ~LayerTilingData();

    static scoped_ptr<LayerTilingData> create(const IntSize& tileSize, BorderTexelOption);

    bool hasEmptyBounds() const { return m_tilingData.hasEmptyBounds(); }
    int numTilesX() const { return m_tilingData.numTilesX(); }
    int numTilesY() const { return m_tilingData.numTilesY(); }
    IntRect tileBounds(int i, int j) const { return m_tilingData.tileBounds(i, j); }
    IntPoint textureOffset(int xIndex, int yIndex) const { return m_tilingData.textureOffset(xIndex, yIndex); }

    // Change the tile size. This may invalidate all the existing tiles.
    void setTileSize(const IntSize&);
    IntSize tileSize() const;
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

        const IntRect& opaqueRect() const { return m_opaqueRect; }
        void setOpaqueRect(const IntRect& opaqueRect) { m_opaqueRect = opaqueRect; }
    private:
        int m_i;
        int m_j;
        IntRect m_opaqueRect;
        DISALLOW_COPY_AND_ASSIGN(Tile);
    };
    typedef std::pair<int, int> TileMapKey;
    typedef ScopedPtrHashMap<TileMapKey, Tile> TileMap;

    void addTile(scoped_ptr<Tile>, int, int);
    scoped_ptr<Tile> takeTile(int, int);
    Tile* tileAt(int, int) const;
    const TileMap& tiles() const { return m_tiles; }

    void setBounds(const IntSize&);
    IntSize bounds() const;

    void contentRectToTileIndices(const IntRect&, int &left, int &top, int &right, int &bottom) const;
    IntRect tileRect(const Tile*) const;

    Region opaqueRegionInContentRect(const IntRect&) const;

    void reset();

protected:
    LayerTilingData(const IntSize& tileSize, BorderTexelOption);

    TileMap m_tiles;
    TilingData m_tilingData;
};

}

#endif
